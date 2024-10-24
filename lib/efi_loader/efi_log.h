/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * EFI logging of calls from an EFI app
 *
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#ifndef __EFI_LOG_H
#define __EFI_LOG_H

#include <linux/types.h>
#include <efi.h>

struct efil_allocate_pages {
	int type;
	int memory_type;
	efi_uintn_t pages;
	u64 *memory;
	u64 e_memory;
};

struct efil_hdr {
	int upto;
	int size;
};

int efi_logs_allocate_pages(int type, int memory_type, efi_uintn_t pages,
			    u64 *memory);
int efi_loge_allocate_pages(efi_status_t r, uint64_t *memory);

/**
 * efi_log_show() - Show the EFI log
 *
 * Displays the log of EFI boot-services calls
 */
void efi_log_show(void);

#endif /* __EFI_LOG_H */
