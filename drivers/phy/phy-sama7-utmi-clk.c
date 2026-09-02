// SPDX-License-Identifier: GPL-2.0+
/*
 * Support for Atmel/Microchip USB PHY's.
 *
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries
 *
 * Author: Sergiu Moga <sergiu.moga@microchip.com>
 */

#include <dm.h>
#include <linux/clk-provider.h>
#include <syscon.h>
#include <regmap.h>
#include <mach/sama7-sfr.h>
#include <reset.h>
#include <dt-bindings/clk/at91.h>

struct sama7_utmi_clk {
	struct clk		uclk;
	struct regmap		*regmap_sfr;
	struct reset_ctl	*reset;
	u8 id;
};

#define to_sama7_utmi_clk(_c) container_of(_c, struct sama7_utmi_clk, uclk)

#define UBOOT_DM_CLK_MICROCHIP_SAMA7G5_UTMI "sama7-utmi-clk"
#define UBOOT_DM_MICROCHIP_SAMA7G5_UTMI "sama7-utmi"

#define AT91_TO_CLK_ID(_t, _i)		(((_t) << 8) | ((_i) & 0xff))

/*
 * UTMI clock description
 * @n:	clock name
 * @p:	clock parent name
 * @id: clock id in RSTC_GRSTR
 */
static struct {
	const char *n;
	const char *p;
	u8 id;
} sama7_utmick[] = {
	{ .n = "utmi1",	.p = "utmick", .id = 0, },
	{ .n = "utmi2",	.p = "utmi1",  .id = 1, },
	{ .n = "utmi3",	.p = "utmi1",  .id = 2, },
};

static int sama7_utmi_clk_enable(struct clk *clk)
{
	int ret;

	struct sama7_utmi_clk *utmi = to_sama7_utmi_clk(clk);
	u8 id = utmi->id;

	ret = reset_assert(utmi->reset);
	if (ret)
		return ret;

	ret = regmap_update_bits(utmi->regmap_sfr, SAMA7_SFR_UTMI0R(id),
				 SAMA7_SFR_UTMI_COMMONON, 0);
	if (ret < 0)
		return ret;

	ret = reset_deassert(utmi->reset);
	if (ret)
		return ret;

	/* Datasheet states a minimum of 45 us before any USB operation */
	udelay(50);

	return 0;
}

static int sama7_utmi_clk_disable(struct clk *clk)
{
	int ret;
	struct sama7_utmi_clk *utmi = to_sama7_utmi_clk(clk);
	u8 id = utmi->id;

	ret = reset_assert(utmi->reset);
	if (ret)
		return ret;

	regmap_update_bits(utmi->regmap_sfr, SAMA7_SFR_UTMI0R(id),
			   SAMA7_SFR_UTMI_COMMONON, SAMA7_SFR_UTMI_COMMONON);

	return 0;
}

static ulong sama7_utmi_clk_get_rate(struct clk *clk)
{
	/* Return utmick's rate: 480MHz */
	return clk_get_parent_rate(clk);
}

static const struct clk_ops sama7_utmi_clk_ops = {
	.enable = sama7_utmi_clk_enable,
	.disable = sama7_utmi_clk_disable,
	.get_rate = sama7_utmi_clk_get_rate,
};

static struct clk*
sama7_utmi_clk_register(struct regmap *regmap_sfr, struct reset_ctl *reset,
			const char *name, const char *parent_name, u8 id)
{
	struct clk *clk;
	struct sama7_utmi_clk *utmi_clk;
	int ret;

	if (!regmap_sfr || !reset || !name || !parent_name)
		return ERR_PTR(-EINVAL);

	utmi_clk = kzalloc(sizeof(*utmi_clk), GFP_KERNEL);
	if (!utmi_clk)
		return ERR_PTR(-ENOMEM);

	utmi_clk->reset = reset;
	utmi_clk->regmap_sfr = regmap_sfr;
	utmi_clk->id = id;

	clk = &utmi_clk->uclk;
	ret = clk_register(clk, UBOOT_DM_CLK_MICROCHIP_SAMA7G5_UTMI,
			   name, parent_name);
	if (ret) {
		kfree(utmi_clk);
		clk = ERR_PTR(ret);
	}

	clk_dm(AT91_TO_CLK_ID(USB_UTMI, utmi_clk->id), clk);

	return clk;
}

static int sama7_utmi_probe(struct udevice *dev)
{
	struct clk *utmi_parent_clk, *utmi_clk;
	struct regmap *regmap_sfr;
	struct reset_ctl *phy_reset;
	int i;
	char name[16];

	utmi_parent_clk = devm_clk_get(dev, "utmi_clk");
	if (IS_ERR(utmi_parent_clk))
		return PTR_ERR(utmi_parent_clk);

	regmap_sfr = syscon_regmap_lookup_by_phandle(dev, "sfr-phandle");
	if (IS_ERR(regmap_sfr))
		return PTR_ERR(regmap_sfr);

	for (i = 0; i < ARRAY_SIZE(sama7_utmick); i++) {
		snprintf(name, sizeof(name), "usb%d_reset", i);
		phy_reset = devm_reset_control_get(dev, name);
		if (IS_ERR(phy_reset))
			return PTR_ERR(phy_reset);

		utmi_clk = sama7_utmi_clk_register(regmap_sfr, phy_reset,
						   sama7_utmick[i].n,
						   sama7_utmick[i].p,
						   sama7_utmick[i].id);
		if (IS_ERR(utmi_clk))
			return PTR_ERR(utmi_clk);
	}

	return 0;
};

static const struct udevice_id sama7_utmi_clk_dt_ids[] = {
	{ .compatible = "microchip,sama7g5-utmi-clk", },
	{ /* sentinel */},
};

static int utmi_clk_of_xlate(struct clk *clk, struct ofnode_phandle_args *args)
{
	if (args->args_count != 1) {
		debug("UTMI: clk: Invalid args_count: %d\n", args->args_count);
		return -EINVAL;
	}

	clk->id = AT91_TO_CLK_ID(USB_UTMI, args->args[0]);

	return 0;
}

static const struct clk_ops sama7_utmi_ops = {
	.of_xlate	= utmi_clk_of_xlate,
	.enable		= ccf_clk_enable,
	.disable	= ccf_clk_disable,
};

U_BOOT_DRIVER(microhip_sama7g5_utmi_clk) = {
	.name = UBOOT_DM_CLK_MICROCHIP_SAMA7G5_UTMI,
	.id = UCLASS_CLK,
	.ops = &sama7_utmi_clk_ops,
};

U_BOOT_DRIVER(microhip_sama7g5_utmi) = {
	.name = UBOOT_DM_MICROCHIP_SAMA7G5_UTMI,
	.of_match = sama7_utmi_clk_dt_ids,
	.id = UCLASS_CLK,
	.ops = &sama7_utmi_ops,
	.probe = sama7_utmi_probe,
};
