/*
 * Copyright (c) 2011 Jiri Svoboda
 * Copyright (c) 2012 Mohammed Q. Hussain
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
 * @brief Arabic keyboard layout (Based on US QWERTY layout's code)
 */

#include <errno.h>
#include <io/console.h>
#include <io/keycode.h>
#include "../layout.h"
#include "../kbd.h"

static errno_t ar_create(layout_t *);
static void ar_destroy(layout_t *);
static char32_t ar_parse_ev(layout_t *, kbd_event_t *ev);

layout_ops_t ar_ops = {
	.create = ar_create,
	.destroy = ar_destroy,
	.parse_ev = ar_parse_ev,
	.name = "ar"
};

static char32_t map_not_shifted[] = {
	[KC_BACKTICK] = L'ذ',

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

	[KC_LBRACKET] = L'ج',
	[KC_RBRACKET] = L'د',

	[KC_SEMICOLON] = L'ك',
	[KC_QUOTE] = L'ط',
	[KC_BACKSLASH] = '\\',

	[KC_COMMA] = L'و',
	[KC_PERIOD] = L'ز',
	[KC_SLASH] = L'ظ',

	[KC_Q] = L'ض',
	[KC_W] = L'ص',
	[KC_E] = L'ث',
	[KC_R] = L'ق',
	[KC_T] = L'ف',
	[KC_Y] = L'غ',
	[KC_U] = L'ع',
	[KC_I] = L'ه',
	[KC_O] = L'خ',
	[KC_P] = L'ح',

	[KC_A] = L'ش',
	[KC_S] = L'س',
	[KC_D] = L'ي',
	[KC_F] = L'ب',
	[KC_G] = L'ل',
	[KC_H] = L'ا',
	[KC_J] = L'ت',
	[KC_K] = L'ن',
	[KC_L] = L'م',

	[KC_Z] = L'ئ',
	[KC_X] = L'ء',
	[KC_C] = L'ؤ',
	[KC_V] = L'ر',
	[KC_B] = L'ﻻ',
	[KC_N] = L'ى',
	[KC_M] = L'ة',
};

static char32_t map_shifted[] = {
	[KC_BACKTICK] = L'ّ',

	[KC_1] = '!',
	[KC_2] = '@',
	[KC_3] = '#',
	[KC_4] = '$',
	[KC_5] = '%',
	[KC_6] = '^',
	[KC_7] = '&',
	[KC_8] = '*',
	[KC_9] = ')',
	[KC_0] = '(',

	[KC_MINUS] = '_',
	[KC_EQUALS] = '+',

	[KC_LBRACKET] = '<',
	[KC_RBRACKET] = '>',

	[KC_SEMICOLON] = ':',
	[KC_QUOTE] = '"',
	[KC_BACKSLASH] = '|',

	[KC_COMMA] = ',',
	[KC_PERIOD] = '.',
	[KC_SLASH] = L'؟',

	[KC_Q] = L'َ',
	[KC_W] = L'ً',
	[KC_E] = L'ُ',
	[KC_R] = L'ٌ',
	[KC_T] = L'ﻹ',
	[KC_Y] = L'إ',
	[KC_U] = L'`',
	[KC_I] = L'÷',
	[KC_O] = L'×',
	[KC_P] = L'؛',

	[KC_A] = L'ِ',
	[KC_S] = L'ٍ',
	[KC_D] = L']',
	[KC_F] = L'[',
	[KC_G] = L'ﻷ',
	[KC_H] = L'أ',
	[KC_J] = L'ـ',
	[KC_K] = L'،',
	[KC_L] = L'/',

	[KC_Z] = L'~',
	[KC_X] = L'ْ',
	[KC_C] = L'}',
	[KC_V] = L'{',
	[KC_B] = L'ﻵ',
	[KC_N] = L'آ',
	[KC_M] = L'\'',
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

static errno_t ar_create(layout_t *state)
{
	return EOK;
}

static void ar_destroy(layout_t *state)
{
}

static char32_t ar_parse_ev(layout_t *state, kbd_event_t *ev)
{
	char32_t c;

	/* Produce no characters when Ctrl or Alt is pressed. */
	if ((ev->mods & (KM_CTRL | KM_ALT)) != 0)
		return 0;

	c = translate(ev->key, map_neutral, sizeof(map_neutral) / sizeof(char32_t));
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
