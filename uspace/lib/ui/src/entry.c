/*
 * Copyright (c) 2021 Jiri Svoboda
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @addtogroup libui
 * @{
 */
/**
 * @file Text entry.
 *
 * Currentry text entry is always read-only. It differs from label mostly
 * by its looks.
 */

#include <errno.h>
#include <gfx/context.h>
#include <gfx/cursor.h>
#include <gfx/render.h>
#include <gfx/text.h>
#include <stdlib.h>
#include <str.h>
#include <ui/control.h>
#include <ui/paint.h>
#include <ui/entry.h>
#include <ui/ui.h>
#include <ui/window.h>
#include "../private/entry.h"
#include "../private/resource.h"

static void ui_entry_ctl_destroy(void *);
static errno_t ui_entry_ctl_paint(void *);
static ui_evclaim_t ui_entry_ctl_kbd_event(void *, kbd_event_t *);
static ui_evclaim_t ui_entry_ctl_pos_event(void *, pos_event_t *);

enum {
	ui_entry_hpad = 4,
	ui_entry_vpad = 4,
	ui_entry_hpad_text = 1,
	ui_entry_vpad_text = 0,
	ui_entry_cursor_overshoot = 1,
	ui_entry_cursor_width = 2
};

/** Text entry control ops */
ui_control_ops_t ui_entry_ops = {
	.destroy = ui_entry_ctl_destroy,
	.paint = ui_entry_ctl_paint,
	.kbd_event = ui_entry_ctl_kbd_event,
	.pos_event = ui_entry_ctl_pos_event
};

/** Create new text entry.
 *
 * @param window UI window
 * @param text Text
 * @param rentry Place to store pointer to new text entry
 * @return EOK on success, ENOMEM if out of memory
 */
errno_t ui_entry_create(ui_window_t *window, const char *text,
    ui_entry_t **rentry)
{
	ui_entry_t *entry;
	errno_t rc;

	entry = calloc(1, sizeof(ui_entry_t));
	if (entry == NULL)
		return ENOMEM;

	rc = ui_control_new(&ui_entry_ops, (void *) entry, &entry->control);
	if (rc != EOK) {
		free(entry);
		return rc;
	}

	entry->text = str_dup(text);
	if (entry->text == NULL) {
		ui_control_delete(entry->control);
		free(entry);
		return ENOMEM;
	}

	entry->window = window;
	entry->halign = gfx_halign_left;
	*rentry = entry;

	return EOK;
}

/** Destroy text entry.
 *
 * @param entry Text entry or @c NULL
 */
void ui_entry_destroy(ui_entry_t *entry)
{
	if (entry == NULL)
		return;

	ui_control_delete(entry->control);
	free(entry);
}

/** Get base control from text entry.
 *
 * @param entry Text entry
 * @return Control
 */
ui_control_t *ui_entry_ctl(ui_entry_t *entry)
{
	return entry->control;
}

/** Set text entry rectangle.
 *
 * @param entry Text entry
 * @param rect New text entry rectangle
 */
void ui_entry_set_rect(ui_entry_t *entry, gfx_rect_t *rect)
{
	entry->rect = *rect;
}

/** Set text entry horizontal text alignment.
 *
 * @param entry Text entry
 * @param halign Horizontal alignment
 */
void ui_entry_set_halign(ui_entry_t *entry, gfx_halign_t halign)
{
	entry->halign = halign;
}

/** Set text entry read-only flag.
 *
 * @param entry Text entry
 * @param read_only True iff entry is to be read-only.
 */
void ui_entry_set_read_only(ui_entry_t *entry, bool read_only)
{
	entry->read_only = read_only;
}

/** Set entry text.
 *
 * @param entry Text entry
 * @param text New entry text
 * @return EOK on success, ENOMEM if out of memory
 */
errno_t ui_entry_set_text(ui_entry_t *entry, const char *text)
{
	char *tcopy;

	tcopy = str_dup(text);
	if (tcopy == NULL)
		return ENOMEM;

	free(entry->text);
	entry->text = tcopy;
	entry->pos = str_size(text);

	return EOK;
}

/** Paint cursor.
 *
 * @param entry Text entry
 * @param pos Cursor position (top-left corner of next character)
 * @return EOK on success or an error code
 */
