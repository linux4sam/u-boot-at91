// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2026 Microchip Technology Inc. and its subsidiaries
 *
 * Author: Ryan Wanner <ryan.wanner@microchip.com>
 */

#include <asm/io.h>
#include <asm/arch/at91sam9_smc.h>
#include <asm/arch/at91_common.h>
#include <asm/arch/at91_rstc.h>
#include <asm/arch/at91_sfr.h>
#include <asm/arch/clk.h>
#include <asm/arch/gpio.h>
#include <debug_uart.h>
#include <asm/mach-types.h>
#include <init.h>
#include <linux/delay.h>

#define LVDS_MIPI_DISPLAY_EEPROM	"eeprom@53"

void at91_ext_board_display_detect(const char *eeprom);

DECLARE_GLOBAL_DATA_PTR;

void at91_prepare_cpu_var(void);

int board_late_init(void)
{
	at91_prepare_cpu_var();

#if (IS_ENABLED(CONFIG_I2C_EEPROM))
	at91_ext_board_display_detect(LVDS_MIPI_DISPLAY_EEPROM);
#endif

	return 0;
}

#if (IS_ENABLED(CONFIG_DEBUG_UART_BOARD_INIT))
static void board_dbgu0_hw_init(void)
{
	at91_pio3_set_a_periph(AT91_PIO_PORTA, 26, 1);	/* DRXD */
	at91_pio3_set_a_periph(AT91_PIO_PORTA, 27, 1);	/* DTXD */

	at91_periph_clk_enable(ATMEL_ID_DBGU);
}

void board_debug_uart_init(void)
{
	board_dbgu0_hw_init();
}
#endif

int board_early_init_f(void)
{
	return 0;
}

int board_init(void)
{
	at91_set_pio_output(AT91_PIO_PORTC, 30, 1);	// set PC30 as output
	/* address of boot parameters */
	gd->bd->bi_boot_params = gd->bd->bi_dram[0].start + 0x100;

	return 0;
}

#if (IS_ENABLED(CONFIG_MISC_INIT_R))
int misc_init_r(void)
{
#if (IS_ENABLED(CONFIG_SPI_FLASH_SFDP_SUPPORT))
	at91_spi_nor_set_ethaddr("ethaddr");
#endif
	return 0;
}
#endif

int dram_init_banksize(void)
{
	return fdtdec_setup_memory_banksize();
}
