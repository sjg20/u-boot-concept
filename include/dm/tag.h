/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2021 Linaro Limited
 *			Author: AKASHI Takahiro
 */

#ifndef _DM_TAG_H
#define _DM_TAG_H

#include <linux/list.h>
#include <linux/types.h>

struct udevice;

enum dm_tag_t {
	DM_TAG_EFI = 0,

	DM_TAG_COUNT,
};

/**
 * dmtag_node
 *
 * @sibling: List of dm-tag nodes
 * @dev:     Associated udevice
 * @tag:     Tag type
 * @ptr:     Pointer as a value
 * @val:     Value
 */
struct dmtag_node {
	struct list_head sibling;
	struct  udevice *dev;
	enum dm_tag_t tag;
	union {
		void *ptr;
		ulong val;
	};
};

/**
 * dev_tag_set_ptr()
 *
 */
int dev_tag_set_ptr(struct udevice *dev, enum dm_tag_t tag, void *ptr);

/**
 * dev_tag_set_val()
 *
 */
int dev_tag_set_val(struct udevice *dev, enum dm_tag_t tag, ulong val);

/**
 * dev_tag_get_ptr()
 *
 */
int dev_tag_get_ptr(struct udevice *dev, enum dm_tag_t tag, void **ptrp);

/**
 * dev_tag_get_val()
 *
 */
int dev_tag_get_val(struct udevice *dev, enum dm_tag_t tag, ulong *valp);

/**
 * dev_tag_del()
 *
 */
int dev_tag_del(struct udevice *dev, enum dm_tag_t tag);

/**
 * dev_tag_del_all()
 *
 */
int dev_tag_del_all(struct udevice *dev);

#endif /* _DM_TAG_H */
