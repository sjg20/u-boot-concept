// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <blk.h>
#include <bootdevice.h>
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
	/* So far we only support distroboot and EFI_LOADER */
	MAX_BOOTDEVICES			= 2,

	/*
	 * Set some sort of limit on the number of bootflows a bootdevice can
	 * return. Note that for disks this limits the partitions numbers that
	 * are scanned to 1..MAX_BOOTFLOWS_PER_BOOTDEVICE / MAX_BOOTDEVICES
	 */
	MAX_BOOTFLOWS_PER_BOOTDEVICE	= 20 * MAX_BOOTDEVICES,
};

static const char *const bootdevice_state[BOOTFLOWST_COUNT] = {
	"base",
	"media",
	"part",
	"fs",
	"file",
	"loaded",
};

static const char *const bootdevice_type[BOOTFLOWT_COUNT] = {
	"distro-boot",
	"efi-loader",
};

int bootdevice_get_state(struct bootflow_state **statep)
{
	struct uclass *uc;
	int ret;

	ret = uclass_get(UCLASS_BOOTDEVICE, &uc);
	if (ret)
		return ret;
	*statep = uclass_get_priv(uc);

	return 0;
}

const char *bootdevice_state_get_name(enum bootflow_state_t state)
{
	if (state < 0 || state >= BOOTFLOWST_COUNT)
		return "?";

	return bootdevice_state[state];
}

const char *bootdevice_type_get_name(enum bootflow_type_t type)
{
	if (type < 0 || type >= BOOTFLOWT_COUNT)
		return "?";

	return bootdevice_type[type];
}

void bootflow_free(struct bootflow *bflow)
{
	free(bflow->name);
	free(bflow->subdir);
	free(bflow->fname);
	free(bflow->buf);
}

