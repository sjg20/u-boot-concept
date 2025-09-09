/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Helper functions for U-Boot library demo
 *
 * Copyright 2025 Canonical Ltd.
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#ifndef __DEMO_HELPER_H
#define __DEMO_HELPER_H

/**
 * demo_show_banner() - Show the demo banner
 */
void demo_show_banner(void);

/**
 * demo_show_footer() - Show the demo footer
 */
void demo_show_footer(void);

/**
 * demo_add_numbers() - Add two numbers and print the result
 *
 * @a: First number
 * @b: Second number
 * Return: Sum of the two numbers
 */
int demo_add_numbers(int a, int b);

#endif /* __DEMO_HELPER_H */
