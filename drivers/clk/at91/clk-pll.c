// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  PLL clock support for AT91 architectures.
 *
 *  Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries
 *
 *  Author: Claudiu Beznea <claudiu.beznea@microchip.com>
 *
 *  Based on drivers/clk/at91/clk-pll.c from Linux.
 */

#include <linux/clk-provider.h>
#include <linux/clk/at91_pmc.h>
#include <clk-uclass.h>
#include <div64.h>
#include <dm.h>

#include "pmc.h"

#define UBOOT_DM_CLK_AT91_PLL	"at91-pll-clk"

#define PLL_STATUS_MASK(id)	(1 << (1 + (id)))
#define PLL_REG(id)		(AT91_CKGR_PLLAR + ((id) * 4))
#define PLL_DIV_MASK		0xff
#define PLL_DIV_MAX(layout)	(layout)->div_mask
#define PLL_DIV(reg, layout)	((reg) & (layout)->div_mask)
#define PLL_MUL(reg, layout)	(((reg) >> (layout)->mul_shift) & \
				 (layout)->mul_mask)
#define PLL_MUL_MIN		2
#define PLL_MUL_MASK(layout)	((layout)->mul_mask)
#define PLL_MUL_MAX(layout)	(PLL_MUL_MASK(layout) + 1)
#define PLL_ICPR_SHIFT(id)	((id) * 16)
#define PLL_ICPR_MASK(id)	(0xffff << PLL_ICPR_SHIFT(id))
#define PLL_MAX_COUNT		0x3f
#define PLL_COUNT_SHIFT		8
#define PLL_OUT_SHIFT		14
#define PLL_MAX_ID		1

struct clk_pll {
	void __iomem *base;
	const struct clk_pll_layout *layout;
	const struct clk_pll_characteristics *characteristics;
	struct clk clk;
	u8 id;
};

#define to_clk_pll(hw) container_of(hw, struct clk_pll, hw)

static inline bool clk_pll_ready(void __iomem *base, int id)
{
	unsigned int status;

	pmc_read(base, AT91_PMC_SR, &status);

	return !!(status & PLL_STATUS_MASK(id));
}

static int clk_pll_enable(struct clk *clk)
{
	struct clk_pll *pll = to_clk_pll(clk);
	void __iomem *base = pll->base;
	const struct clk_pll_layout *layout = pll->layout;
	const struct clk_pll_characteristics *characteristics =
							pll->characteristics;
	u8 id = pll->id;
	u8 out = 0;
	unsigned int tmp;

	pmc_read(base, AT91_PMC_SR, &tmp);
	if (tmp & PLL_STATUS_MASK(id))
		return 0;

	if (characteristics->out)
		out = characteristics->out[id];

	if (characteristics->icpll)
		pmc_update_bits(base, AT91_PMC_PLLICPR, PLL_ICPR_MASK(id),
			characteristics->icpll[id] << PLL_ICPR_SHIFT(id));

	pmc_update_bits(base, PLL_REG(id),
			(layout->div_mask << layout->div_shift) |
			(PLL_MAX_COUNT << PLL_COUNT_SHIFT) |
			(0x3 << PLL_OUT_SHIFT),
			1 << layout->div_shift | /* div=1 means PLL enable. */
			(PLL_MAX_COUNT << PLL_COUNT_SHIFT) |
			(out << PLL_OUT_SHIFT));

	while (!clk_pll_ready(base, id)) {
		debug("waiting for pll %u\n", id);
		cpu_relax();
	}

	return 0;
}

static int clk_pll_disable(struct clk *clk)
{
	struct clk_pll *pll = to_clk_pll(clk);

	pmc_update_bits(pll->base, PLL_REG(pll->id),
			pll->layout->div_mask << pll->layout->div_shift, 0);

	return 0;
}

