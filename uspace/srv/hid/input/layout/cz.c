/*
 * Copyright (c) 2011 Jiri Svoboda
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

/** @addtogroup input
 * @{
 */

/** @file
 * @brief Czech QWERTZ layout
 */

#include <errno.h>
#include <io/console.h>
#include <io/keycode.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../input.h"
#include "../layout.h"

static errno_t cz_create(layout_t *);
static void cz_destroy(layout_t *);
static char32_t cz_parse_ev(layout_t *, kbd_event_t *ev);

enum m_state {
	ms_start,
	ms_hacek,
	ms_carka
};

typedef struct {
	enum m_state mstate;
} layout_cz_t;

layout_ops_t cz_ops = {
	.create = cz_create,
	.destroy = cz_destroy,
	.parse_ev = cz_parse_ev,
	.name = "cz"
};

static char32_t map_lcase[] = {
	[KC_Q] = 'q',
	[KC_W] = 'w',
	[KC_E] = 'e',
	[KC_R] = 'r',
	[KC_T] = 't',
	[KC_Y] = 'z',
	[KC_U] = 'u',
	[KC_I] = 'i',
	[KC_O] = 'o',
	[KC_P] = 'p',

	[KC_A] = 'a',
	[KC_S] = 's',
	[KC_D] = 'd',
	[KC_F] = 'f',
	[KC_G] = 'g',
	[KC_H] = 'h',
	[KC_J] = 'j',
	[KC_K] = 'k',
	[KC_L] = 'l',

	[KC_Z] = 'y',
	[KC_X] = 'x',
	[KC_C] = 'c',
	[KC_V] = 'v',
	[KC_B] = 'b',
	[KC_N] = 'n',
	[KC_M] = 'm',
};

static char32_t map_ucase[] = {
	[KC_Q] = 'Q',
	[KC_W] = 'W',
	[KC_E] = 'E',
	[KC_R] = 'R',
	[KC_T] = 'T',
	[KC_Y] = 'Z',
	[KC_U] = 'U',
	[KC_I] = 'I',
	[KC_O] = 'O',
	[KC_P] = 'P',

	[KC_A] = 'A',
	[KC_S] = 'S',
	[KC_D] = 'D',
	[KC_F] = 'F',
	[KC_G] = 'G',
	[KC_H] = 'H',
	[KC_J] = 'J',
	[KC_K] = 'K',
	[KC_L] = 'L',

	[KC_Z] = 'Y',
	[KC_X] = 'X',
	[KC_C] = 'C',
	[KC_V] = 'V',
	[KC_B] = 'B',
	[KC_N] = 'N',
	[KC_M] = 'M',
};

static char32_t map_not_shifted[] = {
	[KC_BACKTICK] = ';',

	[KC_1] = '+',

	[KC_MINUS] = '=',

	[KC_RBRACKET] = ')',

	[KC_QUOTE] = L'§',

	[KC_COMMA] = ',',
	[KC_PERIOD] = '.',
	[KC_SLASH] = '-',
};

static char32_t map_shifted[] = {
	[KC_1] = '1',
	[KC_2] = '2',
	[KC_3] = '3',
	[KC_4] = '4',
	[KC_5] = '5',
	[KC_6] = '6',
	[KC_7] = '7',
	[KC_8] = '8',
	[KC_9] = '9',
	[KC_0] = '0',

	[KC_MINUS] = '%',

	[KC_LBRACKET] = '/',
	[KC_RBRACKET] = '(',

	[KC_SEMICOLON] = '"',
	[KC_QUOTE] = '!',
	[KC_BACKSLASH] = '\'',

	[KC_COMMA] = '?',
	[KC_PERIOD] = ':',
	[KC_SLASH] = '_',
};

