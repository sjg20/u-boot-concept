// SPDX-License-Identifier: GPL-2.0+
/*
 * Support for tiny device (those without a fully uclass and driver)
 *
 * Copyright 2020 Google LLC
 */

#define LOG_DEBUG
#define LOG_CATEGORY	LOGC_TINYDEV

#include <common.h>
#include <dm.h>
#include <log.h>
#include <malloc.h>

DECLARE_GLOBAL_DATA_PTR;

struct tinydev *tiny_dev_find(enum uclass_id uclass_id, int seq)
{
	struct tinydev *info = ll_entry_start(struct tinydev, tiny_dev);
	const int n_ents = ll_entry_count(struct tinydev, tiny_dev);
	struct tinydev *entry;

	log_debug("find %d seq %d: n_ents=%d\n", uclass_id, seq, n_ents);
	for (entry = info; entry != info + n_ents; entry++) {
		struct tiny_drv *drv = entry->drv;

		log_content("   - entry %p, uclass %d %d\n", entry,
			    drv->uclass_id, uclass_id);
		if (drv->uclass_id == uclass_id) {
			if (CONFIG_IS_ENABLED(TINY_RELOC)) {
				struct tinydev *copy;

				copy = malloc(sizeof(*copy));
				if (!copy)
					return NULL;
				memcpy(copy, entry, sizeof(*copy));
				log_debug("   - found, copied to %p\n", copy);
				return copy;
			}
			log_debug("   - found at %p\n", entry);
			return entry;
		}
	}
	log_debug("   - not found\n");

	return NULL;
}

int tiny_dev_probe(struct tinydev *tdev)
{
	struct tiny_drv *drv;
	int ret;

	if (tdev->flags & DM_FLAG_ACTIVATED)
		return 0;
	drv = tdev->drv;

	if (!tdev->priv && drv->priv_size) {
		tdev->priv = calloc(1, drv->priv_size);
		if (!tdev->priv)
			return -ENOMEM;
	}
	if (drv->probe) {
		ret = drv->probe(tdev);
		if (ret)
			return log_msg_ret("probe", ret);
	}

	tdev->flags |= DM_FLAG_ACTIVATED;

	return 0;
}

struct tinydev *tiny_dev_get(enum uclass_id uclass_id, int seq)
{
	struct tinydev *dev;
	int ret;

	dev = tiny_dev_find(uclass_id, seq);
	if (!dev)
		return NULL;

	ret = tiny_dev_probe(dev);
	if (ret)
		return NULL;

	return dev;
}

struct tinydev *tinydev_from_dev_idx(tinydev_idx_t index)
{
	struct tinydev *start = U_BOOT_TINY_DEVICE_START;

	return start + index;
}

tinydev_idx_t tinydev_to_dev_idx(const struct tinydev *tdev)
{
	struct tinydev *start = U_BOOT_TINY_DEVICE_START;

	return tdev - start;
}

struct tinydev *tinydev_get_parent(const struct tinydev *tdev)
{
	return tinydev_from_dev_idx(tdev->parent);
}

#ifndef CONFIG_SYS_MALLOC_F
#error "Must enable CONFIG_SYS_MALLOC_F with tinydev"
#endif

static void *tinydev_look_data(struct tinydev *tdev, enum dm_data_t type)
{
	struct tinydev_info *info = &((gd_t *)gd)->tinydev_info;
	struct tinydev_data *data;
	int i;
#ifdef TIMYDEV_SHRINK_DATA
	uint idx = tinydev_to_dev_idx(tdev);

	for (i = 0, data = info->data; i < info->data_count; i++) {
		if (data->type == type && data->tdev_idx == idx)
			return malloc_ofs_to_ptr(data->ofs);
	}
#else
	for (i = 0, data = info->data; i < info->data_count; i++) {
		if (data->type == type && data->tdev == tdev)
			return data->ptr;
	}
#endif

	return NULL;
}

void *tinydev_alloc_data(struct tinydev *tdev, enum dm_data_t type, int size)
{
	struct tinydev_info *info = &((gd_t *)gd)->tinydev_info;
	struct tinydev_data *data;
	void *ptr;

	if (info->data_count == ARRAY_SIZE(info->data)) {
		/* To fix this, increase CONFIG_TINYDEV_DATA_MAX_COUNT */
		panic("tinydev data exhusted");
		return NULL;
	}
	data = &info->data[info->data_count];
	ptr = calloc(1, size);
	if (!ptr)
		return NULL;  /* alloc_simple() has already written a message */
	data->type = type;
#ifdef TIMYDEV_SHRINK_DATA
	data->tdev_idx = tinydev_to_dev_idx(tdev);
	data->ofs = malloc_ptr_to_ofs(ptr);
#else
	data->tdev = tdev;
	data->ptr = ptr;
#endif

	return ptr;
}

void *tinydev_ensure_data(struct tinydev *tdev, enum dm_data_t type, int size,
			 bool *existsp)
{
	bool exists = true;
	void *ptr;

	ptr = tinydev_look_data(tdev, type);
	if (!ptr) {
		exists = false;
		ptr = tinydev_alloc_data(tdev, type, size);
	}
	if (existsp)
		*existsp = exists;

	return ptr;
}

void *tinydev_get_data(struct tinydev *tdev, enum dm_data_t type)
{
	void *ptr = tinydev_look_data(tdev, type);

	if (!ptr) {
		log_debug("Cannot found type %d for device %p\n", type, tdev);
		panic("tinydev missing data");
	}

	return ptr;
}

