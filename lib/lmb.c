// SPDX-License-Identifier: GPL-2.0+
/*
 * Procedures for maintaining information about logical memory blocks.
 *
 * Peter Bergner, IBM Corp.	June 2001.
 * Copyright (C) 2001 Peter Bergner.
 */

#include <common.h>
#include <efi_loader.h>
#include <image.h>
#include <mapmem.h>
#include <lmb.h>
#include <log.h>
#include <malloc.h>

#include <asm/global_data.h>
#include <asm/sections.h>

DECLARE_GLOBAL_DATA_PTR;

#define LMB_ALLOC_ANYWHERE	0

static void lmb_dump_region(struct lmb_region *rgn, char *name)
{
	int i;

	printf(" %s.cnt = 0x%lx / max = 0x%lx\n", name, rgn->cnt, rgn->max);

	for (i = 0; i < rgn->cnt; i++) {
		struct lmb_area *area = &rgn->area[i];
		unsigned long long end;

		end = area->base + area->size - 1;

		printf(" %s[%d]\t[0x%llx-0x%llx], 0x%08llx bytes flags: %x\n",
		       name, i, (unsigned long long)area->base, end,
		       (unsigned long long)area->size, area->flags);
	}
}

void lmb_dump_all_force(struct lmb *lmb)
{
	printf("lmb_dump_all:\n");
	lmb_dump_region(&lmb->memory, "memory");
	lmb_dump_region(&lmb->reserved, "reserved");
}

void lmb_dump_all(struct lmb *lmb)
{
#ifdef DEBUG
	lmb_dump_all_force(lmb);
#endif
}

static long lmb_addrs_overlap(phys_addr_t base1, phys_size_t size1,
			      phys_addr_t base2, phys_size_t size2)
{
	const phys_addr_t base1_end = base1 + size1 - 1;
	const phys_addr_t base2_end = base2 + size2 - 1;

	return base1 <= base2_end && base2 <= base1_end;
}

static long lmb_addrs_adjacent(phys_addr_t base1, phys_size_t size1,
			       phys_addr_t base2, phys_size_t size2)
{
	if (base2 == base1 + size1)
		return 1;
	else if (base1 == base2 + size2)
		return -1;

	return 0;
}

static long lmb_areas_adjacent(struct lmb_region *rgn, ulong a1, ulong a2)
{
	phys_addr_t base1 = rgn->area[a1].base;
	phys_size_t size1 = rgn->area[a1].size;
	phys_addr_t base2 = rgn->area[a2].base;
	phys_size_t size2 = rgn->area[a2].size;

	return lmb_addrs_adjacent(base1, size1, base2, size2);
}

static void lmb_remove_area(struct lmb_region *rgn, unsigned long area)
{
	unsigned long i;

	for (i = area; i < rgn->cnt - 1; i++)
		rgn->area[i] = rgn->area[i + 1];
	rgn->cnt--;
}

/* Assumption: base addr of area 1 < base addr of area 2 */
static void lmb_coalesce_areas(struct lmb_region *rgn, uint a1, uint a2)
{
	rgn->area[a1].size += rgn->area[a2].size;
	lmb_remove_area(rgn, a2);
}

void lmb_init(struct lmb *lmb)
{
#if IS_ENABLED(CONFIG_LMB_USE_MAX_AREAS)
	lmb->memory.max = CONFIG_LMB_MAX_AREAS;
	lmb->reserved.max = CONFIG_LMB_MAX_AREAS;
#else
	lmb->memory.max = CONFIG_LMB_MEMORY_AREAS;
	lmb->reserved.max = CONFIG_LMB_RESERVED_AREAS;
	lmb->memory.area = lmb->memory_areas;
	lmb->reserved.area = lmb->reserved_areas;
#endif
	lmb->memory.cnt = 0;
	lmb->reserved.cnt = 0;
}

