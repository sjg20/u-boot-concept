// SPDX-License-Identifier: GPL-2.0+
/*
 * Support for tiny device (those without a fully uclass and driver)
 *
 * Copyright 2020 Google LLC
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <malloc.h>

struct tinydev *tiny_dev_find(enum uclass_id uclass_id, int seq)
{
	struct tinydev *info = ll_entry_start(struct tinydev, tiny_dev);
	const int n_ents = ll_entry_count(struct tinydev, tiny_dev);
	struct tinydev *entry;

	log_debug("find %d seq %d: n_ents=%d\n", uclass_id, seq, n_ents);
	for (entry = info; entry != info + n_ents; entry++) {
		struct tiny_drv *drv = entry->drv;

		log_debug("- entry %p, uclass %d %d\n", entry,
			  drv->uclass_id, uclass_id);
		if (drv->uclass_id == uclass_id) {
			if (CONFIG_IS_ENABLED(TINY_RELOC)) {
				struct tinydev *copy;

				copy = malloc(sizeof(*copy));
				if (!copy)
					return NULL;
				memcpy(copy, entry, sizeof(*copy));
				return copy;
			}
			return entry;
		}
	}

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
