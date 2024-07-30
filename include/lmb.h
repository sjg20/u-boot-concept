/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef _LINUX_LMB_H
#define _LINUX_LMB_H
#ifdef __KERNEL__

#include <asm/types.h>
#include <asm/u-boot.h>
#include <linux/bitops.h>

/*
 * Logical memory blocks.
 *
 * Copyright (C) 2001 Peter Bergner, IBM Corp.
 */

#include <alist.h>

struct lmb {
	struct alist free_mem;
	struct alist used_mem;
};


/**
 * enum lmb_flags - definition of memory region attributes
 * @LMB_NONE: no special request
 * @LMB_NOMAP: don't add to mmu configuration
 */
enum lmb_flags {
	LMB_NONE		= BIT(0),
	LMB_NOMAP		= BIT(1),
	LMB_NOOVERWRITE		= BIT(2),
};

/**
 * struct lmb_region - Description of one region.
 *
 * @base:	Base address of the region.
 * @size:	Size of the region
 * @flags:	memory region attributes
 */
struct lmb_region {
	phys_addr_t base;
	phys_size_t size;
	enum lmb_flags flags;
};

/**
 * initr_lmb() - Initialise the LMB lists
 *
 * Initialise the LMB lists needed for keeping the memory map. There
 * are two lists, in form of alloced list data structure. One for the
 * available memory, and one for the used memory.
 *
 * Return: 0 on success, -ve on error
 */
int initr_lmb(void);

/**
 * lmb_add_memory() - Add memory range for LMB allocations
 *
 * Add the entire available memory range to the pool of memory that
 * can be used by the LMB module for allocations.
 *
 * Return: None
 *
 */
void lmb_add_memory(void);

long lmb_add(phys_addr_t base, phys_size_t size, uint flags);
long lmb_reserve(phys_addr_t base, phys_size_t size, uint flags);
phys_addr_t lmb_alloc(phys_size_t size, ulong align, uint flags);
phys_addr_t lmb_alloc_base(phys_size_t size, ulong align, phys_addr_t max_addr,
			   uint flags);
phys_addr_t lmb_alloc_addr(phys_addr_t base, phys_size_t size, uint flags);
phys_size_t lmb_get_free_size(phys_addr_t addr);

/**
 * lmb_is_reserved_flags() - test if address is in reserved region with flag bits set
 *
 * The function checks if a reserved region comprising @addr exists which has
 * all flag bits set which are set in @flags.
 *
 * @addr:	address to be tested
 * @flags:	bitmap with bits to be tested
 * Return:	1 if matching reservation exists, 0 otherwise
 */
int lmb_is_reserved_flags(phys_addr_t addr, int flags);

long lmb_free(phys_addr_t base, phys_size_t size, uint flags);

void lmb_dump_all(void);
void lmb_dump_all_force(void);

void board_lmb_reserve(void);
void arch_lmb_reserve(void);
void arch_lmb_reserve_generic(ulong sp, ulong end, ulong align);

#if CONFIG_IS_ENABLED(LMB)
/**
 * lmb_init() - Initialise the LMB memory
 *
 * Initialise the LMB-subsystem-related data structures.
 *
 * Return: 0 if OK, -ve on failure.
 */
int lmb_init(void);
#else
static int lmb_init(void) { return 0; }
#endif

/**
 * lmb_push() - Store existing lmb state and set up a new state
 *
 * This is only used for testing
 *
 * @store: Place to store the current lmb state
 * Return: 0 if OK, -ENOMEM if out of memory
 */
int lmb_push(struct lmb *store);

/**
 * lmb_pop() - Restore an old lmb state
 *
 * This is only used for testing
 *
 * @store: lmb state to restore
 */
void lmb_pop(struct lmb *store);

struct lmb *lmb_get(void);

#endif /* __KERNEL__ */

#endif /* _LINUX_LMB_H */
