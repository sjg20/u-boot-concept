/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Helpers for U-Boot running as an EFI payload.
 *
 * Copyright (c) 2024 Linaro, Ltd
 */

#ifndef _EFI_STUB_H
#define _EFI_STUB_H

#include <efi.h>
#include <linux/types.h>

struct efi_priv;

enum efi_entry_t {
	EFIET_END,	/* Signals this is the last (empty) entry */
	EFIET_MEMORY_MAP,
	EFIET_GOP_MODE,
	EFIET_SYS_TABLE,

	/* Number of entries */
	EFIET_MEMORY_COUNT,
};

/**
 * efi_info_get() - get an entry from an EFI table
 *
 * This function is called from U-Boot proper to read information set up by the
 * EFI stub. It can only be used when running from the EFI stub, not when U-Boot
 * is running as an app.
 *
 * @type:	Entry type to search for
 * @datap:	Returns pointer to entry data
 * @sizep:	Returns entry size
 * Return: 0 if OK, -ENODATA if there is no table, -ENOENT if there is no entry
 * of the requested type, -EPROTONOSUPPORT if the table has the wrong version
 */
int efi_info_get(enum efi_entry_t type, void **datap, int *sizep);

struct device_node;

/**
 * of_populate_from_efi() - Populate the live tree from EFI tables
 *
 * @root: Root node of tree to populate
 *
 * This function populates the live tree with information from EFI tables
 * it is only applicable when running U-Boot as an EFI payload with
 * CONFIG_EFI_STUB enabled.
 */
int of_populate_from_efi(struct device_node *root);

/**
 * dram_init_banksize_from_efi() - Initialize the memory banks from EFI tables
 *
 * This function initializes the memory banks from the EFI memory map table we
 * stashed from the EFI stub. It is only applicable when running U-Boot as an
 * EFI payload with CONFIG_EFI_STUB enabled.
 */
int dram_init_banksize_from_efi(void);

/**
 * efi_add_known_memory_from_efi() - Add known memory pages from the memory map
 * of the EFI bootloader that booted U-Boot. This is only applicable when running
 * U-Boot as an EFI payload with CONFIG_EFI_STUB enabled.
 */
void efi_add_known_memory_from_efi(void);

/**
 * setup_info_table() - sets up a table containing information from EFI
 *
 * We must call exit_boot_services() before jumping out of the stub into U-Boot
 * proper, so that U-Boot has full control of peripherals, memory, etc.
 *
 * Once we do this, we cannot call any boot-services functions so we must find
 * out everything we need to before doing that.
 *
 * Set up a struct efi_info_hdr table which can hold various records (e.g.
 * struct efi_entry_memmap) with information obtained from EFI.
 *
 * @priv: Pointer to our private information which contains the list
 * @size: Size of the table to allocate
 * Return: 0 if OK, non-zero on error
 */
int setup_info_table(struct efi_priv *priv, int size);

/**
 * add_entry_addr() - Add a new entry to the efi_info list
 *
 * This adds an entry, consisting of a tag and two lots of data. This avoids the
 * caller having to coalesce the data first
 *
 * @priv: Pointer to our private information which contains the list
 * @type: Type of the entry to add
 * @ptr1: Pointer to first data block to add
 * @size1: Size of first data block in bytes (can be 0)
 * @ptr2: Pointer to second data block to add
 * @size2: Size of second data block in bytes (can be 0)
 */
void add_entry_addr(struct efi_priv *priv, enum efi_entry_t type, void *ptr1,
		    int size1, void *ptr2, int size2);

/**
 * arch_efi_main_init() - Set up the stub ready for use
 *
 * @priv: Pointer to private information
 * @boot: Boot-services table
 * Return: 0 if OK, or EFI error code
 */
efi_status_t arch_efi_main_init(struct efi_priv *priv,
				struct efi_boot_services *boot);

/**
 * arch_efi_jump_to_payload() - Jump to the U-Boot payload
 *
 * Jumps to U-Boot in an arch-specific way
 *
 * @priv: Pointer to private information
 */
void arch_efi_jump_to_payload(struct efi_priv *priv);

efi_status_t EFIAPI efi_main_common(efi_handle_t image,
				    struct efi_system_table *sys_table);

/* true if we must use the hardware UART directory (EFI not available) */
extern bool use_hw_uart;

#endif /* _EFI_STUB_H */
