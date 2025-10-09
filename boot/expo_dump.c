// SPDX-License-Identifier: GPL-2.0+
/*
 * Dump functions for expo objects
 *
 * Copyright 2025 Canonical Ltd
 * Written by Simon Glass <simon.glass@canonical.com>
 */

#define LOG_CATEGORY	LOGC_EXPO

#include <dm.h>
#include <expo.h>
#include <mapmem.h>
#include <membuf.h>
#include <video.h>
#include "scene_internal.h"

/**
 * struct dump_ctx - Context for dumping expo structures
 *
 * @mb: Membuf to write output to
 * @scn: Current scene being dumped (or NULL if not in a scene)
 * @indent: Current indentation level (number of spaces)
 */
struct dump_ctx {
	struct membuf *mb;
	struct scene *scn;
	int indent;
};

/**
 * outf() - Output a formatted string with indentation
 *
 * @ctx: Dump context containing membuf, scene, and indent level
 * @fmt: Format string
 * @...: Arguments for format string
 */
static void outf(struct dump_ctx *ctx, const char *fmt, ...)
{
	char buf[256];
	va_list args;
	int len;

	va_start(args, fmt);
	membuf_printf(ctx->mb, "%*s", ctx->indent, "");
	len = vsnprintf(buf, sizeof(buf), fmt, args);
	membuf_put(ctx->mb, buf, len);
	va_end(args);
}

static const char *obj_name(struct dump_ctx *ctx, uint id)
{
	struct scene_obj *obj;

	if (!id)
		return "(none)";

	obj = scene_obj_find(ctx->scn, id, SCENEOBJT_NONE);
	if (!obj)
		return "(not found)";

	return obj->name;
}

static void dump_menu(struct dump_ctx *ctx, struct scene_obj_menu *menu)
{
	struct scene_obj *obj = &menu->obj;
	struct scene_menitem *item;

	outf(ctx, "Menu: pointer_id %d title_id %d manual %d\n",
	     menu->pointer_id, menu->title_id,
	     !!(obj->flags & SCENEOF_MANUAL));

	ctx->indent += 2;
	list_for_each_entry(item, &menu->item_head, sibling) {
		outf(ctx, "Item %d: name '%s' label_id %d desc_id %d\n",
		     item->id, item->name, item->label_id, item->desc_id);
	}
	ctx->indent -= 2;
}

static void dump_text(struct dump_ctx *ctx, struct scene_obj_txt *txt)
{
	const char *str = expo_get_str(ctx->scn->expo, txt->gen.str_id);

	outf(ctx, "Text: str_id %d font_name '%s' font_size %d\n",
	     txt->gen.str_id,
	     txt->gen.font_name ? txt->gen.font_name : "(default)",
	     txt->gen.font_size);
	ctx->indent += 2;
	outf(ctx, "str '%s'\n", str ? str : "(null)");
	ctx->indent -= 2;
}

static void dump_box(struct dump_ctx *ctx, struct scene_obj_box *box)
{
	outf(ctx, "Box: fill %d width %d\n", box->fill, box->width);
}

static void dump_image(struct dump_ctx *ctx, struct scene_obj_img *img)
{
	outf(ctx, "Image: data %lx\n", (ulong)map_to_sysmem(img->data));
}

static void dump_textline(struct dump_ctx *ctx,
			  struct scene_obj_textline *tline)
{
	outf(ctx, "Textline: label_id %d edit_id %d\n",
	     tline->label_id, tline->edit_id);
	ctx->indent += 2;
	outf(ctx, "max_chars %d pos %d\n", tline->max_chars, tline->pos);
	ctx->indent -= 2;
}

static void dump_textedit(struct dump_ctx *ctx,
			  struct scene_obj_txtedit *tedit)
{
	outf(ctx, "Textedit: str_id %d font_name '%s' font_size %d\n",
	     tedit->gen.str_id,
	     tedit->gen.font_name ? tedit->gen.font_name : "(default)",
	     tedit->gen.font_size);
}

static void obj_dump_(struct dump_ctx *ctx, struct scene_obj *obj)
{
	char flags_buf[256];
	bool first = true;
	int bit;
	int pos = 0;

	outf(ctx, "Object %d (%s): type %s\n", obj->id, obj->name,
	     scene_obj_type_name(obj->type));
	ctx->indent += 2;

	/* Build flags string */
	for (bit = 0; bit < 16; bit++) {
		uint flag = BIT(bit);

		if (obj->flags & flag) {
			pos += snprintf(flags_buf + pos, sizeof(flags_buf) - pos,
					"%s%s", first ? "" : ", ",
					scene_flag_name(flag));
			first = false;
		}
	}
	outf(ctx, "flags %s\n", pos > 0 ? flags_buf : "");
	outf(ctx, "bbox: (%d,%d)-(%d,%d)\n",
	     obj->bbox.x0, obj->bbox.y0, obj->bbox.x1, obj->bbox.y1);
	outf(ctx, "dims: %dx%d\n", obj->dims.x, obj->dims.y);

	switch (obj->type) {
	case SCENEOBJT_NONE:
		break;
	case SCENEOBJT_IMAGE:
		dump_image(ctx, (struct scene_obj_img *)obj);
		break;
	case SCENEOBJT_TEXT:
		dump_text(ctx, (struct scene_obj_txt *)obj);
		break;
	case SCENEOBJT_BOX:
		dump_box(ctx, (struct scene_obj_box *)obj);
		break;
	case SCENEOBJT_MENU:
		dump_menu(ctx, (struct scene_obj_menu *)obj);
		break;
	case SCENEOBJT_TEXTLINE:
		dump_textline(ctx, (struct scene_obj_textline *)obj);
		break;
	case SCENEOBJT_TEXTEDIT:
		dump_textedit(ctx, (struct scene_obj_txtedit *)obj);
		break;
	}
	ctx->indent -= 2;
}

