/*
 * Copyright (c) 2011 The Chromium OS Authors.
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
#include <elf.h>

/**
 * Process a single ELF relocation entry
 *
 * @param addr		Pointer to address of intruction/data to relocate
 * @param info		The ELF information word / flags
 * @param symtab	The ELF relocation symbol table
 * @param reloc_off	Offset of relocated U-Boot relative to load address
 * @return 0 if ok, -1 on error
 */
static inline int arch_elf_relocate_entry(Elf32_Addr *addr, Elf32_Word info,
			    Elf32_Sym *symtab, ulong reloc_off)
{
	int sym;

	switch (ELF32_R_TYPE(info)) {
	/* relative fix: increase location by offset */
	case 23: /* TODO: add R_ARM_... defines to elf.h */
		*addr += reloc_off;
		break;

	/* absolute fix: set location to (offset) symbol value */
	case 2:
		sym = ELF32_R_SYM(info);
		*addr = symtab[sym].st_value + reloc_off;
		break;

	default:
		debug("*** Invalid relocation\n");
		return -1;
	}
	return 0;
}
