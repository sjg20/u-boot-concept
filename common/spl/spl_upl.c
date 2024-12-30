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

DECLARE_GLOBAL_DATA_PTR;

struct upl s_upl;

void upl_set_fit_addr(ulong fit)
{
	struct upl *upl = &s_upl;

	upl->fit = fit;
}

void upl_set_fit_info(ulong fit, int conf_offset, ulong entry_addr)
{
	struct upl *upl = &s_upl;

	upl->fit = fit;
	upl->conf_offset = conf_offset;
	log_debug("upl: add fit %lx conf %x\n", fit, conf_offset);
}

int _upl_add_image(int node, ulong load_addr, ulong size, const char *desc)
{
	struct upl *upl = &s_upl;
	struct upl_image img;

	img.load = load_addr;
	img.size = size;
	img.offset = node;
	img.description = desc;
	if (!alist_add(&upl->image, img))
		return -ENOMEM;
	log_debug("upl: add image %s at %lx size %lx\n", desc, load_addr, size);

	return 0;
}

int spl_write_upl_handoff(void)
{
	struct upl *upl = &s_upl;
	ulong addr, size;
	struct abuf buf;
	ofnode root;
	void *ptr;
	int ret;

	log_debug("UPL: Writing handoff - image_count=%d\n", upl->image.count);
	upl->addr_cells = IS_ENABLED(CONFIG_PHYS_64BIT) ? 2 : 1;
	upl->size_cells = IS_ENABLED(CONFIG_PHYS_64BIT) ? 2 : 1;
	upl->bootmode = UPLBM_DEFAULT;
	ret = upl_add_serial(&upl->serial);
	if (ret)
		return log_msg_ret("ser", ret);
	ret = upl_add_graphics(&upl->graphics, &addr, &size);
	if (ret && ret != -ENOENT)
		return log_msg_ret("gra", ret);

	root = ofnode_root();
	ret = upl_write_handoff(upl, root, true);
	if (ret)
		return log_msg_ret("wr", ret);

	ret = oftree_to_fdt(oftree_default(), &buf);
	if (ret)
		return log_msg_ret("fdt", ret);
	log_debug("FDT size %zx\n", abuf_size(&buf));

	ptr = bloblist_add(BLOBLISTT_CONTROL_FDT, abuf_size(&buf), 0);
	if (!ptr)
		return log_msg_ret("blo", -ENOENT);
	memcpy(ptr, abuf_data(&buf), abuf_size(&buf));

	return 0;
}

void spl_upl_init(void)
{
	upl_init(&s_upl);
}
