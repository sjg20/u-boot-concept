// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <blk.h>
#include <bootmethod.h>
#include <command.h>
#include <dm.h>
#include <fs.h>
#include <log.h>
#include <malloc.h>
#include <mapmem.h>
#include <part.h>
#include <pxe_utils.h>
#include <dm/lists.h>

#define DISTRO_FNAME	"/boot/extlinux/extlinux.conf"

enum {
	/*
	 * Set some sort of limit on the number of bootflows a bootmethod can
	 * return
	 */
	MAX_BOOTFLOWS_PER_BOOTMETHOD	= 100,
};

int bootmethod_get_bootflow(struct udevice *dev, int seq,
			    struct bootflow *bflow)
{
	const struct bootmethod_ops *ops = bootmethod_get_ops(dev);

	if (!ops->get_bootflow)
		return -ENOSYS;

	return ops->get_bootflow(dev, seq, bflow);
}

static int next_bootflow(struct udevice *dev, int seq, struct bootflow *bflow)
{
	int ret;

	ret = bootmethod_get_bootflow(dev, seq, bflow);
	if (ret)
		return ret;

	return 0;
}

int bootmethod_first_bootflow(struct bootmethod_iter *iter, int flags,
			      struct bootflow *bflow)
{
	struct udevice *dev;
	int ret;

	iter->flags = flags;
	iter->seq = 0;
	ret = uclass_first_device_err(UCLASS_BOOTMETHOD, &dev);
	if (ret)
		return ret;
	iter->dev = dev;

	ret = bootmethod_next_bootflow(iter, bflow);
	if (ret)
		return ret;

	return 0;
}

int bootmethod_next_bootflow(struct bootmethod_iter *iter,
			     struct bootflow *bflow)
{
	int ret;

	do {
		ret = next_bootflow(iter->dev, iter->seq, bflow);

		/* If we got a valid bootflow, return it */
		if (!ret) {
			log_info("Bootmethod '%s' seq %d: Found bootflow\n",
				 iter->dev->name, iter->seq);
			return 0;
		}

		/* If we got some other error, try the next partition */
		else if (ret != -ESHUTDOWN) {
			log_debug("Bootmethod '%s' seq %d: Error %d\n",
				  iter->dev->name, iter->seq, ret);
			if (iter->seq++ == MAX_BOOTFLOWS_PER_BOOTMETHOD)
				return log_msg_ret("max", -E2BIG);
		}

		/* we got to the end of that bootmethod, try the next */
		ret = uclass_next_device_err(&iter->dev);

		/* if there are no more bootmethods, give up */
		if (ret)
			return ret;

		/* start at the beginning of this bootmethod */
		iter->seq = 0;
	} while (1);
}

int bootmethod_bind(struct udevice *parent, const char *drv_name,
		    const char *name, struct udevice **devp)
{
	struct udevice *dev;
	char dev_name[30];
	char *str;
	int ret;

	snprintf(dev_name, sizeof(dev_name), "%s.%s", parent->name, name);
	str = strdup(dev_name);
	if (!str)
		return -ENOMEM;
	ret = device_bind_driver(parent, drv_name, str, &dev);
	if (ret)
		return ret;
	*devp = dev;

	return 0;
}

static int disto_getfile(struct pxe_context *ctx, const char *file_path,
			 char *file_addr)
{
	//TODO

	return 0;
}

static int distro_boot(struct blk_desc *desc, int partnum)
{
	struct cmd_tbl cmdtp = {};	/* dummy */
	struct pxe_context ctx;
	loff_t size, bytes_read;
	ulong addr;
	void *buf;
	int ret;

	ret = fs_size(DISTRO_FNAME, &size);
	if (ret)
		return log_msg_ret("size", ret);
	log_info("   - distro file size %x\n", (uint)size);
	if (size > 0x10000)
		return log_msg_ret("chk", -E2BIG);

	// FIXME: FS closes the file after fs_size()
	ret = fs_set_blk_dev_with_part(desc, partnum);
	if (ret)
		return log_msg_ret("set", ret);

	buf = malloc(size);
	if (!buf)
		return log_msg_ret("buf", -ENOMEM);
	addr = map_to_sysmem(buf);

	ret = fs_read(DISTRO_FNAME, addr, 0, 0, &bytes_read);
	printf("read %d %lx\n", ret, addr);
	if (ret)
		return log_msg_ret("read", ret);
	if (size != bytes_read)
		return log_msg_ret("bread", -EINVAL);

	pxe_setup_ctx(&ctx, &cmdtp, disto_getfile, NULL, true);

	ret = pxe_process(&ctx, addr, false);
	if (ret)
		return log_msg_ret("bread", -EINVAL);

	return 0;
}

int bootmethod_find_in_blk(struct udevice *blk, int seq, struct bootflow *bflow)
{
	struct blk_desc *desc = dev_get_uclass_plat(blk);
	struct disk_partition info;
	int partnum = seq + 1;
	int ret;

	ret = part_get_info(desc, partnum, &info);
	if (ret)
		return log_msg_ret("part", ret);
	ret = fs_set_blk_dev_with_part(desc, partnum);
	log_info("%s: Found partition %x type %x fstype %d\n", blk->name,
		 partnum, info.sys_ind, ret ? -1 : fs_get_type());
	if (ret)
		return log_msg_ret("fs", ret);

	if (CONFIG_IS_ENABLED(BOOTMETHOD_DISTRO)) {
		ret = distro_boot(desc, partnum);
		printf("ret=%d\n", ret);
		if (ret)
			return log_msg_ret("distro", ret);
	}

	return 0;
}

UCLASS_DRIVER(bootmethod) = {
	.id		= UCLASS_BOOTMETHOD,
	.name		= "bootmethod",
	.flags		= DM_UC_FLAG_SEQ_ALIAS,
	.per_device_auto	= sizeof(struct bootmethod_priv),
};
