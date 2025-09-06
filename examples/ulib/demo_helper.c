// SPDX-License-Identifier: GPL-2.0+
/*
 * Helper functions for U-Boot library demo
 *
 * Copyright 2025 Canonical Ltd.
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#include <u-boot-api.h>

void demo_show_banner(void)
{
	ub_printf("=================================\n");
	ub_printf("    U-Boot Library Demo Helper\n");
	ub_printf("=================================\n");
}

void demo_show_footer(void)
{
	ub_printf("=================================\n");
	ub_printf("      Demo Complete!\n");
	ub_printf("=================================\n");
}

int demo_add_numbers(int a, int b)
{
	ub_printf("Helper: Adding %d + %d = %d\n", a, b, a + b);

	return a + b;
}