static errno_t ui_entry_paint_cursor(ui_entry_t *entry, gfx_coord2_t *pos)
{
	ui_resource_t *res;
	gfx_rect_t rect;
	gfx_font_metrics_t metrics;
	errno_t rc;

	res = ui_window_get_res(entry->window);

	if (res->textmode) {
		rc = gfx_cursor_set_pos(res->gc, pos);
		return rc;
	}

	gfx_font_get_metrics(res->font, &metrics);

	rect.p0.x = pos->x;
	rect.p0.y = pos->y - ui_entry_cursor_overshoot;
	rect.p1.x = pos->x + ui_entry_cursor_width;
	rect.p1.y = pos->y + metrics.ascent + metrics.descent + 1 +
	    ui_entry_cursor_overshoot;

	rc = gfx_set_color(res->gc, res->entry_fg_color);
	if (rc != EOK)
		goto error;

	rc = gfx_fill_rect(res->gc, &rect);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	return rc;
}

/** Return width of text before cursor.
 *
 * @param entry Text entry
 * @return Widht of text before cursor
 */
static gfx_coord_t ui_entry_lwidth(ui_entry_t *entry)
{
	ui_resource_t *res;
	uint8_t tmp;
	gfx_coord_t width;

	res = ui_window_get_res(entry->window);

	tmp = entry->text[entry->pos];

	entry->text[entry->pos] = '\0';
	width = gfx_text_width(res->font, entry->text);
	entry->text[entry->pos] = tmp;

	return width;
}

/** Paint text entry.
 *
 * @param entry Text entry
 * @return EOK on success or an error code
 */
errno_t ui_entry_paint(ui_entry_t *entry)
{
	ui_resource_t *res;
	ui_entry_geom_t geom;
	gfx_text_fmt_t fmt;
	gfx_coord2_t pos;
	gfx_rect_t inside;
	errno_t rc;

	res = ui_window_get_res(entry->window);

	ui_entry_get_geom(entry, &geom);

	if (res->textmode == false) {
		/* Paint inset frame */
		rc = ui_paint_inset_frame(res, &entry->rect, &inside);
		if (rc != EOK)
			goto error;
	} else {
		inside = entry->rect;
	}

	/* Paint entry background */

	rc = gfx_set_color(res->gc, res->entry_bg_color);
	if (rc != EOK)
		goto error;

	rc = gfx_fill_rect(res->gc, &inside);
	if (rc != EOK)
		goto error;

	pos = geom.text_pos;

	gfx_text_fmt_init(&fmt);
	fmt.color = res->entry_fg_color;
	fmt.halign = gfx_halign_left;
	fmt.valign = gfx_valign_top;

	rc = gfx_set_clip_rect(res->gc, &inside);
	if (rc != EOK)
		goto error;

	rc = gfx_puttext(res->font, &pos, &fmt, entry->text);
	if (rc != EOK) {
		(void) gfx_set_clip_rect(res->gc, NULL);
		goto error;
	}

	if (entry->active) {
		/* Cursor */
		pos.x += ui_entry_lwidth(entry);

		rc = ui_entry_paint_cursor(entry, &pos);
		if (rc != EOK) {
			(void) gfx_set_clip_rect(res->gc, NULL);
			goto error;
		}
	}

	rc = gfx_set_clip_rect(res->gc, NULL);
	if (rc != EOK)
		goto error;

	rc = gfx_update(res->gc);
	if (rc != EOK)
		goto error;

	return EOK;
error:
	return rc;
}

/** Find position in text entry.
 *
 * @param entry Text entry
 * @param fpos Position for which we need to find text offset
 * @return Corresponding byte offset in entry text
 */
size_t ui_entry_find_pos(ui_entry_t *entry, gfx_coord2_t *fpos)
{
	ui_resource_t *res;
	ui_entry_geom_t geom;
	gfx_text_fmt_t fmt;

	res = ui_window_get_res(entry->window);

	ui_entry_get_geom(entry, &geom);

	gfx_text_fmt_init(&fmt);
	fmt.halign = gfx_halign_left;
	fmt.valign = gfx_valign_top;

	return gfx_text_find_pos(res->font, &geom.text_pos, &fmt,
	    entry->text, fpos);
}

/** Destroy text entry control.
 *
 * @param arg Argument (ui_entry_t *)
 */
void ui_entry_ctl_destroy(void *arg)
{
	ui_entry_t *entry = (ui_entry_t *) arg;

	ui_entry_destroy(entry);
}

/** Paint text entry control.
 *
 * @param arg Argument (ui_entry_t *)
 * @return EOK on success or an error code
 */
errno_t ui_entry_ctl_paint(void *arg)
{
	ui_entry_t *entry = (ui_entry_t *) arg;

	return ui_entry_paint(entry);
}

/** Insert string at cursor position.
 *
 * @param entry Text entry
 * @param str String
 * @return EOK on success, ENOMEM if out of memory
 */
