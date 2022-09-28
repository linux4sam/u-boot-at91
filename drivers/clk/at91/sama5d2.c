// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries
 *
 * Author: Claudiu Beznea <claudiu.beznea@microchip.com>
 *
 * Based on sama5d2.c on Linux.
 */

#include <common.h>
#include <clk-uclass.h>
#include <dm.h>
#include <dt-bindings/clk/at91.h>
#include <linux/clk/at91_pmc.h>
#include <linux/clk-provider.h>

#include "pmc.h"

/* Clock names used as parents for multiple clocks. */
static const char *clk_names[] = {
	[ID_MAIN_RC_OSC]	= "main_rc_osc",
	[ID_MAIN_OSC]		= "main_osc",
	[ID_MAINCK]		= "mainck",
	[ID_PLLACK]		= "pllack",
	[ID_PLLADIVCK]		= "plladivck",
	[ID_MCK_DIV]		= "mck_div",
	[ID_MCK_PRES]		= "mck_pres",
	[ID_H32MX]		= "h32mxck",
	[ID_UTMI]		= "utmick",
	[ID_USBCK]		= "usbck"
};

/* Fractional PLL output range. */
static const struct clk_range plla_outputs[] = {
	{ .min = 600000000, .max = 1200000000, },
};

static u16 plla_icpll[] = { 0 };

static u8 plla_out[] = { 0 };

/* PLL characteristics. */
static const struct clk_pll_characteristics plla_characteristics = {
	.input = { .min = 8000000, .max = 50000000 },
	.num_output = ARRAY_SIZE(plla_outputs),
	.output = plla_outputs,
	.icpll = plla_icpll,
	.out = plla_out,
};

/* MCK characteristics. */
static const struct clk_master_characteristics mck_characteristics = {
	.output = { .min = 124000000, .max = 166000000, },
	.divisors = { 1, 2, 4, 3 },
};

/* MCK layout. */
static const struct clk_master_layout mck_layout = {
	.mask = 0x373,
	.pres_shift = 4,
	.offset = 0x30,
};

/* Programmable clock layout. */
static const struct clk_programmable_layout programmable_layout = {
	.pres_mask = 0xff,
	.pres_shift = 4,
	.css_mask = 0x7,
	.have_slck_mck = 0,
	.is_pres_direct = 1,
};

/* Peripheral clock layout. */
static const struct clk_pcr_layout pcr_layout = {
	.offset = 0x10c,
	.cmd = BIT(12),
	.gckcss_mask = GENMASK(10, 8),
	.pid_mask = GENMASK(6, 0),
};

/**
 * Programmable clock description
 * @n:			clock name
 * @cid:		clock id corresponding to clock subsystem
 */
static const struct {
	const char *n;
	u8 cid;
} sama5d2_prog[] = {
	{ .n = "prog0", .cid = ID_PROG0, },
	{ .n = "prog1", .cid = ID_PROG1, },
	{ .n = "prog2", .cid = ID_PROG2, },
};

/**
 * System clock description
 * @n:			clock name
 * @p:			parent clock name
 * @id:			clock id corresponding to system clock driver
 * @cid:		clock id corresponding to clock subsystem
 */
static const struct {
	const char *n;
	const char *p;
	u8 id;
	u8 cid;
} sama5d2_systemck[] = {
	{ .n = "ddrck", .p = "mck_div", .id = 2,  .cid = ID_DDR, },
	{ .n = "lcdck", .p = "mck_div", .id = 3,  .cid = ID_LCD, },
	{ .n = "uhpck", .p = "usbck",	.id = 6,  .cid = ID_USBH, },
	{ .n = "udpck", .p = "usbck",	.id = 7,  .cid = ID_USBD, },
	{ .n = "pck0",  .p = "prog0",	.id = 8,  .cid = ID_PCK0, },
	{ .n = "pck1",  .p = "prog1",	.id = 9,  .cid = ID_PCK1, },
	{ .n = "pck2",  .p = "prog2",	.id = 10, .cid = ID_PCK2, },
	{ .n = "iscck", .p = "mck_div", .id = 18, .cid = ID_ISCCK, },
};