void arch_lmb_reserve_generic(struct lmb *lmb, ulong sp, ulong end, ulong align)
{
	struct bd_info *bd = gd->bd;
	ulong bank_end;
	int bank;

	/*
	 * Reserve memory from aligned address below the bottom of U-Boot stack
	 * until end of U-Boot area using LMB to prevent U-Boot from overwriting
	 * that memory.
	 */
	debug("## Current stack ends at 0x%08lx ", sp);

	/* adjust sp by 4K to be safe */
	sp -= align;
	for (bank = 0; bank < CONFIG_NR_DRAM_BANKS; bank++) {
		if (!bd->bi_dram[bank].size || sp < bd->bi_dram[bank].start)
			continue;
		/* Watch out for RAM at end of address space! */
		bank_end = bd->bi_dram[bank].start + bd->bi_dram[bank].size - 1;
		if (sp > bank_end)
			continue;
		if (bank_end > end)
			bank_end = end - 1;

		lmb_reserve(lmb, sp, bank_end - sp + 1);

		if (gd->flags & GD_FLG_SKIP_RELOC)
			lmb_reserve(lmb, (phys_addr_t)(uintptr_t)_start, gd->mon_len);

		break;
	}
}

/**
 * efi_lmb_reserve() - add reservations for EFI memory
 *
 * Add reservations for all EFI memory areas that are not
 * EFI_CONVENTIONAL_MEMORY.
 *
 * @lmb:	lmb environment
 * Return:	0 on success, 1 on failure
 */
static __maybe_unused int efi_lmb_reserve(struct lmb *lmb)
{
	struct efi_mem_desc *memmap = NULL, *map;
	efi_uintn_t i, map_size = 0;
	efi_status_t ret;

	ret = efi_get_memory_map_alloc(&map_size, &memmap);
	if (ret != EFI_SUCCESS)
		return 1;

	for (i = 0, map = memmap; i < map_size / sizeof(*map); ++map, ++i) {
		if (map->type != EFI_CONVENTIONAL_MEMORY) {
			lmb_reserve_flags(lmb,
					  map_to_sysmem((void *)(uintptr_t)
							map->physical_start),
					  map->num_pages * EFI_PAGE_SIZE,
					  map->type == EFI_RESERVED_MEMORY_TYPE
					      ? LMB_NOMAP : LMB_NONE);
		}
	}
	efi_free_pool(memmap);

	return 0;
}

static void lmb_reserve_common(struct lmb *lmb, void *fdt_blob)
{
	arch_lmb_reserve(lmb);
	board_lmb_reserve(lmb);

	if (CONFIG_IS_ENABLED(OF_LIBFDT) && fdt_blob)
		boot_fdt_add_mem_rsv_regions(lmb, fdt_blob);

	if (CONFIG_IS_ENABLED(EFI_LOADER))
		efi_lmb_reserve(lmb);
}

/* Initialize the struct, add memory and call arch/board reserve functions */
void lmb_init_and_reserve(struct lmb *lmb, struct bd_info *bd, void *fdt_blob)
{
	int i;

	lmb_init(lmb);

	for (i = 0; i < CONFIG_NR_DRAM_BANKS; i++) {
		if (bd->bi_dram[i].size) {
			lmb_add(lmb, bd->bi_dram[i].start,
				bd->bi_dram[i].size);
		}
	}

	lmb_reserve_common(lmb, fdt_blob);
}

/* Initialize the struct, add memory and call arch/board reserve functions */
void lmb_init_and_reserve_range(struct lmb *lmb, phys_addr_t base,
				phys_size_t size, void *fdt_blob)
{
	lmb_init(lmb);
	lmb_add(lmb, base, size);
	lmb_reserve_common(lmb, fdt_blob);
}

