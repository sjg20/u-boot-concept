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

static int prep_rec(enum efil_tag tag, uint size, void **recp)
{
	struct efil_hdr *hdr = bloblist_find(BLOBLISTT_EFI_LOG, 0);
	struct efil_rec_hdr *rec_hdr;

	if (!hdr)
		return -ENOENT;
	size += sizeof(struct efil_rec_hdr);
	if (hdr->upto + size > hdr->size)
		return -ENOSPC;

	hdr->pending_upto = hdr->upto + size;

	rec_hdr = (void *)hdr + hdr->upto;
	rec_hdr->size = size;
	rec_hdr->tag = tag;
	*recp = rec_hdr + 1;

	return 0;
}

static void *finish_rec(void)
{
	struct efil_hdr *hdr = bloblist_find(BLOBLISTT_EFI_LOG, 0);
	struct efil_rec_hdr *rec_hdr;

	if (!hdr || !hdr->pending_upto)
		return NULL;
	rec_hdr = (void *)hdr + hdr->upto;
	hdr->upto = hdr->pending_upto;
	hdr->pending_upto = 0;

	return rec_hdr + 1;
}

int efi_logs_allocate_pages(int type, int memory_type, efi_uintn_t pages,
			    u64 *memory)
{
	struct efil_allocate_pages *rec;
	int ret;

	ret = prep_rec(EFILT_ALLOCATE_PAGES, sizeof(*rec), (void **)&rec);
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

	rec = finish_rec();
	if (!rec)
		return -ENOSPC;
	rec->ret = efi_ret;
	rec->e_memory = *memory;
	rec->end = false;

	return 0;
}

int efi_log_show(void)
{
	struct efil_hdr *hdr = bloblist_find(BLOBLISTT_EFI_LOG, 0);
	struct efil_rec_hdr *rec_hdr;

	printf("EFI log\n");
	if (!hdr)
		return -ENOENT;
	for (rec_hdr = (void *)hdr + sizeof(*hdr);
	     (void *)rec_hdr - (void *)hdr < hdr->upto;
	     rec_hdr= (void *)rec_hdr+ rec_hdr->size) {
		printf("here\n");
	}

	return 0;
}

int efi_log_init(void)
{
	struct efil_hdr *hdr;

	hdr = bloblist_add(BLOBLISTT_EFI_LOG, CONFIG_EFI_LOG_SIZE, 0);
	if (!hdr) {
		log_warning("Failed to setup EFI log\n");
		return log_msg_ret("eli", -ENOMEM);
	}
	hdr->upto = sizeof(struct efil_hdr);
	hdr->size = CONFIG_EFI_LOG_SIZE;

	return 0;
}