/**
 * 32 bit matrix peripheral clock description
 * @n:		clock name
 * @id:		clock id
 */
static const struct {
	const char *n;
	struct clk_range r;
	u8 id;
} sama5d2_periph32ck[] = {
	{ .n = "macb0_clk",   .id = 5,  .r = { .min = 0, .max = 83000000 }, },
	{ .n = "tdes_clk",    .id = 11, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "matrix1_clk", .id = 14, },
	{ .n = "hsmc_clk",    .id = 17, },
	{ .n = "pioA_clk",    .id = 18, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "flx0_clk",    .id = 19, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "flx1_clk",    .id = 20, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "flx2_clk",    .id = 21, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "flx3_clk",    .id = 22, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "flx4_clk",    .id = 23, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "uart0_clk",   .id = 24, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "uart1_clk",   .id = 25, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "uart2_clk",   .id = 26, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "uart3_clk",   .id = 27, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "uart4_clk",   .id = 28, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "twi0_clk",    .id = 29, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "twi1_clk",    .id = 30, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "spi0_clk",    .id = 33, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "spi1_clk",    .id = 34, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "tcb0_clk",    .id = 35, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "tcb1_clk",    .id = 36, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "pwm_clk",     .id = 38, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "adc_clk",     .id = 40, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "uhphs_clk",   .id = 41, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "udphs_clk",   .id = 42, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "ssc0_clk",    .id = 43, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "ssc1_clk",    .id = 44, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "trng_clk",    .id = 47, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "pdmic_clk",   .id = 48, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "securam_clk", .id = 51, },
	{ .n = "i2s0_clk",    .id = 54, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "i2s1_clk",    .id = 55, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "can0_clk",    .id = 56, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "can1_clk",    .id = 57, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "ptc_clk",     .id = 58, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "classd_clk",  .id = 59, .r = { .min = 0, .max = 83000000 }, },
};

/**
 * 64 bit matrix peripheral clock description
 * @n:		clock name
 * @id:		clock id
 */
static const struct {
	char *n;
	u8 id;
} sama5d2_periphck[] = {
	{ .n = "dma0_clk",    .id = 6, },
	{ .n = "dma1_clk",    .id = 7, },
	{ .n = "aes_clk",     .id = 9, },
	{ .n = "aesb_clk",    .id = 10, },
	{ .n = "sha_clk",     .id = 12, },
	{ .n = "mpddr_clk",   .id = 13, },
	{ .n = "matrix0_clk", .id = 15, },
	{ .n = "sdmmc0_hclk", .id = 31, },
	{ .n = "sdmmc1_hclk", .id = 32, },
	{ .n = "lcdc_clk",    .id = 45, },
	{ .n = "isc_clk",     .id = 46, },
	{ .n = "qspi0_clk",   .id = 52, },
	{ .n = "qspi1_clk",   .id = 53, },
};

/**
 * Generic clock description
 * @n:			clock name
 * @r:			clock output range
 * @id:			clock id
 */
static const struct {
	const char *n;
	struct clk_range r;
	u8 id;
} sama5d2_gck[] = {
	{ .n = "sdmmc0_gclk", .id = 31, },
	{ .n = "sdmmc1_gclk", .id = 32, },
	{ .n = "tcb0_gclk",   .id = 35, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "tcb1_gclk",   .id = 36, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "pwm_gclk",    .id = 38, .r = { .min = 0, .max = 83000000 }, },
	{ .n = "isc_gclk",    .id = 46, },
	{ .n = "pdmic_gclk",  .id = 48, },
	{ .n = "i2s0_gclk",   .id = 54, },
	{ .n = "i2s1_gclk",   .id = 55, },
	{ .n = "can0_gclk",   .id = 56, .r = { .min = 0, .max = 80000000 }, },
	{ .n = "can1_gclk",   .id = 57, .r = { .min = 0, .max = 80000000 }, },
};