static char32_t map_ns_nocaps[] = {
	[KC_2] = L'ě',
	[KC_3] = L'š',
	[KC_4] = L'č',
	[KC_5] = L'ř',
	[KC_6] = L'ž',
	[KC_7] = L'ý',
	[KC_8] = L'á',
	[KC_9] = L'í',
	[KC_0] = L'é',

	[KC_LBRACKET] = L'ú',
	[KC_SEMICOLON] = L'ů'
};

static char32_t map_ns_caps[] = {
	[KC_2] = L'Ě',
	[KC_3] = L'Š',
	[KC_4] = L'Č',
	[KC_5] = L'Ř',
	[KC_6] = L'Ž',
	[KC_7] = L'Ý',
	[KC_8] = L'Á',
	[KC_9] = L'Í',
	[KC_0] = L'É',

	[KC_LBRACKET] = L'Ú',
	[KC_SEMICOLON] = L'Ů'
};

static char32_t map_neutral[] = {
	[KC_BACKSPACE] = '\b',
	[KC_TAB] = '\t',
	[KC_ENTER] = '\n',
	[KC_SPACE] = ' ',

	[KC_NSLASH] = '/',
	[KC_NTIMES] = '*',
	[KC_NMINUS] = '-',
	[KC_NPLUS] = '+',
	[KC_NENTER] = '\n'
};

static char32_t map_numeric[] = {
	[KC_N7] = '7',
	[KC_N8] = '8',
	[KC_N9] = '9',
	[KC_N4] = '4',
	[KC_N5] = '5',
	[KC_N6] = '6',
	[KC_N1] = '1',
	[KC_N2] = '2',
	[KC_N3] = '3',

	[KC_N0] = '0',
	[KC_NPERIOD] = '.'
};

static char32_t map_hacek_lcase[] = {
	[KC_E] = L'ě',
	[KC_R] = L'ř',
	[KC_T] = L'ť',
	[KC_Y] = L'ž',
	[KC_U] = L'ů',

	[KC_S] = L'š',
	[KC_D] = L'ď',

	[KC_C] = L'č',
	[KC_N] = L'ň'
};

static char32_t map_hacek_ucase[] = {
	[KC_E] = L'Ě',
	[KC_R] = L'Ř',
	[KC_T] = L'Ť',
	[KC_Y] = L'Ž',
	[KC_U] = L'Ů',

	[KC_S] = L'Š',
	[KC_D] = L'Ď',

	[KC_C] = L'Č',
	[KC_N] = L'Ň'
};

static char32_t map_carka_lcase[] = {
	[KC_E] = L'é',
	[KC_U] = L'ú',
	[KC_I] = L'í',
	[KC_O] = L'ó',

	[KC_A] = L'á',

	[KC_Z] = L'ý',
};

static char32_t map_carka_ucase[] = {
	[KC_E] = L'É',
	[KC_U] = L'Ú',
	[KC_I] = L'Í',
	[KC_O] = L'Ó',

	[KC_A] = L'Á',

	[KC_Z] = L'Ý',
};

static char32_t translate(unsigned int key, char32_t *map, size_t map_length)
{
	if (key >= map_length)
		return 0;
	return map[key];
}

static char32_t parse_ms_hacek(layout_cz_t *cz_state, kbd_event_t *ev)
{
	char32_t c;

	cz_state->mstate = ms_start;

	/* Produce no characters when Ctrl or Alt is pressed. */
	if ((ev->mods & (KM_CTRL | KM_ALT)) != 0)
		return 0;

	if (((ev->mods & KM_SHIFT) != 0) ^ ((ev->mods & KM_CAPS_LOCK) != 0))
		c = translate(ev->key, map_hacek_ucase, sizeof(map_hacek_ucase) / sizeof(char32_t));
	else
		c = translate(ev->key, map_hacek_lcase, sizeof(map_hacek_lcase) / sizeof(char32_t));

	return c;
}

