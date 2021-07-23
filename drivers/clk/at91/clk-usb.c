// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * SAM9X5 USB clock support for AT91 architectures.
 *
 * Copyright (C) 2020 Microchip Technology Inc. and its subsidiaries
 *
 * Author: Claudiu Beznea <claudiu.beznea@microchip.com>
 *
 * Based on drivers/clk/at91/clk-usb.c from Linux.
 */

#include <linux/clk-provider.h>
#include <linux/clk/at91_pmc.h>
#include <dm.h>

#include "pmc.h"

#define UBOOT_DM_CLK_AT91_SAM9X5_USB	"at91-sam9x5-usb-clk"
#define UBOOT_DM_CLK_AT91_SAM9N12_USB	"at91-sam9n12-usb-clk"

#define SAM9X5_USB_DIV_SHIFT	8
#define SAM9X5_USB_MAX_DIV	0xf

#define RM9200_USB_DIV_SHIFT	28
#define RM9200_USB_DIV_TAB_SIZE	4

#define SAM9X5_USBS_MASK	GENMASK(0, 0)
#define SAM9X60_USBS_MASK	GENMASK(1, 0)

struct at91sam9x5_clk_usb {
	void __iomem *base;
	u32 *clk_mux_table;
	struct clk clk;
	u32 usbs_mask;
	u8 num_parents;
};

#define to_at91sam9x5_clk_usb(clk) \
	container_of(clk, struct at91sam9x5_clk_usb, clk)

static ulong at91sam9x5_clk_usb_get_rate(struct clk *clk)
{
	struct at91sam9x5_clk_usb *usb = to_at91sam9x5_clk_usb(clk);
	ulong parent_rate = clk_get_parent_rate(clk);
	u32 tmp;

	if (!parent_rate)
		return 0;

	pmc_read(usb->base, AT91_PMC_USB, &tmp);
	tmp = (tmp & AT91_PMC_OHCIUSBDIV) >> SAM9X5_USB_DIV_SHIFT;

	return DIV_ROUND_CLOSEST(parent_rate, tmp + 1);
}

static int at91sam9x5_clk_usb_set_parent(struct clk *clk, struct clk *parent)
{
	struct at91sam9x5_clk_usb *usb = to_at91sam9x5_clk_usb(clk);
	int index;

	index = at91_clk_mux_val_to_index(usb->clk_mux_table, usb->num_parents,
					  parent->id);
	if (index < 0)
		return index;

	pmc_update_bits(usb->base, AT91_PMC_USB, usb->usbs_mask, index);

	return 0;
}

static ulong at91sam9x5_clk_usb_set_rate(struct clk *clk, ulong rate)
{
	struct at91sam9x5_clk_usb *usb = to_at91sam9x5_clk_usb(clk);
	ulong parent_rate = clk_get_parent_rate(clk);
	unsigned long div;

	if (!rate || !parent_rate)
		return 0;

	div = DIV_ROUND_CLOSEST(parent_rate, rate);
	if (div > SAM9X5_USB_MAX_DIV + 1 || !div)
		return 0;

	pmc_update_bits(usb->base, AT91_PMC_USB, AT91_PMC_OHCIUSBDIV,
			(div - 1) << SAM9X5_USB_DIV_SHIFT);

	return 0;
}

static const struct clk_ops at91sam9x5_usb_ops = {
	.set_parent = at91sam9x5_clk_usb_set_parent,
	.set_rate = at91sam9x5_clk_usb_set_rate,
	.get_rate = at91sam9x5_clk_usb_get_rate,
};