errno_t ui_entry_insert_str(ui_entry_t *entry, const char *str)
{
	uint8_t tmp;
	char *ltext = NULL;
	char *newtext;
	char *oldtext;
	int rc;

	tmp = entry->text[entry->pos];
	entry->text[entry->pos] = '\0';
	ltext = str_dup(entry->text);
	if (ltext == NULL)
		return ENOMEM;

	entry->text[entry->pos] = tmp;

	rc = asprintf(&newtext, "%s%s%s", ltext, str, entry->text + entry->pos);
	if (rc < 0) {
		free(ltext);
		return ENOMEM;
	}

	oldtext = entry->text;
	entry->text = newtext;
	entry->pos += str_size(str);
	free(oldtext);
	free(ltext);

	ui_entry_paint(entry);

	return EOK;
}

/** Delete character before cursor.
 *
 * @param entry Text entry
 */
void ui_entry_backspace(ui_entry_t *entry)
{
	size_t off;

	if (entry->pos == 0)
		return;

	/* Find offset where character before cursor starts */
	off = entry->pos;
	(void) str_decode_reverse(entry->text, &off,
	    str_size(entry->text));

	memmove(entry->text + off, entry->text + entry->pos,
	    str_size(entry->text + entry->pos) + 1);
	entry->pos = off;

	ui_entry_paint(entry);
}

/** Delete character after cursor.
 *
 * @param entry Text entry
 */
void ui_entry_delete(ui_entry_t *entry)
{
	size_t off;

	/* Find offset where character after cursor end */
	off = entry->pos;
	(void) str_decode(entry->text, &off,
	    str_size(entry->text));

	memmove(entry->text + entry->pos, entry->text + off,
	    str_size(entry->text + off) + 1);

	ui_entry_paint(entry);
}

/** Handle text entry key press without modifiers.
 *
 * @param entry Text entry
 * @param kbd_event Keyboard event
 * @return @c ui_claimed iff the event is claimed
 */
ui_evclaim_t ui_entry_key_press_unmod(ui_entry_t *entry, kbd_event_t *event)
{
	assert(event->type == KEY_PRESS);

	switch (event->key) {
	case KC_BACKSPACE:
		ui_entry_backspace(entry);
		break;

	case KC_DELETE:
		ui_entry_delete(entry);
		break;

	case KC_ESCAPE:
		ui_entry_deactivate(entry);
		break;

	case KC_HOME:
		ui_entry_seek_start(entry);
		break;

	case KC_END:
		ui_entry_seek_end(entry);
		break;

	case KC_LEFT:
		ui_entry_seek_prev_char(entry);
		break;

	case KC_RIGHT:
		ui_entry_seek_next_char(entry);
		break;

	default:
		break;
	}

	return ui_claimed;
}

/** Handle text entry keyboard event.
 *
 * @param entry Text entry
 * @param kbd_event Keyboard event
 * @return @c ui_claimed iff the event is claimed
 */
ui_evclaim_t ui_entry_kbd_event(ui_entry_t *entry, kbd_event_t *event)
{
	char buf[STR_BOUNDS(1) + 1];
	size_t off;
	errno_t rc;

	if (!entry->active)
		return ui_unclaimed;

	if (event->type == KEY_PRESS && event->c >= ' ') {
		off = 0;
		rc = chr_encode(event->c, buf, &off, sizeof(buf));
		if (rc == EOK) {
			buf[off] = '\0';
			(void) ui_entry_insert_str(entry, buf);
		}
	}

	if (event->type == KEY_PRESS &&
	    (event->mods & (KM_CTRL | KM_ALT | KM_SHIFT)) == 0)
		return ui_entry_key_press_unmod(entry, event);

	return ui_claimed;
}

/** Handle text entry position event.
 *
 * @param entry Text entry
 * @param pos_event Position event
 * @return @c ui_claimed iff the event is claimed
 */
ui_evclaim_t ui_entry_pos_event(ui_entry_t *entry, pos_event_t *event)
{
	gfx_coord2_t pos;

	if (entry->read_only)
		return ui_unclaimed;

	if (event->type == POS_UPDATE) {
		pos.x = event->hpos;
		pos.y = event->vpos;

		if (gfx_pix_inside_rect(&pos, &entry->rect)) {
			if (!entry->pointer_inside) {
				ui_window_set_ctl_cursor(entry->window,
				    ui_curs_ibeam);
				entry->pointer_inside = true;
			}
		} else {
			if (entry->pointer_inside) {
				ui_window_set_ctl_cursor(entry->window,
				    ui_curs_arrow);
				entry->pointer_inside = false;
			}
		}
	}

	if (event->type == POS_PRESS) {
		pos.x = event->hpos;
		pos.y = event->vpos;

		if (gfx_pix_inside_rect(&pos, &entry->rect)) {
			entry->pos = ui_entry_find_pos(entry, &pos);
			if (entry->active)
				ui_entry_paint(entry);
			else
				ui_entry_activate(entry);

			return ui_claimed;
		} else {
			ui_entry_deactivate(entry);
		}
	}

	return ui_unclaimed;
}

