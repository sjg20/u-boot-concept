// SPDX-License-Identifier: GPL-2.0+
/*
 * EFI logging of calls from an EFI app
 *
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY LOGC_EFI

#include <bloblist.h>
#include <efi_log.h>
#include <errno.h>
#include <log.h>

static void *get_rec(uint size, int *errp)
{
	struct efil_hdr *hdr = bloblist_find(BLOBLISTT_EFI_LOG, 0);

	if (!hdr) {
		*errp = -ENOENT;
		return NULL;
	}
	if (hdr->upto + size > hdr->size) {
		*errp = -ENOSPC;
		return NULL;
	}
	*errp = 0;

	return (void *)hdr + hdr->upto;
}

int efi_logs_allocate_pages(int type, int memory_type, efi_uintn_t pages,
			    u64 *memory)
{
	struct efil_allocate_pages *rec;
	int ret;

	rec = get_rec(sizeof(*rec), &ret);
	if (ret)
		return ret;

	rec->type = type;
	rec->memory_type = memory_type;
	rec->pages = pages;
	rec->memory = memory;
	rec->e_memory = 0;
	rec->end = false;

	return 0;
}

int efi_loge_allocate_pages(efi_status_t efi_ret, uint64_t *memory)
{
	struct efil_allocate_pages *rec;
	int ret;

	rec = get_rec(sizeof(*rec), &ret);
	if (ret)
		return ret;
	rec->ret = efi_ret;
	rec->e_memory = *memory;
	rec->end = false;

	return 0;
}

int efi_log_show(void)
{
	struct efil_hdr *hdr = bloblist_find(BLOBLISTT_EFI_LOG, 0);
	struct efil_allocate_pages *rec;

	printf("EFI log\n");
	if (!hdr)
		return -ENOENT;
	rec = (void *)hdr + sizeof(*hdr);
	while ((void *)rec - (void *)hdr < hdr->upto) {
		printf("here\n");
	}

	return 0;
}

int efi_log_init(void)
{
	struct efil_hdr *hdr;

	hdr = bloblist_add(BLOBLISTT_EFI_LOG, CONFIG_EFI_LOG_SIZE, 0);
	if (!hdr)
		return log_msg_ret("eli", -ENOMEM);
	hdr->upto = 0;
	hdr->size = CONFIG_EFI_LOG_SIZE;

	return 0;
}