static struct clk *
_at91sam9x5_clk_register_usb(void __iomem *base, const char *name,
			     const char **parent_names, u8 num_parents,
			     u32 usbs_mask, u32 *clk_mux_table)
{
	struct at91sam9x5_clk_usb *usb;
	struct clk *clk;
	u32 tmp;
	int ret;

	if (!base || !name || !parent_names || !num_parents || !clk_mux_table)
		return ERR_PTR(-EINVAL);

	usb = kzalloc(sizeof(*usb), GFP_KERNEL);
	if (!usb)
		return ERR_PTR(-ENOMEM);

	usb->base = base;
	usb->usbs_mask = usbs_mask;
	usb->num_parents = num_parents;
	usb->clk_mux_table = clk_mux_table;

	clk = &usb->clk;
	clk->flags = CLK_GET_RATE_NOCACHE;
	pmc_read(usb->base, AT91_PMC_USB, &tmp);
	tmp &= usbs_mask;
	ret = clk_register(clk, UBOOT_DM_CLK_AT91_SAM9X5_USB, name,
			   parent_names[tmp]);
	if (ret) {
		kfree(usb);
		clk = ERR_PTR(ret);
	}

	return clk;
}

struct clk *
at91sam9x5_clk_register_usb(void __iomem *base, const char *name,
			    const char **parent_names, u8 num_parents,
			    u32 *clk_mux_table)
{
	return _at91sam9x5_clk_register_usb(base, name, parent_names,
					    num_parents, SAM9X5_USBS_MASK,
					    clk_mux_table);
}

struct clk *
sam9x60_clk_register_usb(void __iomem *base, const char *name,
			 const char **parent_names, u8 num_parents)
{
	return _at91sam9x5_clk_register_usb(base, name, parent_names,
					    num_parents, SAM9X60_USBS_MASK,
					    NULL);
}

U_BOOT_DRIVER(at91_sam9x5_usb_clk) = {
	.name = UBOOT_DM_CLK_AT91_SAM9X5_USB,
	.id = UCLASS_CLK,
	.ops = &at91sam9x5_usb_ops,
	.flags = DM_FLAG_PRE_RELOC,
};

static int at91sam9n12_clk_usb_enable(struct clk *clk)
{
	struct at91sam9x5_clk_usb *usb = to_at91sam9x5_clk_usb(clk);

	pmc_update_bits(usb->base, AT91_PMC_USB, AT91_PMC_USBS,
			AT91_PMC_USBS);

	return 0;
}

static int at91sam9n12_clk_usb_disable(struct clk *clk)
{
	struct at91sam9x5_clk_usb *usb = to_at91sam9x5_clk_usb(clk);

	pmc_update_bits(usb->base, AT91_PMC_USB, AT91_PMC_USBS, 0);

	return 0;
}

static const struct clk_ops at91sam9n12_usb_ops = {
	.enable = at91sam9n12_clk_usb_enable,
	.disable = at91sam9n12_clk_usb_disable,
	.set_rate = at91sam9x5_clk_usb_set_rate,
	.get_rate = at91sam9x5_clk_usb_get_rate,
};

struct clk *
at91sam9n12_clk_register_usb(void __iomem *base, const char *name,
			     const char *parent_name)
{
	struct at91sam9x5_clk_usb *usb;
	struct clk *clk;
	int ret;

	if (!base || !name || !parent_name)
		return ERR_PTR(-EINVAL);

	usb = kzalloc(sizeof(*usb), GFP_KERNEL);
	if (!usb)
		return ERR_PTR(-ENOMEM);

	usb->base = base;
	clk = &usb->clk;
	clk->flags = CLK_GET_RATE_NOCACHE;
	ret = clk_register(clk, UBOOT_DM_CLK_AT91_SAM9N12_USB, name,
			   parent_name);
	if (ret) {
		kfree(usb);
		clk = ERR_PTR(ret);
	}

	return clk;
}

U_BOOT_DRIVER(at91_sam9n12_usb_clk) = {
	.name = UBOOT_DM_CLK_AT91_SAM9N12_USB,
	.id = UCLASS_CLK,
	.ops = &at91sam9n12_usb_ops,
	.flags = DM_FLAG_PRE_RELOC,
};

