/*
 * Copyright (c) 2013 The Chromium OS Authors.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <initcall.h>
#include <asm/io.h>
#include <asm/post.h>

DECLARE_GLOBAL_DATA_PTR;

int initcall_run_list(const init_fnc_t init_sequence[])
{
	const init_fnc_t *init_fnc_ptr;
	int count = 0;

	for (init_fnc_ptr = init_sequence; *init_fnc_ptr; ++init_fnc_ptr) {
		unsigned long reloc_ofs = 0;
		int ret;

		if (gd->flags & GD_FLG_RELOC)
			reloc_ofs = gd->reloc_off;
		debug("initcall: %p\n", (char *)*init_fnc_ptr - reloc_ofs);
		post_code(3 + count);
		if (gd->flags & GD_FLG_RELOC) {
// 			if (post >= 9) {
// 				post_code(0xc1);
// 				while (1);
// 			}
		}
// 		while (post == 0x5);
		ret = (*init_fnc_ptr)();
		if (ret) {
			printf("initcall sequence %p failed at call %d: %p (err=%d)\n",
			       init_sequence, count,
			       (char *)*init_fnc_ptr - reloc_ofs, ret);
			return -1;
		}
		count++;
	}
	return 0;
}
