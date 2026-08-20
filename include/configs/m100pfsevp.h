/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2019 Microchip Technology Inc.
 * Padmarao Begari <padmarao.begari@microchip.com>
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <linux/sizes.h>

#define CFG_SYS_SDRAM_BASE       0x80000000

/* Environment options */

#define BOOT_TARGET_DEVICES(func) \
	func(MMC, mmc, 0) \
	func(DHCP, dhcp, na)


#include <config_distro_bootcmd.h>

#ifndef CONFIG_BOOTCOMMAND
#define CONFIG_BOOTCOMMAND "run mmc_mmc"
#endif

#define CFG_EXTRA_ENV_SETTINGS	       \
	"addcons=setenv bootargs ${bootargs} " \
		"console=${consdev},${baudrate}\0" \
	"addip=setenv bootargs ${bootargs} ip=${ipaddr}:${serverip}:" \
		"${gatewayip}:${netmask}:${hostname}:${netdev}:off\0" \
	"addmisc=setenv bootargs ${bootargs} ${miscargs}\0" \
	"bootfile=fitImage\0" \
	"consdev=ttyS1\0" \
	"fdtextract=imxtract ${kernel_addr_r} " \
		"fdt-${fdt_file} ${fdt_addr_r}\0" \
	"fdtselect=if test ! -e microchip_mpfs-m100pfsevp-sdcard.dtb; then " \
		"if selsd; then " \
		"setenv fdt_file microchip_mpfs-m100pfsevp-sdcard.dtb; else " \
		"setenv fdt_file microchip_mpfs-m100pfsevp-emmc.dtb; fi; fi\0" \
	"hostname=m100pfsevp\0"	\
	"miscargs=earlycon=sbi uio_pdrv_genirq.of_id=generic-uio " \
		"pci-hpmemsize=0M libata.force=noncq\0" \
	"mmc_mmc=run mmcload fdtselect " \
		"mmcargs addcons addmisc; " \
	"bootm start ${kernel_addr_r}#conf-${fdt_file}; bootm loados ${kernel_addr_r}; bootm ramdisk; bootm prep; bootm go; \0" \
	"mmcargs=setenv bootargs root=${rootdev} rw rootwait\0" \
	"mmcload=mmc rescan; " \
		"load mmc 0:1 ${kernel_addr_r} ${bootfile}\0" \
	"net_nfs=run netload fdtselect " \
		"nfsargs addcons addip addmisc; " \
		"bootm start ${kernel_addr_r}#conf-${fdt_file}; bootm loados ${kernel_addr_r}; bootm ramdisk; bootm prep; bootm go; \0" \
	"netdev=eth0\0" \
	"netload=tftp ${kernel_addr_r} ${hostname}/${bootfile}\0" \
	"nfsargs=setenv bootargs root=/dev/nfs rw " \
	"nfsroot=${serverip}:${rootpath},v3,tcp\0" \
	"rootdev=/dev/mmcblk0p3\0" \
	"rootpath=/tftpboot/m100pfsevp/rootfs\0" \
	"devtype=mmc\0" \
	"devnum=0\0" \
	"bootm_size=0x10000000\0" \
	"kernel_addr_r=0x84000000\0" \
	"fdt_addr_r=0x88000000\0" \
	"scriptaddr=0x88100000\0" \
	"pxefile_addr_r=0x88200000\0" \
	"ramdisk_addr_r=0x88300000\0" \
	BOOTENV

#endif /* __CONFIG_H */
