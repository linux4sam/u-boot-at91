// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries
 *
 * Author: Balamanikandan Gunasundar <balamanikandan.gunasundar@microchip.com>
 *	   Varshini Rajendran <varshini.rajendran@microchip.com>
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

DECLARE_GLOBAL_DATA_PTR;

void at91_prepare_cpu_var(void);

#ifdef CONFIG_RESET_PHY_R
void reset_phy(void)
{
	at91_set_pio_output(AT91_PIO_PORTC, 25, 0);
	mdelay(10);
	at91_set_pio_output(AT91_PIO_PORTC, 25, 1);
}
#endif

int board_late_init(void)
{
	at91_prepare_cpu_var();

	return 0;
}

#ifdef CONFIG_DEBUG_UART_BOARD_INIT
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

#define MAC24AA_MAC_OFFSET     0xfa

#ifdef CONFIG_MISC_INIT_R
int misc_init_r(void)
{
#ifdef CONFIG_I2C_EEPROM
	at91_set_ethaddr(MAC24AA_MAC_OFFSET);
#endif
	return 0;
}
#endif

int board_init(void)
{
	/* address of boot parameters */
	gd->bd->bi_boot_params = gd->bd->bi_dram[0].start + 0x100;

	return 0;
}

int dram_init_banksize(void)
{
	return fdtdec_setup_memory_banksize();
}

int dram_init(void)
{
	return fdtdec_setup_mem_size_base();
}
