// SPDX-License-Identifier: GPL-2.0+
/*
 * Support for Atmel/Microchip USB PHY's.
 *
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries
 *
 * Author: Sergiu Moga <sergiu.moga@microchip.com>
 */

#include <clk.h>
#include <dm.h>
#include <generic-phy.h>
#include <syscon.h>
#include <regmap.h>
#include <mach/sama7-sfr.h>

struct sama7_usb_phy {
	struct clk *uclk;
	struct regmap *sfr;
	int port;
};

int sama7_usb_phy_init(struct phy *phy)
{
	struct sama7_usb_phy *sama7_phy = dev_get_priv(phy->dev);
	int port = sama7_phy->port;

	regmap_update_bits(sama7_phy->sfr, SAMA7_SFR_UTMI0R(port),
			   SAMA7_SFR_UTMI_RX_TX_PREEM_AMP_TUNE_1X,
			   SAMA7_SFR_UTMI_RX_TX_PREEM_AMP_TUNE_1X);

	regmap_update_bits(sama7_phy->sfr, SAMA7_SFR_UTMI0R(port),
			   SAMA7_SFR_UTMI_RX_VBUS,
			   SAMA7_SFR_UTMI_RX_VBUS);

	return 0;
}

int sama7_phy_power_on(struct phy *phy)
{
	struct sama7_usb_phy *sama7_phy = dev_get_priv(phy->dev);

	clk_prepare_enable(sama7_phy->uclk);

	return 0;
}

int sama7_phy_power_off(struct phy *phy)
{
	struct sama7_usb_phy *sama7_phy = dev_get_priv(phy->dev);

	clk_disable_unprepare(sama7_phy->uclk);

	return 0;
}

int sama7_usb_phy_probe(struct udevice *dev)
{
	struct sama7_usb_phy *sama7_phy = dev_get_priv(dev);

	sama7_phy->uclk = devm_clk_get(dev, "utmi_clk");
	if (IS_ERR(sama7_phy->uclk))
		return PTR_ERR(sama7_phy->uclk);

	sama7_phy->sfr = syscon_regmap_lookup_by_phandle(dev, "sfr-phandle");
	if (IS_ERR(sama7_phy->sfr)) {
		sama7_phy->sfr = NULL;
		return PTR_ERR(sama7_phy->sfr);
	}

	return dev_read_u32(dev, "reg", &sama7_phy->port);
}

static const struct phy_ops sama7_usb_phy_ops = {
	.init = sama7_usb_phy_init,
	.power_on = sama7_phy_power_on,
	.power_off = sama7_phy_power_off,
};

static const struct udevice_id sama7_usb_phy_of_match[] = {
	{ .compatible = "microchip,sama7g5-usb-phy", },
	{ },
};

U_BOOT_DRIVER(sama7_usb_phy_driver) = {
	.name = "sama7-usb-phy",
	.id = UCLASS_PHY,
	.of_match = sama7_usb_phy_of_match,
	.ops = &sama7_usb_phy_ops,
	.probe = sama7_usb_phy_probe,
	.priv_auto = sizeof(struct sama7_usb_phy),
};
