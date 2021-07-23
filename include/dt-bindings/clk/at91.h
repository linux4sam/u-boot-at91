/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * This header provides constants for AT91 pmc status.
 * The constants defined in this header are being used in dts and PMC code.
 *
 * Copyright (C) 2020 Microchip Technology Inc. and its subsidiaries
 *
 * Author: Claudiu Beznea <claudiu.beznea@microchip.com>
 *
 * Based on include/dt-bindings/clock/at91.h on Linux.
 */

#ifndef _DT_BINDINGS_CLK_AT91_H
#define _DT_BINDINGS_CLK_AT91_H

#define PMC_TYPE_CORE		1
#define PMC_TYPE_SYSTEM		2
#define PMC_TYPE_PERIPHERAL	3
#define PMC_TYPE_GCK		4
#define PMC_TYPE_SLOW		5

/* Per SoC core clock identifiers. */
#ifdef CONFIG_SAMA5D2
#define ID_SLCK			1
#define ID_MAIN_XTAL		2
#define ID_MAIN_RC		3
#define ID_MAIN_RC_OSC		4
#define ID_MAIN_OSC		5
#define ID_MAINCK		6

#define ID_PLLACK		7
#define ID_PLLADIVCK		8
#define ID_MCK_DIV		9
#define ID_MCK_PRES		10

#define ID_H32MX		11

#define ID_UTMI			12

#define ID_PROG0		13
#define ID_PROG1		14
#define ID_PROG2		15

#define ID_PCK0			16
#define ID_PCK1			17
#define ID_PCK2			18

#define ID_DDR			19
#define ID_LCD			20
#define ID_USBH			21
#define ID_USBD			22
#define ID_ISCCK		23

#endif

#endif
