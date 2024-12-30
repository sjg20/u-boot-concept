// SPDX-License-Identifier: GPL-2.0+
/*
 * UPL handoff parsing
 *
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY UCLASS_BOOTSTD

#include <alist.h>
#include <bloblist.h>
#include <dm.h>
#include <image.h>
#include <spl.h>
#include <upl.h>

struct upl s_upl;

void upl_set_fit_addr(ulong fit)
{
	struct upl *upl = &s_upl;

	upl->fit.base = fit;
}

void upl_set_fit_info(ulong fit, int conf_offset, ulong entry_addr)
{
	struct upl *upl = &s_upl;

	upl->fit.base = fit;
	upl->conf_offset = conf_offset;
	log_debug("upl: add fit %lx conf %x\n", fit, conf_offset);
}

int _upl_add_image(int node, ulong load_addr, ulong size, const char *desc)
{
	struct upl *upl = &s_upl;
	struct upl_image img;

	memset(&img, '\0', sizeof(img));
	img.reg.base = load_addr;
	img.reg.size = size;
	img.offset = node;
	img.description = desc;
	if (!alist_add(&upl->image, img))
		return -ENOMEM;
	log_debug("upl: add image %s at %lx size %lx\n", desc, load_addr, size);

	return 0;
}

int spl_write_upl_handoff(void)
{
	struct upl s_upl, *upl = &s_upl;
	struct abuf buf;
	void *ptr;
	int ret;

	log_debug("UPL: Writing handoff - image_count=%d\n", upl->image.count);

	ret = upl_write_to_buf(upl, ofnode_root(), &buf);
	if (ret)
		return log_msg_ret("wuh", ret);

	ptr = bloblist_add(BLOBLISTT_CONTROL_FDT, abuf_size(&buf), 0);
	if (!ptr)
		return log_msg_ret("blo", -ENOENT);
	memcpy(ptr, abuf_data(&buf), abuf_size(&buf));
	abuf_uninit(&buf);

	return 0;
}

void spl_upl_init(void)
{
	upl_init(&s_upl);
}
