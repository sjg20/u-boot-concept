/*
 * Copyright (c) 2011 The Chromium OS Authors.
 * (C) Copyright 2008
 * Graeme Russ, graeme.russ@gmail.com.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/*
 * board/config.h - configuration options, board specific
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <configs/x86-common.h>
#include <configs/x86-chromebook.h>

#undef CONFIG_CFB_CONSOLE

#undef CONFIG_STD_DEVICES_SETTINGS
#define CONFIG_STD_DEVICES_SETTINGS     "stdin=usbkbd,i8042-kbd,serial\0" \
					"stdout=vidconsole,serial\0" \
					"stderr=vidconsole,serial\0"

#define CONFIG_ENV_SECT_SIZE		0x1000
#define CONFIG_ENV_OFFSET		0x003f8000

#define CONFIG_USB_XHCI_HCD
#define CONFIG_USB_XHCI
#define CONFIG_SYS_USB_XHCI_MAX_ROOT_PORTS	16
#define CONFIG_SYS_CACHELINE_SIZE		32

#undef CONFIG_BOOTCOMMAND
#define CONFIG_BOOTCOMMANDx	\
	"usb start"

#endif	/* __CONFIG_H */