static void scene_dump_(struct dump_ctx *ctx)
{
	struct scene_obj *obj;

	outf(ctx, "Scene %d: name '%s'\n", ctx->scn->id, ctx->scn->name);
	ctx->indent += 2;
	outf(ctx, "title_id %d (%s)\n",
	     ctx->scn->title_id, obj_name(ctx, ctx->scn->title_id));
	outf(ctx, "highlight_id %d (%s)\n",
	     ctx->scn->highlight_id, obj_name(ctx, ctx->scn->highlight_id));

	list_for_each_entry(obj, &ctx->scn->obj_head, sibling) {
		/* Skip hidden objects */
		if (obj->flags & SCENEOF_HIDE)
			continue;
		obj_dump_(ctx, obj);
	}
	ctx->indent -= 2;
}

void scene_dump(struct membuf *mb, struct scene *scn, int indent)
{
	struct dump_ctx ctx;

	ctx.mb = mb;
	ctx.scn = scn;
	ctx.indent = indent;

	scene_dump_(&ctx);
}

static void expo_dump_(struct dump_ctx *ctx, struct expo *exp)
{
	struct scene *scn;
	struct expo_theme *theme = &exp->theme;

	outf(ctx, "Expo: name '%s'\n", exp->name);
	ctx->indent = 2;
	outf(ctx, "display %s\n",
	     exp->display ? exp->display->name : "(null)");
	outf(ctx, "cons %s\n", exp->cons ? exp->cons->name : "(none)");
	outf(ctx, "mouse %s\n", exp->mouse ? exp->mouse->name : "(none)");
	outf(ctx, "scene_id %d\n", exp->scene_id);
	outf(ctx, "next_id %d\n", exp->next_id);
	outf(ctx, "req_width %d\n", exp->req_width);
	outf(ctx, "req_height %d\n", exp->req_height);
	outf(ctx, "text_mode %d\n", exp->text_mode);
	outf(ctx, "popup %d\n", exp->popup);
	outf(ctx, "show_highlight %d\n", exp->show_highlight);
	outf(ctx, "mouse_enabled %d\n", exp->mouse_enabled);
	outf(ctx, "mouse_ptr %p\n", exp->mouse_ptr);
	outf(ctx, "mouse_size %dx%d\n", exp->mouse_size.w,
	     exp->mouse_size.h);
	outf(ctx, "mouse_pos (%d,%d)\n", exp->mouse_pos.x,
	     exp->mouse_pos.y);
	outf(ctx, "damage (%d,%d)-(%d,%d)\n", exp->damage.x0, exp->damage.y0,
	     exp->damage.x1, exp->damage.y1);
	outf(ctx, "done %d\n", exp->done);
	outf(ctx, "save %d\n", exp->save);
	outf(ctx, "last_key_ms %ld\n", exp->last_key_ms);

	if (exp->display) {
		struct video_priv *vid_priv = dev_get_uclass_priv(exp->display);

		outf(ctx, "video: %dx%d white_on_black %d\n",
		     vid_priv->xsize, vid_priv->ysize,
		     vid_priv->white_on_black);
	}

	outf(ctx, "Theme:\n");
	ctx->indent = 4;
	outf(ctx, "font_size %d\n", theme->font_size);
	outf(ctx, "white_on_black %d\n", theme->white_on_black);
	outf(ctx, "menu_inset %d\n", theme->menu_inset);
	outf(ctx, "menuitem_gap_y %d\n", theme->menuitem_gap_y);

	ctx->indent = 0;
	outf(ctx, "\nScenes:\n");
	ctx->indent = 2;
	list_for_each_entry(scn, &exp->scene_head, sibling) {
		ctx->scn = scn;
		scene_dump_(ctx);
		outf(ctx, "\n");
	}
}

void expo_dump(struct expo *exp, struct membuf *mb)
{
	struct dump_ctx ctx;

	ctx.mb = mb;
	ctx.scn = NULL;
	ctx.indent = 0;

	expo_dump_(&ctx, exp);
}
