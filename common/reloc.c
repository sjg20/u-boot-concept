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

#include <common.h>
#include <asm-generic/sections.h>
#include <asm/reloc.h>
#include <reloc.h>
#include <nand.h>

DECLARE_GLOBAL_DATA_PTR;

static int reloc_make_copy(void)
{
	char *dst_addr = (char *)gd->relocaddr;

	/* TODO: __text_start would be better when we have it */
	char *src_addr = (char *)_start;
	/* TODO: switch over to __image_copy_end when we can */
#ifdef CONFIG_SPL_BUILD
	char *end_addr = src_addr + _image_copy_end_ofs;
#else
	char *end_addr = src_addr + _rel_dyn_start_ofs;
#endif

	if (dst_addr != src_addr) {
		size_t size = end_addr - src_addr;

		debug("%s: copy code %p-%p to %p-%p\n", __func__,
		      src_addr, end_addr, dst_addr, dst_addr + size);
		memcpy(dst_addr, src_addr, size);
	}
	return 0;
}

static int reloc_elf(void)
{
#ifndef CONFIG_SPL_BUILD
	const Elf32_Rel *ptr, *end;
	Elf32_Addr *addr;
	char *src_addr = (char *)_start;
	Elf32_Sym *dynsym;
	ulong reloc_ofs = gd->reloc_off;

	/* scan the relocation table for relevant entries */
	ptr = (Elf32_Rel *)(src_addr + _rel_dyn_start_ofs);
	end = (Elf32_Rel *)(src_addr + _rel_dyn_end_ofs);
	dynsym = (Elf32_Sym *)(src_addr + _dynsym_start_ofs);
	debug("%s: process reloc entries %p-%p, dynsym at %p\n", __func__,
	      ptr, end, dynsym);
	for (; ptr < end; ptr++) {
		addr = (Elf32_Addr *)(ptr->r_offset + reloc_ofs);
		if (arch_elf_relocate_entry(addr, ptr->r_info, dynsym,
				reloc_ofs))
			return -1;
	}
#endif
	return 0;
}

static int reloc_clear_bss(void)
{
	char *dst_addr = (char *)_start + _bss_start_ofs;
	size_t size = _bss_end_ofs - _bss_start_ofs;

#ifndef CONFIG_SPL_BUILD
	/* No relocation for SPL (TBD: better to set reloc_off to zero) */
	dst_addr += gd->reloc_off;
#endif

	/* TODO: use memset */
	debug("%s: zero bss %p-%p\n", __func__, dst_addr, dst_addr + size);
	memset(dst_addr, '\0', size);

	return 0;
}

void __relocate_code(ulong dest_addr_sp, gd_t *new_gd, ulong dest_addr)
{
	ulong new_board_init_r = (uintptr_t)board_init_r + gd->reloc_off;

	/* TODO: It might be better to put the offsets in global data */
	debug("%s, dest_addr_sp=%lx, new_gd=%p, dest_addr=%lx\n", __func__,
	      dest_addr_sp, new_gd, dest_addr);
	reloc_make_copy();
	reloc_elf();
	reloc_clear_bss();

	debug("relocation complete: starting from board_init_r() at %lx\n",
	      new_board_init_r);
	/* TODO: tidy this up since we don't want a separate nand_boot() */
#ifdef CONFIG_NAND_SPL
	nand_boot();
#else
	pivot_to_board_init_r(new_gd, dest_addr,
			      (board_init_r_func)new_board_init_r,
			      dest_addr_sp);
#endif
}

/* Allow architectures to override this function - initially they all will */
void relocate_code(ulong dest_sp, gd_t *new_gd, ulong dest_add)
	__attribute__((weak, alias("__relocate_code")));
