/*
 * Copyright (c) 2020 Jiri Svoboda
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
 * @file Window decoration
 */

#ifndef _UI_WDECOR_H
#define _UI_WDECOR_H

#include <errno.h>
#include <gfx/coord.h>
#include <io/pos_event.h>
#include <stdbool.h>
#include <types/ui/resource.h>
#include <types/ui/wdecor.h>

extern errno_t ui_wdecor_create(ui_resource_t *, const char *,
    ui_wdecor_style_t, ui_wdecor_t **);
extern void ui_wdecor_destroy(ui_wdecor_t *);
extern void ui_wdecor_set_cb(ui_wdecor_t *, ui_wdecor_cb_t *, void *);
extern void ui_wdecor_set_rect(ui_wdecor_t *, gfx_rect_t *);
extern void ui_wdecor_set_active(ui_wdecor_t *, bool);
extern errno_t ui_wdecor_paint(ui_wdecor_t *);
extern void ui_wdecor_pos_event(ui_wdecor_t *, pos_event_t *);
extern void ui_wdecor_rect_from_app(ui_wdecor_style_t, gfx_rect_t *,
    gfx_rect_t *);

#endif

/** @}
 */