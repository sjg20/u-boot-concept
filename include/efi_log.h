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
	enum efi_allocate_type type;
	enum efi_memory_type memory_type;
	efi_uintn_t pages;
	u64 *memory;
	u64 e_memory;
};

enum efil_tag {
	EFILT_ALLOCATE_PAGES,

	EFILT_COUNT,
};

struct efil_rec_hdr {
	enum efil_tag tag;
	int size;
	bool ended;
	efi_status_t e_ret;
};

struct efil_hdr {
	int upto;
	int pending_upto;
	int size;
};

int efi_logs_allocate_pages(int type, int memory_type, efi_uintn_t pages,
			    u64 *memory);
int efi_loge_allocate_pages(efi_status_t ret, uint64_t *memory);

/**
 * efi_log_show() - Show the EFI log
 *
 * Displays the log of EFI boot-services calls
 *
 * Return: 0 on success, or -ve error code
 */
int efi_log_show(void);

int efi_log_init(void);

#endif /* __EFI_LOG_H */
