// SPDX-License-Identifier: GPL-2.0+
/*
 * SMSC PHY drivers
 *
 * Base code from drivers/net/phy/davicom.c
 *   Copyright 2010-2011 Freescale Semiconductor, Inc.
 *   author Andy Fleming
 *
 * Some code copied from linux kernel
 * Copyright (c) 2006 Herbert Valerio Riedel <hvr@gnu.org>
 */
#include <miiphy.h>

/* This code does not check the partner abilities. */
static int smsc_parse_status(struct phy_device *phydev)
{
	int mii_reg;

	mii_reg = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMSR);

	if (mii_reg & (BMSR_100FULL | BMSR_100HALF))
		phydev->speed = SPEED_100;
	else
		phydev->speed = SPEED_10;

	if (mii_reg & (BMSR_10FULL | BMSR_100FULL))
		phydev->duplex = DUPLEX_FULL;
	else
		phydev->duplex = DUPLEX_HALF;

	return 0;
}

static int smsc_startup(struct phy_device *phydev)
{
	int ret;

	ret = genphy_update_link(phydev);
	if (ret)
		return ret;

	return smsc_parse_status(phydev);
}

/*
 * LAN8840
 */

/* Data operations */
#define RGMII_LAN8840_MOD_DATA_NO_POST_INC	0x4000
#define RGMII_LAN8840_MOD_DATA_POST_INC_RW	0x8000

/* PHY Registers */
#define RGMII_LAN8840_MMD_ACCES_CTRL	0x0d
#define RGMII_LAN8840_MMD_REG_DATA	0x0e

#define PHY_ID_LAN8840			0x00221650
#define RGMII_LAN8840_SILICON_REV_MASK	0xfffff0

#define LAN8840RN_MMD_COMMON_CTRL_REG	BIT(1)
#define LAN8840RN_RXC_DLL_CTRL		76
#define LAN8840RN_TXC_DLL_CTRL		77
#define LAN8840RN_DLL_CTRL_BYPASS	BIT_MASK(14)
#define LAN8840RN_DLL_ENABLE_DELAY	0
#define LAN8840RN_DLL_DISABLE_DELAY	BIT(14)

static void lan8840_phy_setup_mmd(struct phy_device *phydev,
				  int devaddr, int regnum, u16 mode)
{
	/*select register addr for mmd*/
	phy_write(phydev, MDIO_DEVAD_NONE,
		  RGMII_LAN8840_MMD_ACCES_CTRL, devaddr);
	/*select register for mmd*/
	phy_write(phydev, MDIO_DEVAD_NONE,
		  RGMII_LAN8840_MMD_REG_DATA, regnum);
	/*setup mode*/
	phy_write(phydev, MDIO_DEVAD_NONE,
		  RGMII_LAN8840_MMD_ACCES_CTRL, (mode | devaddr));
}

static int lan8840_phy_extread(struct phy_device *phydev, int addr,
			       int devaddr, int regnum)
{
	lan8840_phy_setup_mmd(phydev, devaddr, regnum,
			      RGMII_LAN8840_MOD_DATA_NO_POST_INC);

	/* read the value */
	return phy_read(phydev, MDIO_DEVAD_NONE, RGMII_LAN8840_MMD_REG_DATA);
}

static int lan8840_phy_extwrite(struct phy_device *phydev, int addr,
				int devaddr, int regnum, u16 val)
{
	lan8840_phy_setup_mmd(phydev, devaddr, regnum,
			      RGMII_LAN8840_MOD_DATA_POST_INC_RW);

	/*write the value*/
	return	phy_write(phydev, MDIO_DEVAD_NONE,
			  RGMII_LAN8840_MMD_REG_DATA, val);
}