/* This routine called with relocation disabled. */
static long lmb_add_area_flags(struct lmb_region *rgn, phys_addr_t base,
			       phys_size_t size, enum lmb_flags flags)
{
	unsigned long coalesced = 0;
	long adjacent, i;

	if (rgn->cnt == 0) {
		rgn->area[0].base = base;
		rgn->area[0].size = size;
		rgn->area[0].flags = flags;
		rgn->cnt = 1;
		return 0;
	}

	/* First try and coalesce this LMB with another. */
	for (i = 0; i < rgn->cnt; i++) {
		struct lmb_area *area = &rgn->area[i];

		phys_addr_t abase = area->base;
		phys_size_t asize = area->size;
		phys_size_t aflags = area->flags;
		phys_addr_t end = base + size - 1;
		phys_addr_t aend = abase + asize - 1;

		if (abase <= base && end <= aend) {
			if (flags == aflags)
				/* Already have this area, so we're done */
				return 0;
			else
				return -1; /* areas with new flags */
		}

		adjacent = lmb_addrs_adjacent(base, size, abase, asize);
		if (adjacent > 0) {
			if (flags != aflags)
				break;
			area->base -= size;
			area->size += size;
			coalesced++;
			break;
		} else if (adjacent < 0) {
			if (flags != aflags)
				break;
			area->size += size;
			coalesced++;
			break;
		} else if (lmb_addrs_overlap(base, size, abase, asize)) {
			/* areas overlap */
			return -1;
		}
	}

	if (i < rgn->cnt - 1 && lmb_areas_adjacent(rgn, i, i + 1)) {
		if (rgn->area[i].flags == rgn->area[i + 1].flags) {
			lmb_coalesce_areas(rgn, i, i + 1);
			coalesced++;
		}
	}

	if (coalesced)
		return coalesced;
	if (rgn->cnt >= rgn->max)
		return -1;

	/* Couldn't coalesce the LMB, so add it to the sorted table. */
	for (i = rgn->cnt-1; i >= 0; i--) {
		if (base < rgn->area[i].base) {
			rgn->area[i + 1] = rgn->area[i];
		} else {
			rgn->area[i + 1].base = base;
			rgn->area[i + 1].size = size;
			rgn->area[i + 1].flags = flags;
			break;
		}
	}

	if (base < rgn->area[0].base) {
		rgn->area[0].base = base;
		rgn->area[0].size = size;
		rgn->area[0].flags = flags;
	}

	rgn->cnt++;

	return 0;
}

static long lmb_add_area(struct lmb_region *rgn, phys_addr_t base,
			 phys_size_t size)
{
	return lmb_add_area_flags(rgn, base, size, LMB_NONE);
}

/* This routine may be called with relocation disabled. */
long lmb_add(struct lmb *lmb, phys_addr_t base, phys_size_t size)
{
	struct lmb_region *_rgn = &(lmb->memory);

	return lmb_add_area(_rgn, base, size);
}

long lmb_free(struct lmb *lmb, phys_addr_t base, phys_size_t size)
{
	struct lmb_region *rgn = &(lmb->reserved);
	phys_addr_t rgnbegin, rgnend;
	phys_addr_t end = base + size - 1;
	int i;

	rgnbegin = rgnend = 0; /* supress gcc warnings */

	/* Find the area where (base, size) belongs to */
	for (i = 0; i < rgn->cnt; i++) {
		rgnbegin = rgn->area[i].base;
		rgnend = rgnbegin + rgn->area[i].size - 1;

		if ((rgnbegin <= base) && (end <= rgnend))
			break;
	}

	/* Didn't find the area */
	if (i == rgn->cnt)
		return -1;

	/* Check to see if we are removing entire area */
	if ((rgnbegin == base) && (rgnend == end)) {
		lmb_remove_area(rgn, i);
		return 0;
	}

	/* Check to see if areaz is matching at the front */
	if (rgnbegin == base) {
		rgn->area[i].base = end + 1;
		rgn->area[i].size -= size;
		return 0;
	}

	/* Check to see if the area is matching at the end */
	if (rgnend == end) {
		rgn->area[i].size -= size;
		return 0;
	}

	/*
	 * We need to split the entry -  adjust the current one to the
	 * beginning of the hole and add the area after hole.
	 */
	rgn->area[i].size = base - rgn->area[i].base;

	return lmb_add_area_flags(rgn, end + 1, rgnend - end,
				    rgn->area[i].flags);
}

long lmb_reserve_flags(struct lmb *lmb, phys_addr_t base, phys_size_t size,
		       enum lmb_flags flags)
{
	struct lmb_region *_rgn = &(lmb->reserved);

	return lmb_add_area_flags(_rgn, base, size, flags);
}

long lmb_reserve(struct lmb *lmb, phys_addr_t base, phys_size_t size)
{
	return lmb_reserve_flags(lmb, base, size, LMB_NONE);
}

static long lmb_overlaps_region(struct lmb_region *rgn, phys_addr_t base,
				phys_size_t size)
{
	unsigned long i;

	for (i = 0; i < rgn->cnt; i++) {
		phys_addr_t abase = rgn->area[i].base;
		phys_size_t asize = rgn->area[i].size;

		if (lmb_addrs_overlap(base, size, abase, asize))
			break;
	}

	return i < rgn->cnt ? i : -1;
}

phys_addr_t lmb_alloc(struct lmb *lmb, phys_size_t size, ulong align)
{
	return lmb_alloc_base(lmb, size, align, LMB_ALLOC_ANYWHERE);
}

