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

static const char *tag_name[EFILT_COUNT] = {
	"allocate_pages",
	"free_pages",
	"allocate_pool",
	"free_pool",
};

static const char *allocate_type_name[EFI_MAX_ALLOCATE_TYPE] = {
	"any-pages",
	"max-addr",
	"alloc-addr",
};

static const char *memory_type_name[EFI_MAX_MEMORY_TYPE] = {

	"reserved",
	"loader-code",
	"loader-data",
	"boot-code",
	"boot-data",
	"runtime-code",
	"runtime-data",
	"conventional",
	"unusable-memory",
	"acpi-reclaim",
	"acpi-nvs",
	"mmap-io",
	"mmap-ioport",
	"pal-code",
	"persistent",
	"unaccepted",
};

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
	rec_hdr->ended = false;
	*recp = rec_hdr + 1;

	return 0;
}

static void *finish_rec(efi_status_t ret)
{
	struct efil_hdr *hdr = bloblist_find(BLOBLISTT_EFI_LOG, 0);
	struct efil_rec_hdr *rec_hdr;

	if (!hdr || !hdr->pending_upto)
		return NULL;
	rec_hdr = (void *)hdr + hdr->upto;
	rec_hdr->ended = true;
	rec_hdr->e_ret = ret;


	hdr->upto = hdr->pending_upto;
	hdr->pending_upto = 0;

	return rec_hdr + 1;
}

int efi_logs_allocate_pages(enum efi_allocate_type type,
			    enum efi_memory_type memory_type, efi_uintn_t pages,
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

	return 0;
}

int efi_loge_allocate_pages(efi_status_t efi_ret, uint64_t *memory)
{
	struct efil_allocate_pages *rec;

	rec = finish_rec(efi_ret);
	if (!rec)
		return -ENOSPC;
	rec->e_memory = *memory;

	return 0;
}

int efi_logs_free_pages(uint64_t memory, efi_uintn_t pages)
{
	struct efil_free_pages *rec;
	int ret;

	ret = prep_rec(EFILT_FREE_PAGES, sizeof(*rec), (void **)&rec);
	if (ret)
		return ret;

	rec->memory = memory;
	rec->pages = pages;

	return 0;
}

int efi_loge_free_pages(efi_status_t efi_ret)
{
	struct efil_allocate_pages *rec;

	rec = finish_rec(efi_ret);
	if (!rec)
		return -ENOSPC;

	return 0;
}

int efi_logs_allocate_pool(enum efi_memory_type pool_type, efi_uintn_t size,
			   void **buffer)
{
	struct efil_allocate_pool *rec;
	int ret;

	ret = prep_rec(EFILT_ALLOCATE_POOL, sizeof(*rec), (void **)&rec);
	if (ret)
		return ret;

	rec->pool_type = pool_type;
	rec->size = size;
	rec->buffer = buffer;
	rec->e_buffer = NULL;

	return 0;
}

int efi_loge_allocate_pool(efi_status_t efi_ret, void **buffer)
{
	struct efil_allocate_pool *rec;

	rec = finish_rec(efi_ret);
	if (!rec)
		return -ENOSPC;
	rec->e_buffer = *buffer;

	return 0;
}

int efi_logs_free_pool(void *buffer)
{
	struct efil_free_pool *rec;
	int ret;

	ret = prep_rec(EFILT_FREE_POOL, sizeof(*rec), (void **)&rec);
	if (ret)
		return ret;

	rec->buffer = buffer;

	return 0;
}

int efi_loge_free_pool(efi_status_t efi_ret)
{
	struct efil_free_pool *rec;

	rec = finish_rec(efi_ret);
	if (!rec)
		return -ENOSPC;

	return 0;
}

static void show_enum(const char *type_name[], int type)
{
	printf("%s ", type_name[type]);
}

static void show_ulong(const char *prompt, ulong val)
{
	printf("%s %lx", prompt, val);
	if (val >= 10)
		printf("/%ld", val);
	printf(" ");
}

static void show_addr(const char *prompt, ulong addr)
{
	printf("%s %lx ", prompt, addr);
}

static void show_ret(efi_status_t ret)
{
	printf("ret %ld", ret);
}

void show_rec(int seq, struct efil_rec_hdr *rec_hdr)
{
	void *start = (void *)rec_hdr + sizeof(struct efil_rec_hdr);

	printf("%3d %s ", seq, tag_name[rec_hdr->tag]);
	switch (rec_hdr->tag) {
	case EFILT_ALLOCATE_PAGES: {
		struct efil_allocate_pages *rec = start;

		show_enum(allocate_type_name, rec->type);
		show_enum(memory_type_name, rec->memory_type);
		show_ulong("pages", (ulong)rec->pages);
		show_addr("memory", (ulong)rec->memory);
		if (rec_hdr->ended) {
			show_addr("*memory",
				  (ulong)map_to_sysmem((void *)rec->e_memory));
			show_ret(rec_hdr->e_ret);
		}
		break;
	}
	case EFILT_FREE_PAGES: {
		struct efil_free_pages *rec = start;

		show_addr("memory", map_to_sysmem((void *)rec->memory));
		show_ulong("pages", (ulong)rec->pages);
		if (rec_hdr->ended)
			show_ret(rec_hdr->e_ret);
		break;
	}
	case EFILT_ALLOCATE_POOL: {
		struct efil_allocate_pool *rec = start;

		show_enum(allocate_type_name, rec->pool_type);
		show_ulong("size", (ulong)rec->size);
		show_addr("buffer", (ulong)rec->buffer);
		if (rec_hdr->ended) {
			show_addr("*buffer",
				  (ulong)map_to_sysmem((void *)rec->e_buffer));
			show_ret(rec_hdr->e_ret);
		}
		break;
	}
	case EFILT_FREE_POOL: {
		struct efil_free_pool *rec = start;

		show_addr("buffer", map_to_sysmem(rec->buffer));
		if (rec_hdr->ended)
			show_ret(rec_hdr->e_ret);
		break;
	}
	case EFILT_COUNT:
		break;
	}
	printf("\n");
}

int efi_log_show(void)
{
	struct efil_hdr *hdr = bloblist_find(BLOBLISTT_EFI_LOG, 0);
	struct efil_rec_hdr *rec_hdr;
	int i;

	printf("EFI log\n");
	if (!hdr)
		return -ENOENT;
	for (i = 0, rec_hdr = (void *)hdr + sizeof(*hdr);
	     (void *)rec_hdr - (void *)hdr < hdr->upto;
	     i++, rec_hdr = (void *)rec_hdr+ rec_hdr->size)
		show_rec(i, rec_hdr);

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