static char32_t parse_ms_carka(layout_cz_t *cz_state, kbd_event_t *ev)
{
	char32_t c;

	cz_state->mstate = ms_start;

	/* Produce no characters when Ctrl or Alt is pressed. */
	if ((ev->mods & (KM_CTRL | KM_ALT)) != 0)
		return 0;

	if (((ev->mods & KM_SHIFT) != 0) ^ ((ev->mods & KM_CAPS_LOCK) != 0))
		c = translate(ev->key, map_carka_ucase, sizeof(map_carka_ucase) / sizeof(char32_t));
	else
		c = translate(ev->key, map_carka_lcase, sizeof(map_carka_lcase) / sizeof(char32_t));

	return c;
}

static char32_t parse_ms_start(layout_cz_t *cz_state, kbd_event_t *ev)
{
	char32_t c;

	/* Produce no characters when Ctrl or Alt is pressed. */
	if ((ev->mods & (KM_CTRL | KM_ALT)) != 0)
		return 0;

	if (ev->key == KC_EQUALS) {
		if ((ev->mods & KM_SHIFT) != 0)
			cz_state->mstate = ms_hacek;
		else
			cz_state->mstate = ms_carka;

		return 0;
	}

	c = translate(ev->key, map_neutral, sizeof(map_neutral) / sizeof(char32_t));
	if (c != 0)
		return c;

	if ((ev->mods & KM_SHIFT) == 0) {
		if ((ev->mods & KM_CAPS_LOCK) != 0)
			c = translate(ev->key, map_ns_caps, sizeof(map_ns_caps) / sizeof(char32_t));
		else
			c = translate(ev->key, map_ns_nocaps, sizeof(map_ns_nocaps) / sizeof(char32_t));

		if (c != 0)
			return c;
	}

	if (((ev->mods & KM_SHIFT) != 0) ^ ((ev->mods & KM_CAPS_LOCK) != 0))
		c = translate(ev->key, map_ucase, sizeof(map_ucase) / sizeof(char32_t));
	else
		c = translate(ev->key, map_lcase, sizeof(map_lcase) / sizeof(char32_t));

	if (c != 0)
		return c;

	if ((ev->mods & KM_SHIFT) != 0)
		c = translate(ev->key, map_shifted, sizeof(map_shifted) / sizeof(char32_t));
	else
		c = translate(ev->key, map_not_shifted, sizeof(map_not_shifted) / sizeof(char32_t));

	if (c != 0)
		return c;

	if ((ev->mods & KM_NUM_LOCK) != 0)
		c = translate(ev->key, map_numeric, sizeof(map_numeric) / sizeof(char32_t));
	else
		c = 0;

	return c;
}

static bool key_is_mod(unsigned key)
{
	switch (key) {
	case KC_LSHIFT:
	case KC_RSHIFT:
	case KC_LALT:
	case KC_RALT:
	case KC_LCTRL:
	case KC_RCTRL:
		return true;
	default:
		return false;
	}
}

static errno_t cz_create(layout_t *state)
{
	layout_cz_t *cz_state;

	cz_state = malloc(sizeof(layout_cz_t));
	if (cz_state == NULL) {
		printf("%s: Out of memory.\n", NAME);
		return ENOMEM;
	}

	cz_state->mstate = ms_start;
	state->layout_priv = (void *) cz_state;

	return EOK;
}

static void cz_destroy(layout_t *state)
{
	free(state->layout_priv);
}

static char32_t cz_parse_ev(layout_t *state, kbd_event_t *ev)
{
	layout_cz_t *cz_state = (layout_cz_t *) state->layout_priv;

	if (ev->type != KEY_PRESS)
		return 0;

	if (key_is_mod(ev->key))
		return 0;

	switch (cz_state->mstate) {
	case ms_start:
		return parse_ms_start(cz_state, ev);
	case ms_hacek:
		return parse_ms_hacek(cz_state, ev);
	case ms_carka:
		return parse_ms_carka(cz_state, ev);
	}

	return 0;
}

/**
 * @}
 */
