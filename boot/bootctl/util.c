// SPDX-License-Identifier: GPL-2.0+
/*
 * Utility functions
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#define LOG_CATEGORY	UCLASS_BOOTCTL

#include <alist.h>
#include <bootctl.h>
#include <dm.h>
#include <log.h>
#include <stdarg.h>
#include <vsprintf.h>
#include <bootctl/logic.h>
#include <bootctl/measure.h>
#include <bootctl/oslist.h>
#include <bootctl/state.h>
#include <bootctl/ui.h>
#include <bootctl/util.h>

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
	int ret;

	va_start(args, fmt);
	count = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	ret = ops->print(disp, buf);
	if (ret)
		return log_msg_ret("bpp", ret);

	return count;
}

int bc_ui_show(struct udevice *disp)
{
	struct bc_ui_ops *ops = bc_ui_get_ops(disp);
	int ret;

	ret = ops->show(disp);
	if (ret)
		return log_msg_ret("bds", ret);

	return 0;
}

int bc_ui_add(struct udevice *dev, struct osinfo *info)
{
	struct bc_ui_ops *ops = bc_ui_get_ops(dev);
	int ret;

	ret = ops->add(dev, info);
	if (ret)
		return log_msg_ret("bda", ret);

	return 0;
}

int bc_ui_render(struct udevice *disp)
{
	struct bc_ui_ops *ops = bc_ui_get_ops(disp);
	int ret;

	ret = ops->render(disp);
	if (ret)
		return log_msg_ret("bds", ret);

	return 0;
}

int bc_ui_poll(struct udevice *disp, int *seqp, bool *selectedp)
{
	struct bc_ui_ops *ops = bc_ui_get_ops(disp);
	int ret;

	ret = ops->poll(disp, seqp, selectedp);
	if (ret)
		return log_msg_ret("bdp", ret);

	return ret;
}

void bc_oslist_setup_iter(struct oslist_iter *iter)
{
	memset(iter, '\0', sizeof(struct oslist_iter));
}

int bc_oslist_next(struct udevice *dev, struct oslist_iter *iter,
		   struct osinfo *info)
{
	struct bc_oslist_ops *ops = bc_oslist_get_ops(dev);
	int ret;

	log_debug("oslist flags %x\n", iter->bf_iter.flags);
	ret = ops->next(dev, iter, info);
	if (ret)
		return log_msg_ret("bon", ret);

	return 0;
}

int bc_state_load(struct udevice *dev)
{
	struct bc_state_ops *ops = bc_state_get_ops(dev);
	int ret;

	ret = ops->load(dev);
	if (ret)
		return log_msg_ret("bsl", ret);

	return 0;
}

int bc_state_save(struct udevice *dev)
{
	struct bc_state_ops *ops = bc_state_get_ops(dev);
	int ret;

	ret = ops->save(dev);
	if (ret)
		return log_msg_ret("bss", ret);

	return 0;
}

int bc_state_save_to_buf(struct udevice *dev, struct abuf *buf)
{
	struct bc_state_ops *ops = bc_state_get_ops(dev);
	int ret;

	ret = ops->save_to_buf(dev, buf);
	if (ret)
		return log_msg_ret("bsb", ret);

	return 0;
}

int bc_state_clear(struct udevice *dev)
{
	struct bc_state_ops *ops = bc_state_get_ops(dev);
	int ret;

	ret = ops->clear(dev);
	if (ret)
		return log_msg_ret("bsc", ret);

	return 0;
}

int bc_state_read_bool(struct udevice *dev, const char *key, bool *valp)
{
	struct bc_state_ops *ops = bc_state_get_ops(dev);
	int ret;

	ret = ops->read_bool(dev, key, valp);
	if (ret)
		return log_msg_ret("srb", ret);

	return 0;
}

int bc_state_write_bool(struct udevice *dev, const char *key, bool val)
{
	struct bc_state_ops *ops = bc_state_get_ops(dev);
	int ret;

	ret = ops->write_bool(dev, key, val);
	if (ret)
		return log_msg_ret("swb", ret);

	return 0;
}

int bc_state_read_int(struct udevice *dev, const char *key, long *valp)
{
	struct bc_state_ops *ops = bc_state_get_ops(dev);
	int ret;

	ret = ops->read_int(dev, key, valp);
	if (ret)
		return log_msg_ret("sri", ret);

	return 0;
}

int bc_state_write_int(struct udevice *dev, const char *key, long val)
{
	struct bc_state_ops *ops = bc_state_get_ops(dev);
	int ret;

	ret = ops->write_int(dev, key, val);
	if (ret)
		return log_msg_ret("swi", ret);

	return 0;
}

int bc_state_read_str(struct udevice *dev, const char *key, const char **valp)
{
	struct bc_state_ops *ops = bc_state_get_ops(dev);
	int ret;

	ret = ops->read_str(dev, key, valp);
	if (ret)
		return log_msg_ret("srs", ret);

	return 0;
}

int bc_state_write_str(struct udevice *dev, const char *key, const char *val)
{
	struct bc_state_ops *ops = bc_state_get_ops(dev);
	int ret;

	ret = ops->write_str(dev, key, val);
	if (ret)
		return log_msg_ret("sws", ret);

	return 0;
}

int bc_logic_prepare(struct udevice *dev)
{
	struct bc_logic_ops *ops = bc_logic_get_ops(dev);
	int ret;

	ret = ops->prepare(dev);
	if (ret)
		return log_msg_ret("blp", ret);

	return 0;
}

int bc_logic_start(struct udevice *dev)
{
	struct bc_logic_ops *ops = bc_logic_get_ops(dev);
	int ret;

	ret = ops->start(dev);
	if (ret)
		return log_msg_ret("bls", ret);

	return 0;
}

int bc_logic_poll(struct udevice *dev)
{
	struct bc_logic_ops *ops = bc_logic_get_ops(dev);
	int ret;

	ret = ops->poll(dev);
	if (ret)
		return log_msg_ret("blP", ret);

	return 0;
}

int bc_measure_start(struct udevice *dev)
{
	struct bc_measure_ops *ops = bc_measure_get_ops(dev);
	int ret;

	ret = ops->start(dev);
	if (ret)
		return log_msg_ret("blM", ret);

	return 0;
}

int bc_measure_process(struct udevice *dev, const struct osinfo *osinfo,
		       struct alist *result)
{
	struct bc_measure_ops *ops = bc_measure_get_ops(dev);
	int ret;

	alist_init_struct(result, struct measure_info);
	ret = ops->process(dev, osinfo, result);
	if (ret)
		return log_msg_ret("blm", ret);

	return 0;
}

void show_measures(struct alist *result)
{
	struct measure_info *res;

	/* TODO: expand to show more info? */

	printf("Measurement report:");
	alist_for_each(res, result)
		printf(" %s", bootflow_img_type_name(res->img->type));
	printf("\n");
}
