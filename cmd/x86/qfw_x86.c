// SPDX-License-Identifier: GPL-2.0+
/*
 * x86-specific qfw commands
 *
 * Copyright 2025 Simon Glass <sjg@chromium.org>
 */

#include <abuf.h>
#include <command.h>
#include <qfw.h>
#include <asm/e820.h>

int cmd_qfw_e820(struct udevice *dev)
{
	struct abuf tab;
	int ret;

	ret = qfw_get_file(dev, "etc/e820", &tab);
	if (ret)
		return CMD_RET_FAILURE;
	e820_dump(tab.data, tab.size / sizeof(struct e820_entry));
	abuf_uninit(&tab);

	return 0;
}
