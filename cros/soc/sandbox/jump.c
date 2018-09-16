// SPDX-License-Identifier: GPL-2.0
/*
 * Jumping from SPL to U-Boot proper
 *
 * Copyright (c) 2018 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 */

#include <common.h>
#include <os.h>
#include <cros/fwstore.h>
#include <cros/vboot.h>

int fwstore_jump(struct vboot_info *vboot, int offset, int size)
{
	char *buf;
	int ret;

	buf = os_malloc(size);
	if (!buf)
		return log_msg_ret("Allocate fwstore space", -ENOMEM);
	vboot_log(LOGL_INFO, "Reading firmware offset %x, size %x\n", offset,
		  size);
	ret = cros_fwstore_read(vboot->fwstore, offset, size, buf);
	if (ret)
		return log_msg_ret("Read fwstore", ret);
	ret = os_jump_to_image(buf, size);
	if (ret)
		return log_msg_ret("Jump to firmware", ret);

	return 0;
}
