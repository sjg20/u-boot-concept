// SPDX-License-Identifier: GPL-2.0+
/*
 * Utility functions
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#include <bootctl.h>
#include <dm.h>
#include <log.h>
#include <stdarg.h>
#include <vsprintf.h>
#include "util.h"
#include "display.h"
#include "oslist.h"

/**
 * bc_printf() - Print a string to the display
 *
 * @fmt: printf() format string for log record
 * @...: Optional parameters, according to the format string @fmt
 * Return: Number of characters emitted
 */
int bc_printf(struct udevice *disp, const char *fmt, ...)
{
	struct bc_ui_ops *ops = bc_ui_get_ops(disp);
	char buf[CONFIG_SYS_CBSIZE];
	va_list args;
	int count;

	va_start(args, fmt);
	count = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	LOGR("bpp", ops->print(disp, buf));

	return count;
}

int bc_ui_show(struct udevice *disp)
{
	struct bc_ui_ops *ops = bc_ui_get_ops(disp);

	LOGR("bds", ops->show(disp));

	return 0;
}

int bc_ui_add(struct udevice *dev, struct osinfo *info)
{
	struct bc_ui_ops *ops = bc_ui_get_ops(dev);

	LOGR("bda", ops->add(dev, info));

	return 0;
}

int bc_ui_render(struct udevice *disp)
{
	struct bc_ui_ops *ops = bc_ui_get_ops(disp);

	LOGR("bds", ops->render(disp));

	return 0;
}

int bc_ui_poll(struct udevice *disp, struct osinfo **infop)
{
	struct bc_ui_ops *ops = bc_ui_get_ops(disp);

	LOGR("bdp", ops->poll(disp, infop));

	return 0;
}

int bc_oslist_first(struct udevice *dev, struct oslist_iter *iter,
		    struct osinfo *info)
{
	struct bc_oslist_ops *ops = bc_oslist_get_ops(dev);

	LOGR("bol", ops->first(dev, iter, info));

	return 0;
}

int bc_oslist_next(struct udevice *dev, struct oslist_iter *iter,
		   struct osinfo *info)
{
	struct bc_oslist_ops *ops = bc_oslist_get_ops(dev);

	LOGR("bon", ops->next(dev, iter, info));

	return 0;
}