phys_addr_t lmb_alloc_base(struct lmb *lmb, phys_size_t size, ulong align,
			   phys_addr_t max_addr)
{
	phys_addr_t alloc;

	alloc = __lmb_alloc_base(lmb, size, align, max_addr);

	if (alloc == 0)
		printf("ERROR: Failed to allocate 0x%lx bytes below 0x%lx.\n",
		       (ulong)size, (ulong)max_addr);

	return alloc;
}

static phys_addr_t lmb_align_down(phys_addr_t addr, phys_size_t size)
{
	return addr & ~(size - 1);
}

phys_addr_t __lmb_alloc_base(struct lmb *lmb, phys_size_t size, ulong align,
			     phys_addr_t max_addr)
{
	long i, area;
	phys_addr_t base = 0;
	phys_addr_t res_base;

	for (i = lmb->memory.cnt - 1; i >= 0; i--) {
		phys_addr_t lmbbase = lmb->memory.area[i].base;
		phys_size_t lmbsize = lmb->memory.area[i].size;

		if (lmbsize < size)
			continue;
		if (max_addr == LMB_ALLOC_ANYWHERE)
			base = lmb_align_down(lmbbase + lmbsize - size, align);
		else if (lmbbase < max_addr) {
			base = lmbbase + lmbsize;
			if (base < lmbbase)
				base = -1;
			base = min(base, max_addr);
			base = lmb_align_down(base - size, align);
		} else
			continue;

		while (base && lmbbase <= base) {
			area = lmb_overlaps_region(&lmb->reserved, base, size);
			if (area < 0) {
				/* This area isn't reserved, take it */
				if (lmb_add_area(&lmb->reserved, base, size)
				    < 0)
					return 0;
				return base;
			}
			res_base = lmb->reserved.area[area].base;
			if (res_base < size)
				break;
			base = lmb_align_down(res_base - size, align);
		}
	}

	return 0;
}

/*
 * Try to allocate a specific address range: must be in defined memory but not
 * reserved
 */
phys_addr_t lmb_alloc_addr(struct lmb *lmb, phys_addr_t base, phys_size_t size)
{
	int area;

	/* Check if the requested address is in one of the memory areas */
	area = lmb_overlaps_region(&lmb->memory, base, size);
	if (area >= 0) {
		/*
		 * Check if the requested end address is in the same memory
		 * area we found.
		 */
		if (lmb_addrs_overlap(lmb->memory.area[area].base,
				      lmb->memory.area[area].size,
				      base + size - 1, 1)) {
			/* ok, reserve the memory */
			if (lmb_reserve(lmb, base, size) >= 0)
				return base;
		}
	}

	return 0;
}

/* Return number of bytes from a given address that are free */
phys_size_t lmb_get_free_size(struct lmb *lmb, phys_addr_t addr)
{
	int i, area;

	/* check if the requested address is in the memory areas */
	area = lmb_overlaps_region(&lmb->memory, addr, 1);
	if (area >= 0) {
		for (i = 0; i < lmb->reserved.cnt; i++) {
			if (addr < lmb->reserved.area[i].base) {
				/* first reserved range > requested address */
				return lmb->reserved.area[i].base - addr;
			}
			if (lmb->reserved.area[i].base +
			    lmb->reserved.area[i].size > addr) {
				/* requested addr is in this reserved range */
				return 0;
			}
		}
		/* if we come here: no reserved ranges above requested addr */
		return lmb->memory.area[lmb->memory.cnt - 1].base +
		       lmb->memory.area[lmb->memory.cnt - 1].size - addr;
	}

	return 0;
}

int lmb_is_reserved_flags(struct lmb *lmb, phys_addr_t addr, int flags)
{
	int i;

	for (i = 0; i < lmb->reserved.cnt; i++) {
		struct lmb_area *area = &lmb->reserved.area[i];
		phys_addr_t upper = area->base + area->size - 1;

		if (addr >= area->base && addr <= upper)
			return (area->flags & flags) == flags;
	}

	return 0;
}

int lmb_is_reserved(struct lmb *lmb, phys_addr_t addr)
{
	return lmb_is_reserved_flags(lmb, addr, LMB_NONE);
}

__weak void board_lmb_reserve(struct lmb *lmb)
{
	/* please define platform specific board_lmb_reserve() */
}

__weak void arch_lmb_reserve(struct lmb *lmb)
{
	/* please define platform specific arch_lmb_reserve() */
}
