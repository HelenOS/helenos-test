/*
 * Copyright (c) 2009 Jiri Svoboda
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

/** @addtogroup inputgen generic
 * @brief Keyboard layout interface.
 * @ingroup input
 * @{
 */
/** @file
 */

#ifndef KBD_LAYOUT_H_
#define KBD_LAYOUT_H_

#include <io/console.h>

/** Layout instance state */
typedef struct layout {
	/** Ops structure */
	struct layout_ops *ops;

	/* Layout-private data */
	void *layout_priv;
} layout_t;

/** Layout ops */
typedef struct layout_ops {
	errno_t (*create)(layout_t *);
	void (*destroy)(layout_t *);
	char32_t (*parse_ev)(layout_t *, kbd_event_t *);
	const char *name;
} layout_ops_t;

extern layout_ops_t us_qwerty_ops;
extern layout_ops_t us_dvorak_ops;
extern layout_ops_t cz_ops;
extern layout_ops_t ar_ops;
extern layout_ops_t fr_azerty_ops;

extern layout_t *layout_create(layout_ops_t *);
extern void layout_destroy(layout_t *);
extern char32_t layout_parse_ev(layout_t *, kbd_event_t *);

#endif

/**
 * @}
 */