/* Clock setup description. */
static const struct pmc_clk_setup sama5d2_clk_setup[] = {
	/* Add here root clocks that need inital setup (e.g. PLLs). */
	{
		.pid = AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_UTMI),
		.cid = AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_USBCK),
		.rate = 48000000,
	},
};

#define prepare_mux_table(_allocs, _index, _dst, _src, _num, _label)	\
	do {								\
		int _i;							\
		(_dst) = kzalloc(sizeof(*(_dst)) * (_num), GFP_KERNEL);	\
		if (!(_dst)) {						\
			ret = -ENOMEM;					\
			goto _label;					\
		}							\
		(_allocs)[(_index)++] = (_dst);				\
		for (_i = 0; _i < (_num); _i++)				\
			(_dst)[_i] = (_src)[_i];			\
	} while (0)

static int sama5d2_clk_probe(struct udevice *dev)
{
	void __iomem *base = (void *)devfdt_get_addr_ptr(dev);
	unsigned int *clkmuxallocs[64], *muxallocs[64];
	unsigned int cm[10], *tmpclkmux;
	const char *p[10];
	struct clk clk, *c;
	int ret, clkmuxallocindex = 0, i;
	static const struct clk_range r = { 0, 0 };

	if (!base)
		return -EINVAL;

	ret = clk_get_by_index(dev, 0, &clk);
	if (ret)
		return ret;

	ret = clk_get_by_id(clk.id, &c);
	if (ret)
		return ret;

	clk_names[ID_SLCK] = kmemdup(clk_hw_get_name(c),
				     strlen(clk_hw_get_name(c)) + 1,
				     GFP_KERNEL);
	if (!clk_names[ID_SLCK])
		return -ENOMEM;

	ret = clk_get_by_index(dev, 1, &clk);
	if (ret)
		return ret;

	clk_names[ID_MAIN_XTAL] = kmemdup(clk_hw_get_name(&clk),
					  strlen(clk_hw_get_name(&clk)) + 1,
					  GFP_KERNEL);
	if (!clk_names[ID_MAIN_XTAL])
		return -ENOMEM;

	ret = clk_get_by_index(dev, 2, &clk);
	if (ret)
		goto fail;

	clk_names[ID_MAIN_RC] = kmemdup(clk_hw_get_name(&clk),
					strlen(clk_hw_get_name(&clk)) + 1,
					GFP_KERNEL);
	if (ret)
		goto fail;

	/* Register main rc oscillator. */
	c = at91_clk_main_rc(base, clk_names[ID_MAIN_RC_OSC],
			     clk_names[ID_MAIN_RC]);
	if (IS_ERR(c)) {
		ret = PTR_ERR(c);
		goto fail;
	}
	clk_dm(AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_MAIN_RC_OSC), c);

	/* Register main oscillator. */
	c = at91_clk_main_osc(base, clk_names[ID_MAIN_OSC],
			      clk_names[ID_MAIN_XTAL], false);
	if (IS_ERR(c)) {
		ret = PTR_ERR(c);
		goto fail;
	}
	clk_dm(AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_MAIN_OSC), c);

	/* Register mainck. */
	p[0] = clk_names[ID_MAIN_RC_OSC];
	p[1] = clk_names[ID_MAIN_OSC];
	cm[0] = AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_MAIN_RC_OSC);
	cm[1] = AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_MAIN_OSC);
	prepare_mux_table(clkmuxallocs, clkmuxallocindex, tmpclkmux, cm, 2,
			  fail);
	c = at91_clk_sam9x5_main(base, clk_names[ID_MAINCK], p,
				 2, tmpclkmux, PMC_TYPE_CORE);
	if (IS_ERR(c)) {
		ret = PTR_ERR(c);
		goto fail;
	}
	clk_dm(AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_MAINCK), c);

	/* Register PLL A clock. */
	c = at91_clk_register_pll(base, clk_names[ID_PLLACK],
				  clk_names[ID_MAINCK], 0, &sama5d3_pll_layout,
				  &plla_characteristics);
	if (IS_ERR(c)) {
		ret = PTR_ERR(c);
		goto fail;
	}
	clk_dm(AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_PLLACK), c);

	/* Register PLL A Div clock. */
	c = at91_clk_register_div(base, clk_names[ID_PLLADIVCK],
				  clk_names[ID_PLLACK], AT91_PMC_PLLADIV2);
	if (IS_ERR(c)) {
		ret = PTR_ERR(c);
		goto fail;
	}
	clk_dm(AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_PLLADIVCK), c);


	/* Register UTMI clock. */
	c = at91_clk_register_utmi(base, dev, clk_names[ID_UTMI],
				   clk_names[ID_MAINCK]);
	if (IS_ERR(c)) {
		ret = PTR_ERR(c);
		goto fail;
	}
	clk_dm(AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_UTMI), c);

	/* Register master prescaller clock. */
	p[0] = clk_names[ID_SLCK];
	p[1] = clk_names[ID_MAINCK];
	p[2] = clk_names[ID_PLLADIVCK];
	p[3] = clk_names[ID_UTMI];
	cm[0] = AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_SLCK);
	cm[1] = AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_MAINCK);
	cm[2] = AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_PLLADIVCK);
	cm[3] = AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_UTMI);
	prepare_mux_table(clkmuxallocs, clkmuxallocindex, tmpclkmux, cm, 4,
			  fail);
	c = at91_clk_register_master_pres(base, clk_names[ID_MCK_PRES], p, 4,
					  &mck_layout, &mck_characteristics,
					  tmpclkmux);
	if (IS_ERR(c)) {
		ret = PTR_ERR(c);
		goto fail;
	}
	clk_dm(AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_MCK_PRES), c);

	/* Register master div clock. */
	c = at91_clk_register_master_div(base, clk_names[ID_MCK_DIV],
					 clk_names[ID_MCK_PRES], &mck_layout,
					 &mck_characteristics);
	if (IS_ERR(c)) {
		ret = PTR_ERR(c);
		goto fail;
	}
	clk_dm(AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_MCK_DIV), c);

	c = at91_clk_register_div(base, clk_names[ID_H32MX],
				  clk_names[ID_MCK_DIV], AT91_PMC_H32MXDIV);
	if (IS_ERR(c)) {
		ret = PTR_ERR(c);
		goto fail;
	}
	clk_dm(AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_H32MX), c);


	/* Register USB clock. */
	p[0] = clk_names[ID_PLLADIVCK];
	p[1] = clk_names[ID_UTMI];
	cm[0] = AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_PLLADIVCK);
	cm[1] = AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_UTMI);
	prepare_mux_table(clkmuxallocs, clkmuxallocindex, tmpclkmux, cm, 2,
			  fail);
	c = at91sam9x5_clk_register_usb(base, clk_names[ID_USBCK], p, 2,
					tmpclkmux);
	if (IS_ERR(c)) {
		ret = PTR_ERR(c);
		goto fail;
	}
	clk_dm(AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_USBCK), c);

	/* Register programmable clocks. */
	p[0] = clk_names[ID_SLCK];
	p[1] = clk_names[ID_MAINCK];
	p[2] = clk_names[ID_PLLADIVCK];
	p[3] = clk_names[ID_UTMI];
	p[4] = clk_names[ID_MCK_DIV];
	cm[0] = AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_SLCK);
	cm[1] = AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_MAINCK);
	cm[2] = AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_PLLADIVCK);
	cm[3] = AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_UTMI);
	cm[4] = AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_MCK_DIV);
	for (i = 0; i < 3; i++) {
		prepare_mux_table(clkmuxallocs, clkmuxallocindex, tmpclkmux,
				  cm, 5, fail);
		c = at91_clk_register_programmable(base, sama5d2_prog[i].n, p,
						   5, i, &programmable_layout,
						   tmpclkmux, NULL);
		if (IS_ERR(c)) {
			ret = PTR_ERR(c);
			goto fail;
		}
		clk_dm(AT91_TO_CLK_ID(PMC_TYPE_CORE, sama5d2_prog[i].cid), c);
	}

	/* Register system clocks. */
	for (i = 0; i < ARRAY_SIZE(sama5d2_systemck); i++) {
		c = at91_clk_register_system(base, sama5d2_systemck[i].n,
					     sama5d2_systemck[i].p,
					     sama5d2_systemck[i].id);
		if (IS_ERR(c)) {
			ret = PTR_ERR(c);
			goto fail;
		}
		clk_dm(AT91_TO_CLK_ID(PMC_TYPE_SYSTEM, sama5d2_systemck[i].cid),
		       c);
	}

	/* Register 64bit matrix peripheral clocks. */
	for (i = 0; i < ARRAY_SIZE(sama5d2_periphck); i++) {
		c = at91_clk_register_sam9x5_peripheral(base, &pcr_layout,
							sama5d2_periphck[i].n,
							clk_names[ID_MCK_DIV],
							sama5d2_periphck[i].id,
							&r);
		if (IS_ERR(c)) {
			ret = PTR_ERR(c);
			goto fail;
		}
		clk_dm(AT91_TO_CLK_ID(PMC_TYPE_PERIPHERAL,
		       sama5d2_periphck[i].id), c);
	}

	/* Register 32bit matrix peripheral clocks. */
	for (i = 0; i < ARRAY_SIZE(sama5d2_periph32ck); i++) {
		c = at91_clk_register_sam9x5_peripheral(base, &pcr_layout,
							sama5d2_periph32ck[i].n,
							clk_names[ID_H32MX],
							sama5d2_periph32ck[i].id,
							&sama5d2_periph32ck[i].r);
		if (IS_ERR(c)) {
			ret = PTR_ERR(c);
			goto fail;
		}
		clk_dm(AT91_TO_CLK_ID(PMC_TYPE_PERIPHERAL,
		       sama5d2_periph32ck[i].id), c);
	}

	p[0] = clk_names[ID_SLCK];
	p[1] = clk_names[ID_MAINCK];
	p[2] = clk_names[ID_PLLADIVCK];
	p[3] = clk_names[ID_UTMI];
	p[4] = clk_names[ID_MCK_DIV];
	cm[0] = AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_SLCK);
	cm[1] = AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_MAINCK);
	cm[2] = AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_PLLADIVCK);
	cm[3] = AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_UTMI);
	cm[4] = AT91_TO_CLK_ID(PMC_TYPE_CORE, ID_MCK_DIV);
	for (i = 0; i < ARRAY_SIZE(sama5d2_gck); i++) {
		prepare_mux_table(clkmuxallocs, clkmuxallocindex, tmpclkmux,
				  cm, 5, fail);

		c = at91_clk_register_generic(base, &pcr_layout,
					      sama5d2_gck[i].n, p, tmpclkmux,
					      NULL, 5, sama5d2_gck[i].id,
					      &sama5d2_gck[i].r);
		if (IS_ERR(c)) {
			ret = PTR_ERR(c);
			goto fail;
		}
		clk_dm(AT91_TO_CLK_ID(PMC_TYPE_GCK, sama5d2_gck[i].id), c);
	}

	ret = at91_clk_setup(sama5d2_clk_setup, ARRAY_SIZE(sama5d2_clk_setup));
	if (ret)
		goto fail;

	return 0;

fail:
	for (i = 0; i < ARRAY_SIZE(muxallocs); i++)
		kfree(muxallocs[i]);

	for (i = 0; i < ARRAY_SIZE(clkmuxallocs); i++)
		kfree(clkmuxallocs[i]);

	return ret;
}

static const struct udevice_id sama5d2_clk_ids[] = {
	{ .compatible = "atmel,sama5d2-pmc" },
	{ /* Sentinel. */ },
};

U_BOOT_DRIVER(at91_sam9x60_pmc) = {
	.name = "at91-sama5d2-pmc",
	.id = UCLASS_CLK,
	.of_match = sama5d2_clk_ids,
	.ops = &at91_clk_ops,
	.probe = sama5d2_clk_probe,
	.flags = DM_FLAG_PRE_RELOC,
};
