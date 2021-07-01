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
 * @brief US QWERTY layout
 */

#include <errno.h>
#include <io/console.h>
#include <io/keycode.h>
#include "../layout.h"
#include "../kbd.h"

static errno_t us_qwerty_create(layout_t *);
static void us_qwerty_destroy(layout_t *);
static char32_t us_qwerty_parse_ev(layout_t *, kbd_event_t *ev);

layout_ops_t us_qwerty_ops = {
	.create = us_qwerty_create,
	.destroy = us_qwerty_destroy,
	.parse_ev = us_qwerty_parse_ev,
	.name = "us_qwerty"
};

static char32_t map_lcase[] = {
	[KC_Q] = 'q',
	[KC_W] = 'w',
	[KC_E] = 'e',
	[KC_R] = 'r',
	[KC_T] = 't',
	[KC_Y] = 'y',
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

	[KC_Z] = 'z',
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
	[KC_Y] = 'Y',
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

	[KC_Z] = 'Z',
	[KC_X] = 'X',
	[KC_C] = 'C',
	[KC_V] = 'V',
	[KC_B] = 'B',
	[KC_N] = 'N',
	[KC_M] = 'M',
};

static char32_t map_not_shifted[] = {
	[KC_BACKTICK] = '`',

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

	[KC_MINUS] = '-',
	[KC_EQUALS] = '=',

	[KC_LBRACKET] = '[',
	[KC_RBRACKET] = ']',

	[KC_SEMICOLON] = ';',
	[KC_QUOTE] = '\'',
	[KC_BACKSLASH] = '\\',

	[KC_COMMA] = ',',
	[KC_PERIOD] = '.',
	[KC_SLASH] = '/',
};

static char32_t map_shifted[] = {
	[KC_BACKTICK] = '~',

	[KC_1] = '!',
	[KC_2] = '@',
	[KC_3] = '#',
	[KC_4] = '$',
	[KC_5] = '%',
	[KC_6] = '^',
	[KC_7] = '&',
	[KC_8] = '*',
	[KC_9] = '(',
	[KC_0] = ')',

	[KC_MINUS] = '_',
	[KC_EQUALS] = '+',

	[KC_LBRACKET] = '{',
	[KC_RBRACKET] = '}',

	[KC_SEMICOLON] = ':',
	[KC_QUOTE] = '"',
	[KC_BACKSLASH] = '|',

	[KC_COMMA] = '<',
	[KC_PERIOD] = '>',
	[KC_SLASH] = '?',
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

static char32_t translate(unsigned int key, char32_t *map, size_t map_length)
{
	if (key >= map_length)
		return 0;
	return map[key];
}

static errno_t us_qwerty_create(layout_t *state)
{
	return EOK;
}

static void us_qwerty_destroy(layout_t *state)
{
}

static char32_t us_qwerty_parse_ev(layout_t *state, kbd_event_t *ev)
{
	char32_t c;

	/* Produce no characters when Ctrl or Alt is pressed. */
	if ((ev->mods & (KM_CTRL | KM_ALT)) != 0)
		return 0;

	c = translate(ev->key, map_neutral, sizeof(map_neutral) / sizeof(char32_t));
	if (c != 0)
		return c;

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

/**
 * @}
 */