static int lan8840_config_rgmii_delay(struct phy_device *phydev)
{
	struct phy_driver *drv = phydev->drv;
	u16 rxcdll_val, txcdll_val, val;
	int ret;

	switch (phydev->interface) {
	case PHY_INTERFACE_MODE_RGMII:
		rxcdll_val = LAN8840RN_DLL_DISABLE_DELAY;
		txcdll_val = LAN8840RN_DLL_DISABLE_DELAY;
		break;
	case PHY_INTERFACE_MODE_RGMII_ID:
		rxcdll_val = LAN8840RN_DLL_ENABLE_DELAY;
		txcdll_val = LAN8840RN_DLL_ENABLE_DELAY;
		break;
	case PHY_INTERFACE_MODE_RGMII_RXID:
		rxcdll_val = LAN8840RN_DLL_ENABLE_DELAY;
		txcdll_val = LAN8840RN_DLL_DISABLE_DELAY;
		break;
	case PHY_INTERFACE_MODE_RGMII_TXID:
		rxcdll_val = LAN8840RN_DLL_DISABLE_DELAY;
		txcdll_val = LAN8840RN_DLL_ENABLE_DELAY;
		break;
	default:
		return 0;
	}

	val = drv->readext(phydev, 0, LAN8840RN_MMD_COMMON_CTRL_REG,
			   LAN8840RN_RXC_DLL_CTRL);
	val &= ~LAN8840RN_DLL_CTRL_BYPASS;
	val |= rxcdll_val;
	ret = drv->writeext(phydev, 0, LAN8840RN_MMD_COMMON_CTRL_REG,
			    LAN8840RN_RXC_DLL_CTRL, val);

	if (ret)
		return ret;

	val = drv->readext(phydev, 0, LAN8840RN_MMD_COMMON_CTRL_REG,
			   LAN8840RN_TXC_DLL_CTRL);
	val &= ~LAN8840RN_DLL_CTRL_BYPASS;
	val |= txcdll_val;
	ret = drv->writeext(phydev, 0, LAN8840RN_MMD_COMMON_CTRL_REG,
			    LAN8840RN_TXC_DLL_CTRL, val);

	return ret;
}

static int lan8840_config(struct phy_device *phydev)
{
	int ret;

	if (phy_interface_is_rgmii(phydev)) {
		ret = lan8840_config_rgmii_delay(phydev);
		if (ret)
			return ret;
	}

	return genphy_config(phydev);
}

U_BOOT_PHY_DRIVER(lan8700) = {
	.name = "SMSC LAN8700",
	.uid = 0x0007c0c0,
	.mask = 0xffff0,
	.features = PHY_BASIC_FEATURES,
	.config = &genphy_config_aneg,
	.startup = &smsc_startup,
	.shutdown = &genphy_shutdown,
};

U_BOOT_PHY_DRIVER(lan911x) = {
	.name = "SMSC LAN911x Internal PHY",
	.uid = 0x0007c0d0,
	.mask = 0xffff0,
	.features = PHY_BASIC_FEATURES,
	.config = &genphy_config_aneg,
	.startup = &smsc_startup,
	.shutdown = &genphy_shutdown,
};

U_BOOT_PHY_DRIVER(lan8710) = {
	.name = "SMSC LAN8710/LAN8720",
	.uid = 0x0007c0f0,
	.mask = 0xffff0,
	.features = PHY_BASIC_FEATURES,
	.config = &genphy_config_aneg,
	.startup = &genphy_startup,
	.shutdown = &genphy_shutdown,
};

U_BOOT_PHY_DRIVER(lan8740) = {
	.name = "SMSC LAN8740",
	.uid = 0x0007c110,
	.mask = 0xffff0,
	.features = PHY_BASIC_FEATURES,
	.config = &genphy_config_aneg,
	.startup = &genphy_startup,
	.shutdown = &genphy_shutdown,
};

U_BOOT_PHY_DRIVER(lan8741) = {
	.name = "SMSC LAN8741",
	.uid = 0x0007c120,
	.mask = 0xffff0,
	.features = PHY_BASIC_FEATURES,
	.config = &genphy_config_aneg,
	.startup = &genphy_startup,
	.shutdown = &genphy_shutdown,
};

U_BOOT_PHY_DRIVER(lan8742) = {
	.name = "SMSC LAN8742",
	.uid = 0x0007c130,
	.mask = 0xffff0,
	.features = PHY_BASIC_FEATURES,
	.config = &genphy_config_aneg,
	.startup = &genphy_startup,
	.shutdown = &genphy_shutdown,
};

U_BOOT_PHY_DRIVER(lan8840) = {
	.name = "Microchip LAN8840",
	.uid = PHY_ID_LAN8840,
	.mask = RGMII_LAN8840_SILICON_REV_MASK,
	.features = PHY_GBIT_FEATURES,
	.config = &lan8840_config,
	.startup = &genphy_startup,
	.shutdown = &genphy_shutdown,
	.writeext = &lan8840_phy_extwrite,
	.readext = &lan8840_phy_extread,
};
