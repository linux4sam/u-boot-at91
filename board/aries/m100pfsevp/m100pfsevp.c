// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2019 Microchip Technology Inc.
 * Padmarao Begari <padmarao.begari@microchip.com>
 */

#include <command.h>
#include <dm.h>
#include <dm/devres.h>
#include <env.h>
#include <mmc.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <linux/compat.h>
#include <mpfs-mailbox.h>

DECLARE_GLOBAL_DATA_PTR;

#define MPFS_SYSREG_SOFT_RESET		((unsigned int *)0x20002088)

#define GPIO_IOBANK0_LO_GPOUT		((unsigned char *)0x20120088)
#define GPIO_IOBANK0_LO_CLEAR_BITS	((unsigned char *)0x201200a0)
#define GPIO_IOBANK0_LO_SET_BITS	((unsigned char *)0x201200a4)
#define GPIO_IOBANK0_SD_SEL_MASK	0x1000 /* bit #12 selects the SD card */

#define MPFS_MMC_DRIVER_PATH		"/soc/mmc@20008000"
#define MPFS_MMC_DRIVER_NAME		"sdhci-cdns"
#define MPFS_MMC_DEV_NUM		0

#define PERIPH_RESET_VALUE		0x1e8u

int board_init(void)
{
	/* For now nothing to do here. */

	return 0;
}

int board_early_init_f(void)
{
	unsigned int val;

	/* Reset uart, mmc peripheral */
	val = readl(MPFS_SYSREG_SOFT_RESET);
	val = (val & ~(PERIPH_RESET_VALUE));
	writel(val, MPFS_SYSREG_SOFT_RESET);
	return 0;
}

int board_late_init(void)
{
#if IS_ENABLED(CONFIG_MPFS_SYSCONTROLLER)
	u8 device_serial_number[16] = { 0 };
	struct udevice *dev;
	struct mpfs_sys_serv *sys_serv_priv;
	u32 ret;
	int i;

	ret = uclass_get_device_by_name(UCLASS_MISC, "syscontroller", &dev);
	if (ret) {
		debug("%s: system controller setup failed\n", __func__);
		return ret;
	}

	sys_serv_priv = devm_kzalloc(dev, sizeof(*sys_serv_priv), GFP_KERNEL);
	if (!sys_serv_priv)
		return -ENOMEM;

	sys_serv_priv->dev = dev;

	sys_serv_priv->sys_controller = mpfs_syscontroller_get(dev);
	ret = IS_ERR(sys_serv_priv->sys_controller);
	if (ret) {
		debug("%s:  Failed to register system controller sub device ret=%d\n", __func__, ret);
		return -ENODEV;
	}

	ret = mpfs_syscontroller_read_sernum(sys_serv_priv, device_serial_number);
	if (ret) {
		printf("Cannot read device serial number\n");
		return -EINVAL;
	}

	puts("S/N:   ");
	for (i = 15; i >= 0; i--)
	  printf("%02x", device_serial_number[i]);
	puts("\n");
#endif

	return 0;
}

/* code derived from cmd/bind.c */
static int unbind_mmc_dev(void)
{
	const char *path = MPFS_MMC_DRIVER_PATH;
	struct udevice *dev;
	int ret;
	ofnode ofnode;

	ofnode = ofnode_path(path);
	printf("%s: node=%ld\n", __func__, ofnode.of_offset);
	if (!ofnode_valid(ofnode)) {
		printf("%s is not a valid node path\n", path);
		return -EINVAL;
	}

	ret = device_find_global_by_ofnode(ofnode, &dev);

	if (!dev || ret) {
		printf("Cannot find a device with path %s\n", path);
		return -ENODEV;
	}

	ret = device_remove(dev, DM_REMOVE_NORMAL);
	if (ret) {
		printf("Unable to remove. err:%d\n", ret);
		return ret;
	}

	ret = device_unbind(dev);
	if (ret) {
		printf("Unable to unbind. err:%d\n", ret);
		return ret;
	}

	return 0;
}

