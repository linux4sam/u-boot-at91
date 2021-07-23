// SPDX-License-Identifier:
/*
 */

#include <common.h>
#include <dm.h>
#include <syscon.h>

static const struct udevice_id microchip_sfr_ids[] = {
	{ .compatible = "atmel,sama5d2-sfr", },
	{ },
};

U_BOOT_DRIVER(microchip_sfr) = {
	.name = "microchip_sfr",
	.id = UCLASS_SYSCON,
	.of_match = microchip_sfr_ids,
};
