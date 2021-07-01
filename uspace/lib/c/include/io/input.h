/*
 * Copyright (c) 2012 Jiri Svoboda
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

/** @addtogroup libc
 * @{
 */
/** @file
 */

#ifndef _LIBC_IO_INPUT_H_
#define _LIBC_IO_INPUT_H_

#include <async.h>
#include <io/kbd_event.h>

struct input_ev_ops;

typedef struct {
	async_sess_t *sess;
	struct input_ev_ops *ev_ops;
	void *user;
} input_t;

typedef struct input_ev_ops {
	errno_t (*active)(input_t *);
	errno_t (*deactive)(input_t *);
	errno_t (*key)(input_t *, kbd_event_type_t, keycode_t, keymod_t, char32_t);
	errno_t (*move)(input_t *, int, int);
	errno_t (*abs_move)(input_t *, unsigned, unsigned, unsigned, unsigned);
	errno_t (*button)(input_t *, int, int);
} input_ev_ops_t;

extern errno_t input_open(async_sess_t *, input_ev_ops_t *, void *, input_t **);
extern void input_close(input_t *);
extern errno_t input_activate(input_t *);

extern errno_t input_layout_get(async_sess_t *sess, char **layout_name);
extern errno_t input_layout_set(async_sess_t *sess, const char *layout_name);
#endif

/** @}
 */
