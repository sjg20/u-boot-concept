// SPDX-License-Identifier: GPL-2.0
/*
 * Implementation of a filesystem, e.g. on a partition
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#include <dm.h>
#include <fs.h>
#include <dm/device-internal.h>

int fs_get_by_name(const char *name, struct udevice **devp)
{
	struct udevice *dev;
	struct uclass *uc;

	uclass_id_foreach_dev(UCLASS_FS, dev, uc) {
		struct fs_plat *uc_plat = dev_get_uclass_plat(dev);

		if (!strcmp(name, uc_plat->name)) {
			int ret;

			ret = device_probe(dev);
			if (ret)
				return log_msg_ret("fgn", ret);
			*devp = dev;

			return 0;
		}
	}

	return -ENOENT;
}

UCLASS_DRIVER(fs) = {
	.name	= "fs",
	.id	= UCLASS_FS,
	.per_device_plat_auto	= sizeof(struct fs_plat),
};