static long clk_pll_get_best_div_mul(struct clk_pll *pll, unsigned long rate,
				     unsigned long parent_rate,
				     u32 *div, u32 *mul)
{
	const struct clk_pll_layout *layout = pll->layout;
	const struct clk_pll_characteristics *characteristics =
							pll->characteristics;
	unsigned long bestremainder = ULONG_MAX;
	unsigned long maxdiv, mindiv, tmpdiv;
	long bestrate = -ERANGE;
	unsigned long bestdiv;
	unsigned long bestmul;
	int i = 0;

	/* Check if parent_rate is a valid input rate */
	if (parent_rate < characteristics->input.min)
		return -ERANGE;

	/*
	 * Calculate minimum divider based on the minimum multiplier, the
	 * parent_rate and the requested rate.
	 * Should always be 2 according to the input and output characteristics
	 * of the PLL blocks.
	 */
	mindiv = (parent_rate * PLL_MUL_MIN) / rate;
	if (!mindiv)
		mindiv = 1;

	if (parent_rate > characteristics->input.max) {
		tmpdiv = DIV_ROUND_UP(parent_rate, characteristics->input.max);
		if (tmpdiv > PLL_DIV_MAX(layout))
			return -ERANGE;

		if (tmpdiv > mindiv)
			mindiv = tmpdiv;
	}

	/*
	 * Calculate the maximum divider which is limited by PLL register
	 * layout (limited by the MUL or DIV field size).
	 */
	maxdiv = DIV_ROUND_UP(parent_rate * PLL_MUL_MAX(layout), rate);
	if (maxdiv > PLL_DIV_MAX(layout))
		maxdiv = PLL_DIV_MAX(layout);

	/*
	 * Iterate over the acceptable divider values to find the best
	 * divider/multiplier pair (the one that generates the closest
	 * rate to the requested one).
	 */
	for (tmpdiv = mindiv; tmpdiv <= maxdiv; tmpdiv++) {
		unsigned long remainder;
		unsigned long tmprate;
		unsigned long tmpmul;

		/*
		 * Calculate the multiplier associated with the current
		 * divider that provide the closest rate to the requested one.
		 */
		tmpmul = DIV_ROUND_CLOSEST(rate, parent_rate / tmpdiv);
		tmprate = (parent_rate / tmpdiv) * tmpmul;
		if (tmprate > rate)
			remainder = tmprate - rate;
		else
			remainder = rate - tmprate;

		/*
		 * Compare the remainder with the best remainder found until
		 * now and elect a new best multiplier/divider pair if the
		 * current remainder is smaller than the best one.
		 */
		if (remainder < bestremainder) {
			bestremainder = remainder;
			bestdiv = tmpdiv;
			bestmul = tmpmul;
			bestrate = tmprate;
		}

		/*
		 * We've found a perfect match!
		 * Stop searching now and use this multiplier/divider pair.
		 */
		if (!remainder)
			break;
	}

	/* We haven't found any multiplier/divider pair => return -ERANGE */
	if (bestrate < 0)
		return bestrate;

	/* Check if bestrate is a valid output rate  */
	for (i = 0; i < characteristics->num_output; i++) {
		if (bestrate >= characteristics->output[i].min &&
		    bestrate <= characteristics->output[i].max)
			break;
	}

	if (i >= characteristics->num_output)
		return -ERANGE;

	if (div)
		*div = bestdiv;
	if (mul)
		*mul = bestmul - 1;

	return bestrate;
}

static ulong clk_pll_set_rate(struct clk *clk, unsigned long rate)
{
	struct clk_pll *pll = to_clk_pll(clk);
	const struct clk_pll_layout *layout = pll->layout;
	ulong parent_rate = clk_get_parent_rate(clk);

	long ret;
	u32 div;
	u32 mul;

	if (clk_pll_ready(pll->base, pll->id))
		return -EOPNOTSUPP;

	ret = clk_pll_get_best_div_mul(pll, rate, parent_rate,
				       &div, &mul);
	if (ret < 0)
		return ret;

	pmc_update_bits(pll->base, PLL_REG(pll->id),
			layout->div_mask << layout->div_shift |
			layout->mul_mask << layout->mul_shift,
			div << layout->div_shift | mul << layout->mul_shift);

	return 0;
}

static ulong clk_pll_get_rate(struct clk *clk)
{
	struct clk_pll *pll = to_clk_pll(clk);
	void __iomem *base = pll->base;
	ulong parent_rate = clk_get_parent_rate(clk);
	u32 mul, div, val;

	if (!parent_rate)
		return 0;

	pmc_read(base, PLL_REG(pll->id), &val);
	mul = (val >> pll->layout->mul_shift) & pll->layout->mul_mask;
	div = (val >> pll->layout->div_shift) & pll->layout->div_mask;

	return DIV_ROUND_CLOSEST_ULL(parent_rate * (mul + 1), div);
}

static const struct clk_ops pll_ops = {
	.enable = clk_pll_enable,
	.disable = clk_pll_disable,
	.set_rate = clk_pll_set_rate,
	.get_rate = clk_pll_get_rate,
};

struct clk *
at91_clk_register_pll(void __iomem *base, const char *name,
		      const char *parent_name, u8 id,
		      const struct clk_pll_layout *layout,
		      const struct clk_pll_characteristics *characteristics)
{
	struct clk_pll *pll;
	struct clk *clk;
	int ret;

	if (!base || !name || !parent_name || !layout || !characteristics ||
	    id > PLL_MAX_ID)
		return ERR_PTR(-EINVAL);

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return ERR_PTR(-ENOMEM);

	pll->id = id;
	pll->layout = layout;
	pll->characteristics = characteristics;
	pll->base = base;

	clk = &pll->clk;
	clk->flags = CLK_IS_CRITICAL;
	ret = clk_register(clk, UBOOT_DM_CLK_AT91_PLL, name, parent_name);
	if (ret) {
		kfree(pll);
		clk = ERR_PTR(ret);
	}

	return clk;
}

U_BOOT_DRIVER(at91_pll_clk) = {
	.name = UBOOT_DM_CLK_AT91_PLL,
	.id = UCLASS_CLK,
	.ops = &pll_ops,
	.flags = DM_FLAG_PRE_RELOC,
};

const struct clk_pll_layout at91rm9200_pll_layout = {
	.mul_shift = 16,
	.mul_mask = 0x7FF,
	.div_mask = 0xFF,
};

const struct clk_pll_layout at91sam9g45_pll_layout = {
	.mul_shift = 16,
	.mul_mask = 0xFF,
	.div_mask = 0xFF,
};

const struct clk_pll_layout at91sam9g20_pllb_layout = {
	.mul_shift = 16,
	.mul_mask = 0x3F,
	.div_mask = 0xFF,
};

const struct clk_pll_layout sama5d3_pll_layout = {
	.mul_shift = 18,
	.mul_mask = 0x7F,
	.div_mask = 0x1,
};