static int bind_and_init_mmc_dev(void)
{
	const char *path = MPFS_MMC_DRIVER_PATH;
	const char *drv_name = MPFS_MMC_DRIVER_NAME;
	struct udevice *dev;
	struct udevice *parent = NULL;
	int ret;
	ofnode ofnode;
	struct driver *drv;
	drv = lists_driver_lookup_name(drv_name);
	if (!drv) {
		printf("%s is not a valid driver name\n", drv_name);
		return -ENOENT;
	}

	ofnode = ofnode_path(path);
	printf("%s: node=%ld\n", __func__, ofnode.of_offset);
	if (!ofnode_valid(ofnode)) {
		printf("%s is not a valid node path\n", path);
		return -EINVAL;
	}

	while (ofnode_valid(ofnode)) {
		if (!device_find_global_by_ofnode(ofnode, &parent))
			break;
		ofnode = ofnode_get_parent(ofnode);
		printf("%s: parent=%ld\n", __func__, ofnode.of_offset);
	}

	if (!parent) {
		printf("Cannot find a parent device for node path %s\n", path);
		return -ENODEV;
	}

	ofnode = ofnode_path(path);
	printf("%s: node=%ld\n", __func__, ofnode.of_offset);
	ret = lists_bind_fdt(parent, ofnode, &dev, NULL, false);

	if (!dev || ret) {
		printf("Unable to bind. err:%d\n", ret);
		return ret;
	}

	return mmc_init_device(MPFS_MMC_DEV_NUM);
}

static int fdt_mmc_fixup(int sd)
{
	void *blob = (void *)gd->fdt_blob;
	u32 bus_width;
	u32 node;
	int ret;

	node = fdt_path_offset(blob, MPFS_MMC_DRIVER_PATH);
	if (node < 0) {
		printf("No mmc path offset\n");
		return -ENODEV;
	}
	printf("%s: node=%d\n", __func__, node);

	bus_width = sd ? 4 : 8;
	ret = fdt_setprop_u32(blob, node, "bus-width", bus_width);
	if (ret) {
		printf("Error setting bus-width property\n");
		return -ENODEV;
	}

	return 0;
}

int do_selsd(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	unsigned int val;
	int sel = -1;
	int ret;

	val = readl(GPIO_IOBANK0_LO_GPOUT);
        if (argc == 1) {
		if (val & GPIO_IOBANK0_SD_SEL_MASK)
			return 0;
		else
		return 1;
        } else if (argc == 2) {
		if (argv[1][0] == '0') {
			if (val & GPIO_IOBANK0_SD_SEL_MASK)
				sel = 0;
		} else if (argv[1][0] == '1') {
			if (!(val & GPIO_IOBANK0_SD_SEL_MASK))
				sel = 1;
		} else {
			return CMD_RET_USAGE;
		}
		if (sel >= 0) {
			ret = unbind_mmc_dev();
			if (ret)
				return CMD_RET_FAILURE;
			if (sel) {
				writel(GPIO_IOBANK0_SD_SEL_MASK,
				       GPIO_IOBANK0_LO_SET_BITS);
			} else {
				writel(GPIO_IOBANK0_SD_SEL_MASK,
				       GPIO_IOBANK0_LO_CLEAR_BITS);
			}
			ret = fdt_mmc_fixup(sel);
			if (ret)
				return CMD_RET_FAILURE;
			ret = bind_and_init_mmc_dev();
			if (ret)
				return CMD_RET_FAILURE;
		}
	} else {
		return CMD_RET_USAGE;
	}

        return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
        selsd, CONFIG_SYS_MAXARGS, 1, do_selsd,
        "Get or set the SD card as MMC device\n",
        "selsd - returns 0 if the SD card or 1 if the eMMC is selected\n"
        "selsd 1 - select the SD card\n"
        "selsd 0 - select the eMMC\n"
        );
