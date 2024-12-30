// SPDX-License-Identifier: GPL-2.0+
/*
 * UPL handoff parsing
 *
 * Copyright 2024 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY UCLASS_BOOTSTD

#include <bootstage.h>
#include <cpu.h>
#include <display_options.h>
#include <dm.h>
#include <image.h>
#include <log.h>
#include <mapmem.h>
#include <serial.h>
#include <upl.h>
#include <video.h>
#include <asm/global_data.h>
#include <dm/root.h>

DECLARE_GLOBAL_DATA_PTR;

struct upl *cur_upl;

static void upl_prepare(const struct upl_image *img, const struct abuf *buf)
{
	printf("\nUPL: handoff at %lx size %zx\n", abuf_addr(buf), buf->size);
	printf("Starting at %lx ...\n\n", img->entry);

	bootstage_mark_name(BOOTSTAGE_ID_BOOTM_HANDOFF, "upl_prepare");
	if (IS_ENABLED(CONFIG_BOOTSTAGE_REPORT))
		bootstage_report();

	/*
	 * Call remove function of all devices with a removal flag set.
	 * This may be useful for last-stage operations, like cancelling
	 * of DMA operation or releasing device internal buffers.
	 */
	dm_remove_devices_active();
}

int _upl_add_image(int node, ulong load_addr, ulong size, const char *desc)
{
	struct upl_image img;

	if (!cur_upl)
		return 0;

	img.load = load_addr;
	img.size = size;
	img.offset = node;
	img.description = desc;
	if (!alist_add(&cur_upl->image, img))
		return -ENOMEM;
	log_debug("upl: add image %s at %lx size %lx\n", desc, load_addr, size);

	return 0;
}

int upl_exec(ulong addr)
{
	struct bootm_headers images;
	struct upl s_upl, *upl = &s_upl;
	struct upl_image *img;
	ulong img_data, img_len;
	const char *uname;
	struct abuf buf;
	int ret;

	upl_init(upl);

	cur_upl = upl;
	uname = NULL;
	memset(&images, '\0', sizeof(images));
	images.fit_uname_cfg = "conf-1";

	/* a side-effect of this function is to call _upl_add_image() above */
	ret = fit_image_load(&images, addr, &uname, &images.fit_uname_cfg,
			     IH_ARCH_DEFAULT, IH_TYPE_FIRMWARE,
			     BOOTSTAGE_ID_FIT_KERNEL_START,
			     FIT_LOAD_OPTIONAL_NON_ZERO, &img_data, &img_len);
	if (ret < 0) {
		cur_upl = NULL;
		return log_msg_retz("ufi", ret);
	}

	images.fit_hdr_os = map_sysmem(addr, 0);
	img = alist_getw(&upl->image, 0, struct upl_image);
	if (!img)
		return log_msg_ret("uim", ret);

	if (fit_image_get_entry(images.fit_hdr_os, ret, &img->entry))
		return log_msg_ret("uae", -ENOENT);
	log_debug("entry %lx\n", img->entry);

	/* this calls _upl_add_image() with each loaded image */
	ret = boot_get_loadable(&images);
	cur_upl = NULL;
	if (ret)
		return log_msg_ret("ulo", ret);

	/* we now have a full handoff state, including the images */
	ret = upl_create_handoff(upl, &buf);
	if (ret)
		return log_msg_ret("uec", ret);

	if (_DEBUG) {
		set_working_fdt_addr(abuf_addr(&buf));
		run_command("fdt print", 0);
	}

	upl_prepare(img, &buf);

	if (IS_ENABLED(CONFIG_X86)) {
		ret = arch_upl_jump(img->entry, &buf);
	} else {
		printf("UPL is not supported on this architecture\n");
		ret = -ENOSYS;
	}

	return ret;
}