void bootflow_remove(struct bootflow *bflow)
{
	list_del(&bflow->bm_node);
	list_del(&bflow->glob_node);

	bootflow_free(bflow);
	free(bflow);
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
	struct bootflow_state *state;

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
	struct bootflow_state *state;
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

int bootflow_first_glob(struct bootflow **bflowp)
{
	struct bootflow_state *state;
	int ret;

	ret = bootdevice_get_state(&state);
	if (ret)
		return ret;

	if (list_empty(&state->glob_head))
		return -ENOENT;

	*bflowp = list_first_entry(&state->glob_head, struct bootflow,
				   glob_node);

	return 0;
}

int bootflow_next_glob(struct bootflow **bflowp)
{
	struct bootflow_state *state;
	struct bootflow *bflow = *bflowp;
	int ret;

	ret = bootdevice_get_state(&state);
	if (ret)
		return ret;

	*bflowp = NULL;

	if (list_is_last(&bflow->glob_node, &state->glob_head))
		return -ENOENT;

	*bflowp = list_entry(bflow->glob_node.next, struct bootflow, glob_node);

	return 0;
}

int bootdevice_get_bootflow(struct udevice *dev, struct bootdevice_iter *iter,
			    struct bootflow *bflow)
{
	const struct bootdevice_ops *ops = bootdevice_get_ops(dev);

	if (!ops->get_bootflow)
		return -ENOSYS;
	memset(bflow, '\0', sizeof(*bflow));
	bflow->dev = dev;
	bflow->hwpart = iter->hwpart;
	bflow->part = iter->part;
	bflow->method = iter->method;

	return ops->get_bootflow(dev, bflow);
}

static void bootdevice_iter_set_dev(struct bootdevice_iter *iter,
				    struct udevice *dev)
{
	iter->dev = dev;
	if (iter->flags & BOOTFLOWF_SHOW) {
		if (dev)
			printf("Scanning bootdevice '%s':\n", dev->name);
		else
			printf("No more bootdevices\n");
	}
}

/**
 * iter_incr() - Move to the next item (hwpart, part, method) in the iterator
 *
 * @return 0 if OK, -ENOENT if there are no more in this bootmethod
 */
static int iter_incr(struct bootdevice_iter *iter)
{
	int ret;

	/* Get the next boothmethod */
	ret = uclass_next_device_err(&iter->method);
	if (!ret)
		return 0;

	/* No more bootmethods; start at the first one, and... */
	ret = uclass_first_device_err(UCLASS_BOOTMETHOD, &iter->method);
	if (ret)
		return -ESHUTDOWN;

	/* ...select next hardware partition */
	if (++iter->hwpart <= iter->max_hw_part)
		return 0;

	/* No more hardware partitions; start at the first one and... */
	iter->hwpart = 0;

	/* ...select next partition  */
	if (++iter->part <= iter->max_part)
		return 0;

	/* No more partitions; start at the first one and...*/
	iter->part = 0;

	/* ...select next bootdevice */
	return -ESHUTDOWN;
}


/**
 * bootflow_scan() - Check if a bootflow can be obtained
 *
 *
 */
static int bootflow_check(struct bootdevice_iter *iter, struct bootflow *bflow)
{
	struct udevice *dev;
	int ret;

	dev = iter->dev;
	ret = bootdevice_get_bootflow(dev, iter, bflow);

	/* If we got a valid bootflow, return it */
	if (!ret) {
		log_debug("Bootdevice '%s' hwpart %d part %d method '%s': Found bootflow\n",
			  dev->name, iter->hwpart, iter->part,
			  iter->method->name);
		return 0;
	}

	/*
	 * Unless there are no more partitions or no bootflow support,
	 * try the next partition. If we run out of partitions, fall
	 * through to select the next device.
	 */
	else if (ret != -ESHUTDOWN && ret != -ENOSYS) {
		log_debug("Bootdevice '%s' hwpart %d part %d method '%s': Error %d\n",
			  dev->name, iter->hwpart, iter->part,
			  iter->method->name, ret);
		/*
		 * For 'all' we return all bootflows, even
		 * those with errors
		 */
		if (iter->flags & BOOTFLOWF_ALL)
			return log_msg_ret("all", ret);

		/* Try the next partition */
		return -ENOENT;
	}

	return 0;
}

int bootflow_scan_first(struct bootdevice_iter *iter, int flags,
			struct bootflow *bflow)
{
	struct udevice *dev;
	int ret;

	memset(iter, '\0', sizeof(*iter));
	iter->flags = flags;
	ret = uclass_first_device_err(UCLASS_BOOTDEVICE, &dev);
	if (ret)
		return ret;
	bootdevice_iter_set_dev(iter, dev);

	/* Find the first bootmethod (there must be at least one!) */
	ret = uclass_first_device_err(UCLASS_BOOTMETHOD, &iter->method);
	if (ret)
		return log_msg_ret("meth", ret);

	ret = bootdevice_get_bootflow(dev, iter, bflow);
	if (ret)
		return ret;

	return 0;
}

int bootflow_scan_next(struct bootdevice_iter *iter, struct bootflow *bflow)
{
	struct udevice *dev;
	int ret;

	do {
		ret = iter_incr(iter);
		if (ret && ret != -ESHUTDOWN)
			return ret;

		ret = bootflow_check(iter, bflow);
		if (!ret)
			return 0;

		/* we got to the end of that bootdevice, try the next */
		dev = iter->dev;
		ret = uclass_next_device_err(&dev);
		/* if there are no more bootdevices, give up */
		if (ret)
			return ret;

		bootdevice_iter_set_dev(iter, dev);
	} while (1);
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

int bootdevice_find_in_blk(struct udevice *dev, struct udevice *blk, int seq,
			   struct bootflow *bflow)
{
	struct blk_desc *desc = dev_get_uclass_plat(blk);
	struct disk_partition info;
	bool done = false;
	char name[60];

	/*
	 * TODO(sjg@chromium.org): Add a suitable parameter for the method
	 * number. Needs to consider the renaming suggested in the cover letter
	 */
	int methodnum = seq % MAX_BOOTDEVICES;
	int partnum = seq / MAX_BOOTDEVICES + 1;
	int ret;

	if (seq >= MAX_BOOTFLOWS_PER_BOOTDEVICE)
		return log_msg_ret("max", -ESHUTDOWN);

	bflow->blk = blk;
	bflow->seq = seq;
	snprintf(name, sizeof(name), "%s.part_%x", dev->name, partnum);
	bflow->name = strdup(name);
	if (!bflow->name)
		return log_msg_ret("name", -ENOMEM);

	bflow->state = BOOTFLOWST_BASE;
	ret = part_get_info(desc, partnum, &info);

	/*
	 * This error indicates the media is not present. Otherwise we just
	 * blindly scan the next partition. We could be more intelligent here
	 * and check which partition numbers actually exist.
	 */
	if (ret != -EOPNOTSUPP)
		bflow->state = BOOTFLOWST_MEDIA;
	if (ret)
		return log_msg_ret("part", ret);

	bflow->state = BOOTFLOWST_PART;
	bflow->part = partnum;
	ret = fs_set_blk_dev_with_part(desc, partnum);
#ifdef CONFIG_DOS_PARTITION
	log_debug("%s: Found partition %x type %x fstype %d\n", blk->name,
		  partnum, info.sys_ind, ret ? -1 : fs_get_type());
#endif
	if (ret)
		return log_msg_ret("fs", ret);

	bflow->state = BOOTFLOWST_FS;

	switch (methodnum) {
	case 0:
		if (CONFIG_IS_ENABLED(BOOTDEVICE_DISTRO)) {
			done = true;
			ret = distro_boot_setup(desc, partnum, bflow);
			if (ret)
				return log_msg_ret("distro", ret);
		}
		break;

	case 1:
		if (CONFIG_IS_ENABLED(BOOTDEVICE_EFILOADER)) {
			done = true;
			ret = efiloader_boot_setup(desc, partnum, bflow);
			if (ret)
				return log_msg_ret("efi_loader", ret);
		}
		break;
	}
	if (!done)
		return log_msg_ret("supp", -ENOTSUPP);

	return 0;
}

int bootflow_boot(struct bootflow *bflow)
{
	bool done = false;
	int ret;

	if (bflow->state != BOOTFLOWST_LOADED)
		return log_msg_ret("load", -EPROTO);

	switch (bflow->type) {
	case BOOTFLOWT_DISTRO:
		if (CONFIG_IS_ENABLED(BOOTDEVICE_DISTRO)) {
			done = true;
			ret = distro_boot(bflow);
		}
		break;
	case BOOTFLOWT_EFILOADER:
		if (CONFIG_IS_ENABLED(BOOTDEVICE_EFILOADER)) {
			done = true;
			ret = efiloader_boot(bflow);
		}
		break;
	case BOOTFLOWT_COUNT:
		break;
	}

	if (!done)
		return log_msg_ret("type", -ENOSYS);

	if (ret)
		return log_msg_ret("boot", ret);

	/*
	 * internal error, should not get here since we should have booted
	 * something or returned an error
	 */

	return log_msg_ret("end", -EFAULT);
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

static int bootdevice_init(struct uclass *uc)
{
	struct bootflow_state *state = uclass_get_priv(uc);

	INIT_LIST_HEAD(&state->glob_head);

	return 0;
}

static int bootdevice_post_bind(struct udevice *dev)
{
	struct bootdevice_uc_plat *ucp = dev_get_uclass_plat(dev);

	INIT_LIST_HEAD(&ucp->bootflow_head);

	return 0;
}

UCLASS_DRIVER(bootdevice) = {
	.id		= UCLASS_BOOTDEVICE,
	.name		= "bootdevice",
	.flags		= DM_UC_FLAG_SEQ_ALIAS,
	.priv_auto	= sizeof(struct bootflow_state),
	.per_device_plat_auto	= sizeof(struct bootdevice_uc_plat),
	.init		= bootdevice_init,
	.post_bind	= bootdevice_post_bind,
};
