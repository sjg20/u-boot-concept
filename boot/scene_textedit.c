// SPDX-License-Identifier: GPL-2.0+
/*
 * Implementation of a menu in a scene
 *
 * Copyright 2025 Google LLC
 * Written by Simon Glass <sjg@chromium.org>
 */

#define LOG_CATEGORY	LOGC_EXPO

#include <expo.h>
#include <log.h>
#include <linux/err.h>
#include <linux/sizes.h>
#include "scene_internal.h"

enum {
	INITIAL_SIZE	= SZ_4K,
};

int scene_textedit(struct scene *scn, const char *name, uint id, uint str_id,
		   struct scene_obj_txtedit **teditp)
{
	struct scene_obj_txtedit *ted;
	char *buf;
	int ret;

	ret = scene_obj_add(scn, name, id, SCENEOBJT_TEXTEDIT,
			    sizeof(struct scene_obj_txtedit),
			    (struct scene_obj **)&ted);
	if (ret < 0)
		return log_msg_ret("obj", ret);

	abuf_init(&ted->buf);
	if (!abuf_realloc(&ted->buf, INITIAL_SIZE))
		return log_msg_ret("buf", -ENOMEM);
	buf = abuf_data(&ted->buf);
	*buf = '\0';

	ret = expo_str(scn->expo, name, str_id, buf);
	if (ret != str_id)
		return log_msg_ret("tes", -EPERM);
	ted->gen.str_id = str_id;

	if (teditp)
		*teditp = ted;

	return ted->obj.id;
}

void scene_txtedit_display(struct scene_obj_txtedit *ted)
{
}

int scene_txted_set_font(struct scene *scn, uint id, const char *font_name,
			 uint font_size)
{
	struct scene_obj_txtedit *ted;

	ted = scene_obj_find(scn, id, SCENEOBJT_TEXTEDIT);
	if (!ted)
		return log_msg_ret("find", -ENOENT);
	ted->gen.font_name = font_name;
	ted->gen.font_size = font_size;

	return 0;
}
