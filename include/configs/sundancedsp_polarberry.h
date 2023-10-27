#ifndef __CONFIG_H
#define __CONFIG_H

#include <linux/sizes.h>

#define CFG_SYS_SDRAM_BASE       0x80000000

/* Environment options */

#if defined(CONFIG_CMD_DHCP)
#define BOOT_TARGET_DEVICES_DHCP(func)	func(DHCP, dhcp, na)
#else
#define BOOT_TARGET_DEVICES_DHCP(func)
#endif

#if defined(CONFIG_CMD_MTD)
# define BOOT_TARGET_DEVICES_QSPI(func) func(QSPI, qspi, na)
#else
# define BOOT_TARGET_DEVICES_QSPI(func)
#endif

#if defined(CONFIG_CMD_MMC)
#define BOOT_TARGET_DEVICES_MMC(func)	func(MMC, mmc, 0)
#else
#define BOOT_TARGET_DEVICES_MMC(func)
#endif

#if defined(CONFIG_MPFS_PRIORITISE_QSPI_BOOT)
#define BOOTENV_DEV_QSPI(devtypeu, devtypel, instance) \
	"bootcmd_qspi=echo Trying to boot from QSPI...; "\
			"setenv scriptname boot.scr.uimg; " \
			"if mtd list; then setenv mtd_present true; " \
			"mtd read env ${scriptaddr} 0; " \
			"source ${scriptaddr}; setenv mtd_present; " \
			"fi\0 "

#define BOOTENV_DEV_NAME_QSPI(devtypeu, devtypel, instance) \
	"qspi "
#else
#define BOOTENV_DEV_QSPI(devtypeu, devtypel, instance) \
	""

#define BOOTENV_DEV_NAME_QSPI(devtypeu, devtypel, instance) \
	""
#endif

#define BOOT_TARGET_DEVICES(func) \
	BOOT_TARGET_DEVICES_QSPI(func)\
	BOOT_TARGET_DEVICES_MMC(func)\
	BOOT_TARGET_DEVICES_DHCP(func)

#define BOOTENV_DESIGN_OVERLAYS \
	"design_overlays=" \
	"if test -n ${no_of_overlays}; then " \
		"setenv inc 1; " \
		"setenv idx 0; " \
		"fdt resize ${dtbo_size}; " \
		"while test $idx -ne ${no_of_overlays}; do " \
			"setenv dtbo_name dtbo_image${idx}; " \
			"setenv fdt_cmd \"fdt apply $\"$dtbo_name; " \
			"run fdt_cmd; " \
			"setexpr idx $inc + $idx; " \
		"done; " \
	"fi;\0 " \

#if defined(CONFIG_FIT_SIGNATURE)
#define BOOTENV\
	"fdt_high=0xffffffffffffffff;\0 initrd_high=0xffffffffffffffff;\0" \
	"bootcmd=if mtd list; then echo Trying to boot from QSPI...;" \
	"ubi part rootfs; ubifsmount ubi0:rootfs; ubifsload 0x80000000 boot/fitImage;" \
	"ubifsumount; ubi detach; " \
	"run setbootargs;" \
	"cp 0x80000000 ${scriptaddr} ${filesize};" \
	"bootm start ${scriptaddr}#conf-microchip_mpfs-icicle-kit.dtb#${dtbo_conf};" \
	"bootm loados; bootm prep; " \
	"fdt set /soc/ethernet@20112000 mac-address ${icicle_mac_addr0}; " \
	"fdt set /soc/ethernet@20110000 mac-address ${icicle_mac_addr1}; " \
	"bootm go; " \
	"reset; else " \
	"setenv devnum 0; setenv mmcbootpart 1;"\
	"if mmc rescan; then " \
	"load mmc 0:${mmcbootpart} ${scriptaddr} fitImage; " \
	"bootm start ${scriptaddr}; " \
	"bootm loados ${scriptaddr}; " \
	"bootm ramdisk; " \
	"bootm prep; " \
	"fdt set /soc/ethernet@20112000 mac-address ${icicle_mac_addr0}; " \
	"fdt set /soc/ethernet@20110000 mac-address ${icicle_mac_addr1}; " \
	"run design_overlays;" \
	"bootm go; " \
	"reset; " \
	"fi; fi\0 "

#undef CONFIG_BOOTCOMMAND
#define CONFIG_BOOTCOMMAND\
	"run bootcmd;reset;" \

#if defined(CONFIG_MTD_SPI_NAND)
#define BOOTARGS\
	"qspibootargs=uio_pdrv_genirq.of_id=generic-uio ubi.mtd=2 root=ubi0:rootfs rootfstype=ubifs rootwait rw\0 " \
	"setbootargs=setenv bootargs ${qspibootargs} mtdparts=spi2.0:2m(payload),128k(env),119m(rootfs)\0 " \
	"dtbo_conf=conf-mpfs_icicle_flash5_click.dtbo\0 "
#else
#define BOOTARGS\
	"qspibootargs=uio_pdrv_genirq.of_id=generic-uio ubi.mtd=2 root=ubi0:rootfs rootfstype=ubifs rootwait rw\0 " \
	"setbootargs=setenv bootargs ${qspibootargs} mtdparts=spi2.0:2m(payload),128k(env),28m(rootfs)\0 " \
	"dtbo_conf=conf-mpfs_icicle_pmod_sf3.dtbo \0"
#endif

#define CFG_EXTRA_ENV_SETTINGS \
	"bootm_size=0x10000000\0" \
	"scriptaddr=0x8e000000\0" \
	BOOTENV_DESIGN_OVERLAYS \
	BOOTARGS \
	BOOTENV
#else
#include <config_distro_bootcmd.h>

#define CFG_EXTRA_ENV_SETTINGS \
	"fdtfile=microchip/mpfs-polarberry.dtb\0" \
	"bootm_size=0x10000000\0" \
	"scriptaddr=0x8e000000\0" \
	BOOTENV_DESIGN_OVERLAYS \
	BOOTENV \

#endif
#endif /* __CONFIG_H */
