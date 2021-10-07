// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <blk.h>
#include <bootdevice.h>
#include <bootflow.h>
#include <bootmethod.h>
#include <distro.h>
#include <dm.h>
#include <bm_efi.h>
#include <fs.h>
#include <log.h>
#include <malloc.h>
#include <part.h>
#include <dm/lists.h>
#include <dm/uclass-internal.h>

enum {
	/*
	 * Set some sort of limit on the number of partitions a bootdevice can
	 * have. Note that for disks this limits the partitions numbers that
	 * are scanned to 1..MAX_BOOTFLOWS_PER_BOOTDEVICE
	 */
	MAX_PART_PER_BOOTDEVICE		= 30,
};

int bootdevice_get_state(struct bootdevice_state **statep)
{
	struct uclass *uc;
	int ret;

	ret = uclass_get(UCLASS_BOOTDEVICE, &uc);
	if (ret)
		return ret;
	*statep = uclass_get_priv(uc);

	return 0;
}

void bootdevice_clear_bootflows(struct udevice *dev)
{
	struct bootdevice_uc_plat *ucp = dev_get_uclass_plat(dev);

	while (!list_empty(&ucp->bootflow_head)) {
		struct bootflow *bflow;

		bflow = list_first_entry(&ucp->bootflow_head, struct bootflow,
					 bm_node);
		bootflow_remove(bflow);
	}
}

void bootdevice_clear_glob(void)
{
	struct bootdevice_state *state;

	if (bootdevice_get_state(&state))
		return;

	while (!list_empty(&state->glob_head)) {
		struct bootflow *bflow;

		bflow = list_first_entry(&state->glob_head, struct bootflow,
					 glob_node);
		bootflow_remove(bflow);
	}
}

int bootdevice_add_bootflow(struct bootflow *bflow)
{
	struct bootdevice_uc_plat *ucp = dev_get_uclass_plat(bflow->dev);
	struct bootdevice_state *state;
	struct bootflow *new;
	int ret;

	assert(bflow->dev);
	ret = bootdevice_get_state(&state);
	if (ret)
		return ret;

	new = malloc(sizeof(*bflow));
	if (!new)
		return log_msg_ret("bflow", -ENOMEM);
	memcpy(new, bflow, sizeof(*bflow));

	list_add_tail(&new->glob_node, &state->glob_head);
	list_add_tail(&new->bm_node, &ucp->bootflow_head);

	return 0;
}

int bootdevice_first_bootflow(struct udevice *dev, struct bootflow **bflowp)
{
	struct bootdevice_uc_plat *ucp = dev_get_uclass_plat(dev);

	if (list_empty(&ucp->bootflow_head))
		return -ENOENT;

	*bflowp = list_first_entry(&ucp->bootflow_head, struct bootflow,
				   bm_node);

	return 0;
}

int bootdevice_next_bootflow(struct bootflow **bflowp)
{
	struct bootflow *bflow = *bflowp;
	struct bootdevice_uc_plat *ucp = dev_get_uclass_plat(bflow->dev);

	*bflowp = NULL;

	if (list_is_last(&bflow->bm_node, &ucp->bootflow_head))
		return -ENOENT;

	*bflowp = list_entry(bflow->bm_node.next, struct bootflow, bm_node);

	return 0;
}

int bootdevice_bind(struct udevice *parent, const char *drv_name,
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

int bootdevice_find_in_blk(struct udevice *dev, struct udevice *blk,
			   struct bootflow_iter *iter, struct bootflow *bflow)
{
	struct blk_desc *desc = dev_get_uclass_plat(blk);
	struct disk_partition info;
	char name[60];
	int ret;

	/* Sanity check */
	if (iter->part >= MAX_PART_PER_BOOTDEVICE)
		return log_msg_ret("max", -ESHUTDOWN);

	bflow->blk = blk;
	snprintf(name, sizeof(name), "%s.part_%x", dev->name, iter->part);
	bflow->name = strdup(name);
	if (!bflow->name)
		return log_msg_ret("name", -ENOMEM);

	bflow->state = BOOTFLOWST_BASE;
	bflow->part = iter->part;

	/*
	 * partition numbers start at 0 so this cannot succeed, but it can tell
	 * us whether there is valid media there
	 */
	ret = part_get_info(desc, iter->part, &info);
	if (!iter->part && ret == -EPROTONOSUPPORT)
		ret = 0;

	/*
	 * This error indicates the media is not present. Otherwise we just
	 * blindly scan the next partition. We could be more intelligent here
	 * and check which partition numbers actually exist.
	 */
	if (ret == -EOPNOTSUPP)
		ret = -ESHUTDOWN;
	else
		bflow->state = BOOTFLOWST_MEDIA;
	if (ret)
		return log_msg_ret("part", ret);

	/*
	 * Currently we don't get the number of partitions, so just
	 * assume a large number
	 */
	iter->max_part = MAX_PART_PER_BOOTDEVICE;

	if (iter->part) {
		ret = fs_set_blk_dev_with_part(desc, bflow->part);
		bflow->state = BOOTFLOWST_PART;
#ifdef CONFIG_DOS_PARTITION
		log_debug("%s: Found partition %x type %x fstype %d\n",
			  blk->name, bflow->part, info.sys_ind,
			  ret ? -1 : fs_get_type());
#endif
		if (ret)
			return log_msg_ret("fs", ret);
		bflow->state = BOOTFLOWST_FS;
	}

	ret = bootmethod_read_bootflow(bflow->method, bflow);
	if (ret)
		return log_msg_ret("method", ret);

	return 0;
}

void bootdevice_list(bool probe)
{
	struct udevice *dev;
	int ret;
	int i;

	printf("Seq  Probed  Status  Uclass    Name\n");
	printf("---  ------  ------  --------  ------------------\n");
	if (probe)
		ret = uclass_first_device_err(UCLASS_BOOTDEVICE, &dev);
	else
		ret = uclass_find_first_device(UCLASS_BOOTDEVICE, &dev);
	for (i = 0; dev; i++) {
		printf("%3x   [ %c ]  %6s  %-9.9s %s\n", dev_seq(dev),
		       device_active(dev) ? '+' : ' ',
		       ret ? simple_itoa(ret) : "OK",
		       dev_get_uclass_name(dev_get_parent(dev)), dev->name);
		if (probe)
			ret = uclass_next_device_err(&dev);
		else
			ret = uclass_find_next_device(&dev);
	}
	printf("---  ------  ------  --------  ------------------\n");
	printf("(%d device%s)\n", i, i != 1 ? "s" : "");
}

int bootdevice_setup_for_dev(struct udevice *parent, const char *drv_name)
{
	struct udevice *bm;
	int ret;

	if (!CONFIG_IS_ENABLED(BOOTDEVICE))
		return 0;

	ret = device_find_first_child_by_uclass(parent, UCLASS_BOOTDEVICE,
						&bm);
	if (ret) {
		if (ret != -ENODEV) {
			log_debug("Cannot access bootdevice device\n");
			return ret;
		}

		ret = bootdevice_bind(parent, drv_name, "bootdevice", &bm);
		if (ret) {
			log_debug("Cannot create bootdevice device\n");
			return ret;
		}
	}

	return 0;
}
