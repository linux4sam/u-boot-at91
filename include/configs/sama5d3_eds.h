/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Configuration settings for the SAMA5D3 Ethernet Development System board.
 *
 * Copyright (C) 2023 Microchip Technology Inc. and its subsidiaries
 *
 * Author: Jerry Ray <jerry.ray@microchip.com>
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <linux/sizes.h>
#include "at91-sama5_common.h"

#define CFG_SYS_INIT_RAM_ADDR	ATMEL_BASE_SRAM
#define CFG_SYS_INIT_RAM_SIZE	(16 * 1024)

/*
 * This needs to be defined for the OHCI code to work but it is defined as
 * ATMEL_ID_UHPHS in the CPU specific header files.
 */
#define ATMEL_ID_UHP			32

/*
 * Specify the clock enable bit in the PMC_SCER register.
 */
#define ATMEL_PMC_UHP			BIT(6)

/* SDRAM */
#define CFG_SYS_SDRAM_BASE		0x20000000
#define CFG_SYS_SDRAM_SIZE		0x10000000

/* NAND flash */
#ifdef CONFIG_CMD_NAND
//#define CONFIG_SYS_MAX_NAND_DEVICE	1
#define CFG_SYS_NAND_BASE		0x60000000
/* our ALE is AD21 */
#define CFG_SYS_NAND_MASK_ALE		BIT(21)
/* our CLE is AD22 */
#define CFG_SYS_NAND_MASK_CLE		BIT(22)
#endif

/* USB */
/* SPL */
/* *** definitions moved into the uboot defconfig file. *** */

#endif
