// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bootdevice.h>
#include <bootflow.h>
#include <bootmethod.h>
#include <dm.h>
#include <malloc.h>

static const char *const bootflow_state[BOOTFLOWST_COUNT] = {
	"base",
	"media",
	"part",
	"fs",
	"file",
	"loaded",
};

const char *bootflow_state_get_name(enum bootflow_state_t state)
{
	if (state < 0 || state >= BOOTFLOWST_COUNT)
		return "?";

	return bootflow_state[state];
}

int bootflow_first_glob(struct bootflow **bflowp)
{
	struct bootdevice_state *state;
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
	struct bootdevice_state *state;
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

void bootflow_reset_iter(struct bootflow_iter *iter, int flags)
{
	memset(iter, '\0', sizeof(*iter));
	iter->flags = flags;
}

static void bootflow_iter_set_dev(struct bootflow_iter *iter,
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
 * @return 0 if OK, -ESHUTDOWN if there are no more in this bootdevice
 */
static int iter_incr(struct bootflow_iter *iter)
{
	int ret;

	/* Get the next boothmethod */
	ret = uclass_next_device_err(&iter->method);
	if (!ret)
		return 0;

	/* No more bootmethods; start at the first one, and... */
	ret = uclass_first_device_err(UCLASS_BOOTMETHOD, &iter->method);
	if (ret)
		return -ESHUTDOWN;  /* should not happen, but just in case */

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
 * bootflow_check() - Check if a bootflow can be obtained
 *
 * @iter: Provides hwpart, part, method to get
 * @bflow: Bootflow to update on success
 * @return 0 if OK, -NOTTY if there is nothing there (try the next partition),
 *	-ENOSYS if there is no bootflow support on this device, -ESHUTDOWN if
 *	there are no more bootflows on this bootdevice, so the next bootdevice
 *	should be tried
 */
static int bootflow_check(struct bootflow_iter *iter, struct bootflow *bflow)
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
		return -ENOTTY;
	}
	if (ret)
		return log_msg_ret("check", ret);

	return 0;
}

int bootflow_scan_first(struct bootflow_iter *iter, int flags,
			struct bootflow *bflow)
{
	struct udevice *dev;
	int ret;

	bootflow_reset_iter(iter, flags);
	ret = uclass_first_device_err(UCLASS_BOOTDEVICE, &dev);
	if (ret)
		return ret;
	bootflow_iter_set_dev(iter, dev);

	/* Find the first bootmethod (there must be at least one!) */
	ret = uclass_first_device_err(UCLASS_BOOTMETHOD, &iter->method);
	if (ret)
		return log_msg_ret("meth", ret);

	ret = bootflow_check(iter, bflow);
	if (ret) {
		if (ret != -ESHUTDOWN && ret != -ENOSYS &&
		    ret != -ENOTTY) {
			if (iter->flags & BOOTFLOWF_ALL)
				return log_msg_ret("all", ret);
		}
		iter->err = ret;
		ret = bootflow_scan_next(iter, bflow);
		if (ret)
			return log_msg_ret("get", ret);
	}

	return 0;
}

int bootflow_scan_next(struct bootflow_iter *iter, struct bootflow *bflow)
{
	struct udevice *dev;
	int ret;

	do {
		ret = iter_incr(iter);
		if (ret && ret != -ESHUTDOWN)
			return ret;

		if (!ret) {
			ret = bootflow_check(iter, bflow);
			if (!ret)
				return 0;
			if (ret != -ESHUTDOWN && ret != -ENOSYS &&
			    ret != -ENOTTY) {
				if (iter->flags & BOOTFLOWF_ALL)
					return log_msg_ret("all", ret);

				/* try the next one */
				continue;
			}
		}

		/* we got to the end of that bootdevice, try the next */
		dev = iter->dev;
		ret = uclass_next_device_err(&dev);
		/* if there are no more bootdevices, give up */
		if (ret)
			return log_msg_ret("next", ret);

		bootflow_iter_set_dev(iter, dev);
	} while (1);
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

int bootflow_boot(struct bootflow *bflow)
{
	int ret;

	if (bflow->state != BOOTFLOWST_LOADED)
		return log_msg_ret("load", -EPROTO);

	ret = bootmethod_boot(bflow->method, bflow);
	if (ret)
		return log_msg_ret("method", ret);

	if (ret)
		return log_msg_ret("boot", ret);

	/*
	 * internal error, should not get here since we should have booted
	 * something or returned an error
	 */

	return log_msg_ret("end", -EFAULT);
}
