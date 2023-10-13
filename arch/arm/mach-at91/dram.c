// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2023 Microchip Technology, Inc. and its subsidiaries
 */

#include <fdtdec.h>

__weak int dram_init(void)
{
	return fdtdec_setup_mem_size_base();
}
