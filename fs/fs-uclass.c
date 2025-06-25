// SPDX-License-Identifier: GPL-2.0
/*
 * Implementation of a filesystem, e.g. on a partition
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#define LOG_DEBUG
#define LOG_CATEGORY	UCLASS_FS

#include <bootdev.h>
#include <bootmeth.h>
#include <dm.h>
#include <fs.h>
#include <dm/device-internal.h>

int fs_ls(struct udevice *dev, const char *dirname)
{
	/* not yet implement */

	return -ENOSYS;
}

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

int fs_split_path(const char *fname, char **subdirp, const char **leafp)
{
	char *subdir, *p;

	if (!*fname)
		return log_msg_ret("fsp", -EINVAL);

	/* allocate space for the whole filename, for simplicity */
	subdir = strdup(fname);
	if (!subdir)
		return log_msg_ret("fsp", -ENOMEM);

	p = strrchr(subdir, '/');
	if (p) {
		*leafp = p + 1;
		*p = '\0';
	} else {
		*leafp = fname;
		strcpy(subdir, "/");
	}
	*subdirp = subdir;

	return 0;
}

int fs_lookup_dir(struct udevice *dev, const char *path, struct udevice **dirp)
{
	struct fs_ops *ops = fs_get_ops(dev);

	return ops->lookup_dir(dev, path, dirp);
}

int fs_mount(struct udevice *dev)
{
	struct fs_ops *ops = fs_get_ops(dev);
	int ret;

	ret = ops->mount(dev);
	if (ret)
		return log_msg_ret("fsm", ret);

	ret = bootdev_setup_for_dev(dev, "fs_bootdev");
	if (ret)
		return log_msg_ret("fss", ret);

	return 0;
}

int fs_unmount(struct udevice *dev)
{
	struct fs_ops *ops = fs_get_ops(dev);

	return ops->unmount(dev);
}

static int fs_get_bootflow(struct udevice *dev, struct bootflow_iter *iter,
			   struct bootflow *bflow)
{
	struct udevice *fsdev = dev_get_parent(dev);
	int ret;

	log_debug("get_bootflow fs '%s'\n", fsdev->name);

	ret = bootmeth_check(bflow->method, iter);
	if (ret)
		return log_msg_ret("check", ret);

	return 0;
}

static int fs_bootdev_bind(struct udevice *dev)
{
	struct bootdev_uc_plat *ucp = dev_get_uclass_plat(dev);

	/*
	 * we don't know what priority to give this, so pick something a little
	 * slow for now
	 */
	ucp->prio = BOOTDEVP_3_INTERNAL_SLOW;

	return 0;
}

static int fs_bootdev_hunt(struct bootdev_hunter *info, bool show)
{
	struct udevice *dev;
	int ret;

	/* mount all filesystems, which will create bootdevs for each */
	uclass_foreach_dev_probe(UCLASS_FS, dev) {
		ret = fs_mount(dev);
		if (ret)
			log_warning("Failed to mount filesystem '%s'\n",
				    dev->name);
	}

	return 0;
}

struct bootdev_ops fs_bootdev_ops = {
	.get_bootflow	= fs_get_bootflow,
};

static const struct udevice_id fs_bootdev_ids[] = {
	{ .compatible = "u-boot,bootdev-fs" },
	{ }
};

U_BOOT_DRIVER(fs_bootdev) = {
	.name		= "fs_bootdev",
	.id		= UCLASS_BOOTDEV,
	.ops		= &fs_bootdev_ops,
	.bind		= fs_bootdev_bind,
	.of_match	= fs_bootdev_ids,
};

BOOTDEV_HUNTER(fs_bootdev_hunter) = {
	.prio		= BOOTDEVP_3_INTERNAL_SLOW,
	.uclass		= UCLASS_FS,
	.hunt		= fs_bootdev_hunt,
	.drv		= DM_DRIVER_REF(fs_bootdev),
};

UCLASS_DRIVER(fs) = {
	.name	= "fs",
	.id	= UCLASS_FS,
	.per_device_auto	= sizeof(struct fs_priv),
	.per_device_plat_auto	= sizeof(struct fs_plat),
};
