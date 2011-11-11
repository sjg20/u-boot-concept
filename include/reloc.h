/*
 * Copyright (c) 2011 The Chromium OS Authors.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __RELOC_H
#define __RELOC_H

/* This is the prototype for the post-relocation init function */
typedef void (*board_init_r_func)(gd_t *, ulong);

/**
 * Call the relocated U-Boot. This is the last thing that is done after
 * relocation. This function does not return.
 *
 * @param new_gd		Pointer to the relocated global data
 * @param dest_addr		Base code address of relocated U-Boot
 * @param board_init_r_func	Pointer to relocated function to call
 */
void pivot_to_board_init_r(gd_t *new_gd, ulong dest_addr,
		board_init_r_func board_init_r, ulong dest_addr_sp)
		__attribute__ ((noreturn));

/**
 * Relocate U-Boot and jump to the relocated coded
 *
 * This copies U-Boot to a new location, zeroes the BSS, sets up a new stack
 * and jumps to board_init_r() in the relocated code using the
 * pivot_to_board_init_r() function. It does not return.
 *
 * @param dest_sp	New stack pointer to use
 * @param new_gd	Pointer to the relocated global data
 * @param dest_addr		Base code address of relocated U-Boot
 */
void relocate_code(ulong dest_sp, gd_t *new_gd, ulong dest_addr)
		__attribute__ ((noreturn));
#endif
