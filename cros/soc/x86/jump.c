// SPDX-License-Identifier: GPL-2.0
/*
 * Jumping from SPL to U-Boot proper
 *
 * Copyright (c) 2018 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <os.h>
#include <spl.h>
#include <cros/fwstore.h>
#include <cros/vboot.h>

// #define USE_RAM
#
int fwstore_jump(struct vboot_info *vboot, int offset, int size)
{
	struct spl_image_info *spl_image = vboot->spl_image;
#ifdef USE_RAM
	uint32_t addr = CONFIG_SYS_TEXT_BASE;
	char *buf = (char *)(ulong)addr;
	int ret;
#else
	uint32_t addr = offset - CONFIG_ROM_SIZE;
#endif

	vboot_log(LOGL_WARNING, "Reading firmware offset %x (addr %x, size %x)\n",
		  offset, addr, size);
#ifdef USE_RAM
	ret = cros_fwstore_read(vboot->fwstore, offset, size, buf);
	if (ret)
		return log_msg_ret("Read fwstore", ret);
#endif
	spl_image->size = CONFIG_SYS_MONITOR_LEN;
	spl_image->entry_point = addr;
	spl_image->load_addr = addr;
	spl_image->os = IH_OS_U_BOOT;
	spl_image->name = "U-Boot";

	return 0;
}