/** Handle text entry control keyboard event.
 *
 * @param arg Argument (ui_entry_t *)
 * @param kbd_event Keyboard event
 * @return @c ui_claimed iff the event is claimed
 */
static ui_evclaim_t ui_entry_ctl_kbd_event(void *arg, kbd_event_t *event)
{
	ui_entry_t *entry = (ui_entry_t *) arg;

	return ui_entry_kbd_event(entry, event);
}

/** Handle text entry control position event.
 *
 * @param arg Argument (ui_entry_t *)
 * @param pos_event Position event
 * @return @c ui_claimed iff the event is claimed
 */
static ui_evclaim_t ui_entry_ctl_pos_event(void *arg, pos_event_t *event)
{
	ui_entry_t *entry = (ui_entry_t *) arg;

	return ui_entry_pos_event(entry, event);
}

/** Get text entry geometry.
 *
 * @param entry Text entry
 * @param geom Structure to fill in with computed geometry
 */
void ui_entry_get_geom(ui_entry_t *entry, ui_entry_geom_t *geom)
{
	gfx_coord_t hpad;
	gfx_coord_t vpad;
	gfx_coord_t width;
	ui_resource_t *res;

	res = ui_window_get_res(entry->window);

	if (res->textmode) {
		hpad = ui_entry_hpad_text;
		vpad = ui_entry_vpad_text;
	} else {
		hpad = ui_entry_hpad;
		vpad = ui_entry_vpad;
	}

	if (res->textmode == false) {
		ui_paint_get_inset_frame_inside(res, &entry->rect,
		    &geom->interior_rect);
	} else {
		geom->interior_rect = entry->rect;
	}

	width = gfx_text_width(res->font, entry->text);

	switch (entry->halign) {
	case gfx_halign_left:
	case gfx_halign_justify:
		geom->text_pos.x = geom->interior_rect.p0.x + hpad;
		break;
	case gfx_halign_center:
		geom->text_pos.x = (geom->interior_rect.p0.x +
		    geom->interior_rect.p1.x) / 2 - width / 2;
		break;
	case gfx_halign_right:
		geom->text_pos.x = geom->interior_rect.p1.x - hpad - 1 - width;
		break;
	}

	geom->text_pos.y = geom->interior_rect.p0.y + vpad;
}

/** Activate text entry.
 *
 * @param entry Text entry
 */
void ui_entry_activate(ui_entry_t *entry)
{
	ui_resource_t *res;

	res = ui_window_get_res(entry->window);

	if (entry->active)
		return;

	entry->active = true;
	(void) ui_entry_paint(entry);

	if (res->textmode)
		gfx_cursor_set_visible(res->gc, true);
}

/** Move text cursor to the beginning of text.
 *
 * @param entry Text entry
 */
void ui_entry_seek_start(ui_entry_t *entry)
{
	entry->pos = 0;
	(void) ui_entry_paint(entry);
}

/** Move text cursor to the end of text.
 *
 * @param entry Text entry
 */
void ui_entry_seek_end(ui_entry_t *entry)
{
	entry->pos = str_size(entry->text);
	(void) ui_entry_paint(entry);
}

/** Move text cursor one character backward.
 *
 * @param entry Text entry
 */
void ui_entry_seek_prev_char(ui_entry_t *entry)
{
	size_t off;

	off = entry->pos;
	(void) str_decode_reverse(entry->text, &off,
	    str_size(entry->text));
	entry->pos = off;
	(void) ui_entry_paint(entry);
}

/** Move text cursor one character forward.
 *
 * @param entry Text entry
 */
void ui_entry_seek_next_char(ui_entry_t *entry)
{
	size_t off;

	off = entry->pos;
	(void) str_decode(entry->text, &off,
	    str_size(entry->text));
	entry->pos = off;
	(void) ui_entry_paint(entry);
}

/** Deactivate text entry.
 *
 * @param entry Text entry
 */
void ui_entry_deactivate(ui_entry_t *entry)
{
	ui_resource_t *res;

	res = ui_window_get_res(entry->window);

	if (!entry->active)
		return;

	entry->active = false;
	(void) ui_entry_paint(entry);

	if (res->textmode)
		gfx_cursor_set_visible(res->gc, false);
}

/** @}
 */
