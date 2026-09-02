// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Microchip Technology, Inc.
 */

#include <debug_uart.h>
#include <init.h>
#include <fdtdec.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/arch/at91_common.h>
#include <asm/arch/atmel_pio4.h>
#include <asm/arch/clk.h>
#include <asm/arch/gpio.h>
#include <asm/arch/sama7d65.h>
#include <asm/mach-types.h>

DECLARE_GLOBAL_DATA_PTR;

static void board_leds_init(void)
{
	atmel_pio4_set_pio_output(AT91_PIO_PORTA, 19, 0);	/* LED RED */
	atmel_pio4_set_pio_output(AT91_PIO_PORTA, 20, 0);	/* LED GREEN */
	atmel_pio4_set_pio_output(AT91_PIO_PORTA, 21, 1);	/* LED BLUE */
}

int board_late_init(void)
{
	return 0;
}

#if (IS_ENABLED(CONFIG_DEBUG_UART_BOARD_INIT))
static void board_uart0_hw_init(void)
{
	/* FLEXCOM4 IO0 */
	atmel_pio4_set_f_periph(AT91_PIO_PORTA, 18, ATMEL_PIO_PUEN_MASK);
	/* FLEXCOM4 IO1 */
	atmel_pio4_set_f_periph(AT91_PIO_PORTA, 17, 0);

	at91_periph_clk_enable(ATMEL_ID_FLEXCOM4);
}

void board_debug_uart_init(void)
{
	board_uart0_hw_init();
}
#endif

int board_early_init_f(void)
{
	return 0;
}

#define MAC24AA_MAC_OFFSET     0xfa

#if (IS_ENABLED(CONFIG_MISC_INIT_R))
int misc_init_r(void)
{
#if (IS_ENABLED(CONFIG_I2C_EEPROM))
	at91_set_ethaddr(MAC24AA_MAC_OFFSET);
	at91_set_eth1addr(MAC24AA_MAC_OFFSET);
#endif
	return 0;
}
#endif

int board_init(void)
{
	/* address of boot parameters */
	gd->bd->bi_boot_params = gd->bd->bi_dram[0].start + 0x100;

	board_leds_init();

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
