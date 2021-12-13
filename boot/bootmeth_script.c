// SPDX-License-Identifier: GPL-2.0+
/*
 * Bootmethod for booting via a U-Boot script
 *
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY UCLASS_BOOTSTD

#include <common.h>
#include <blk.h>
#include <bootflow.h>
#include <bootmeth.h>
#include <bootstd.h>
#include <dm.h>
#include <fs.h>
#include <malloc.h>
#include <mapmem.h>

#define SCRIPT_FNAME1	"boot.scr.uimg"
#define SCRIPT_FNAME2	"boot.scr"

static int script_check(struct udevice *dev, struct bootflow_iter *iter)
{
	int ret;

	/* This only works on block devices */
	ret = bootflow_iter_uses_blk_dev(iter);
	if (ret)
		return log_msg_ret("blk", ret);

	return 0;
}

static int try_file(struct bootflow *bflow, struct blk_desc *desc,
		    const char *prefix, const char *fname)
{
	char path[200];
	loff_t size;
	int ret, ret2;

	snprintf(path, sizeof(path), "%s%s", prefix ? prefix : "", fname);

	ret = fs_size(path, &size);
	log_debug("   %s - err=%d\n", path, ret);

	/* Sadly FS closes the file after fs_size() so we must * redo this */
	ret2 = fs_set_blk_dev_with_part(desc, bflow->part);
	if (ret2)
		return log_msg_ret("set", ret2);
	if (ret)
		return log_msg_ret("size", ret);

	bflow->fname = strdup(path);
	if (!bflow->fname)
		return log_msg_ret("name", -ENOMEM);
	bflow->size = size;
	bflow->state = BOOTFLOWST_FILE;

	return 0;
}

static int script_read_bootflow(struct udevice *dev, struct bootflow *bflow)
{
	struct blk_desc *desc = dev_get_uclass_plat(bflow->blk);
	const char *const *prefixes;
	struct udevice *bootstd;
	loff_t bytes_read;
	ulong addr;
	uint size;
	int ret, i;
	char *buf;

	ret = uclass_first_device_err(UCLASS_BOOTSTD, &bootstd);
	if (ret)
		return log_msg_ret("std", ret);

	/* We require a partition table */
	if (!bflow->part)
		return -ENOENT;

	prefixes = bootstd_get_prefixes(bootstd);
	i = 0;
	do {
		const char *prefix = prefixes ? prefixes[i] : NULL;

		ret = try_file(bflow, desc, prefix, SCRIPT_FNAME1);
		if (ret)
			ret = try_file(bflow, desc, prefix, SCRIPT_FNAME2);
	} while (prefixes && prefixes[++i]);
	if (ret)
		return log_msg_ret("try", ret);

	size = bflow->size;
	log_debug("   - script file size %x\n", size);
	if (size > 0x10000)
		return log_msg_ret("chk", -E2BIG);

	buf = malloc(size + 1);
	if (!buf)
		return log_msg_ret("buf", -ENOMEM);
	addr = map_to_sysmem(buf);

	ret = fs_read(bflow->fname, addr, 0, 0, &bytes_read);
	if (ret) {
		free(buf);
		return log_msg_ret("read", ret);
	}
	if (size != bytes_read)
		return log_msg_ret("bread", -EINVAL);
	buf[size] = '\0';
	bflow->state = BOOTFLOWST_READY;
	bflow->buf = buf;

	return 0;
}

static int script_read_file(struct udevice *dev, struct bootflow *bflow,
			    const char *file_path, ulong addr, ulong *sizep)
{
	struct blk_desc *desc = dev_get_uclass_plat(bflow->blk);
	loff_t len_read;
	loff_t size;
	int ret;

	ret = fs_set_blk_dev_with_part(desc, bflow->part);
	if (ret)
		return log_msg_ret("set1", ret);
	ret = fs_size(file_path, &size);
	if (ret)
		return log_msg_ret("size", ret);
	if (size > *sizep)
		return log_msg_ret("spc", -ENOSPC);

	ret = fs_set_blk_dev_with_part(desc, bflow->part);
	if (ret)
		return log_msg_ret("set2", ret);
	ret = fs_read(file_path, addr, 0, 0, &len_read);
	if (ret)
		return ret;
	*sizep = len_read;

	return 0;
}

static int script_boot(struct udevice *dev, struct bootflow *bflow)
{
	return 0;
}

static int script_bootmeth_bind(struct udevice *dev)
{
	struct bootmeth_uc_plat *plat = dev_get_uclass_plat(dev);

	plat->desc = IS_ENABLED(CONFIG_BOOTSTD_FULL) ?
		"Script boot from a block device" : "script";

	return 0;
}

static struct bootmeth_ops script_bootmeth_ops = {
	.check		= script_check,
	.read_bootflow	= script_read_bootflow,
	.read_file	= script_read_file,
	.boot		= script_boot,
};

static const struct udevice_id script_bootmeth_ids[] = {
	{ .compatible = "u-boot,script" },
	{ }
};

U_BOOT_DRIVER(bootmeth_script) = {
	.name		= "bootmeth_script",
	.id		= UCLASS_BOOTMETH,
	.of_match	= script_bootmeth_ids,
	.ops		= &script_bootmeth_ops,
	.bind		= script_bootmeth_bind,
};
