// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * PLLDIV clock support for AT91 architectures.
 *
 * Copyright (C) 2020 Microchip Technology Inc. and its subsidiaries
 *
 * Author: Claudiu Beznea <claudiu.beznea@microchip.com>
 *
 * Based on drivers/clk/at91/clk-plldiv.c
 */

#include <linux/clk-provider.h>
#include <linux/clk/at91_pmc.h>
#include <dm.h>

#include "pmc.h"

#define UBOOT_DM_CLK_AT91_PLLDIV	"at91-div-clk"

struct clk_div {
	void __iomem *base;
	struct clk clk;
	u32 bitmask;
};

#define to_clk_div(clk) container_of(clk, struct clk_div, clk)

static ulong clk_div_get_rate(struct clk *clk)
{
	struct clk_div *div = to_clk_div(clk);
	ulong parent_rate = clk_get_parent_rate(clk);
	unsigned int mckr;

	pmc_read(div->base, AT91_PMC_MCKR, &mckr);

	if (mckr & div->bitmask)
		return parent_rate / 2;

	return parent_rate;
}

static ulong clk_div_set_rate(struct clk *clk, unsigned long rate)
{
	struct clk_div *div = to_clk_div(clk);
	ulong parent_rate = clk_get_parent_rate(clk);

	if ((parent_rate != rate) && (parent_rate / 2 != rate))
		return -EINVAL;

	pmc_update_bits(div->base, AT91_PMC_MCKR, div->bitmask,
			parent_rate != rate ? div->bitmask : 0);

	return 0;
}

static const struct clk_ops div_ops = {
	.set_rate = clk_div_set_rate,
	.get_rate = clk_div_get_rate,
};

struct clk *at91_clk_register_div(void __iomem *base, const char *name,
				  const char *parent_name, u32 bitmask)
{
	struct clk_div *div;
	struct clk *clk;
	int ret;

	div = kzalloc(sizeof(*div), GFP_KERNEL);
	if (!div)
		return ERR_PTR(-ENOMEM);

	div->base = base;
	div->bitmask = bitmask;

	clk = &div->clk;
	clk->flags = CLK_GET_RATE_NOCACHE;
	ret = clk_register(clk, UBOOT_DM_CLK_AT91_PLLDIV, name, parent_name);
	if (ret) {
		kfree(div);
		clk = ERR_PTR(ret);
	}

	return clk;
}

U_BOOT_DRIVER(at91_div_clk) = {
	.name = UBOOT_DM_CLK_AT91_PLLDIV,
	.id = UCLASS_CLK,
	.ops = &div_ops,
	.flags = DM_FLAG_PRE_RELOC,
};

