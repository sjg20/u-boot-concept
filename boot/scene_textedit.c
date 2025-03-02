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
		   struct scene_obj_textedit **teditp)
{
	struct scene_obj_textedit *ted;
	char *buf;
	int ret;

	ret = scene_obj_add(scn, name, id, SCENEOBJT_TEXTEDIT,
			    sizeof(struct scene_obj_textedit),
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

	if (teditp)
		*teditp = ted;

	return ted->obj.id;
}

void scene_textedit_display(struct scene_obj_textedit *ted)
{
}
