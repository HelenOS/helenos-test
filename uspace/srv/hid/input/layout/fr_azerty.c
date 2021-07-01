/*
 * Copyright (c) 2019 CHTCHEGLOV Ilia
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
 * @brief French AZERTY layout
 */

#include <errno.h>
#include <io/console.h>
#include <io/keycode.h>
#include "../layout.h"
#include "../kbd.h"

static errno_t fr_azerty_create (layout_t *);
static void fr_azerty_destroy (layout_t *);
static char32_t fr_azerty_parse_ev (layout_t *, kbd_event_t *);

layout_ops_t fr_azerty_ops = {
	.create = fr_azerty_create,
	.destroy = fr_azerty_destroy,
	.parse_ev = fr_azerty_parse_ev,
	.name = "fr_azerty"
};

static char32_t map_lcase[] = {
	[KC_Q] = 'a',
	[KC_W] = 'z',
	[KC_E] = 'e',
	[KC_R] = 'r',
	[KC_T] = 't',
	[KC_Y] = 'y',
	[KC_U] = 'u',
	[KC_I] = 'i',
	[KC_O] = 'o',
	[KC_P] = 'p',

	[KC_A] = 'q',
	[KC_S] = 's',
	[KC_D] = 'd',
	[KC_F] = 'f',
	[KC_G] = 'g',
	[KC_H] = 'h',
	[KC_J] = 'j',
	[KC_K] = 'k',
	[KC_L] = 'l',

	[KC_Z] = 'w',
	[KC_X] = 'x',
	[KC_C] = 'c',
	[KC_V] = 'v',
	[KC_B] = 'b',
	[KC_N] = 'n',
	[KC_M] = ',',
};

static char32_t map_ucase[] = {
	[KC_Q] = 'A',
	[KC_W] = 'Z',
	[KC_E] = 'E',
	[KC_R] = 'R',
	[KC_T] = 'T',
	[KC_Y] = 'Y',
	[KC_U] = 'U',
	[KC_I] = 'I',
	[KC_O] = 'O',
	[KC_P] = 'P',

	[KC_A] = 'Q',
	[KC_S] = 'S',
	[KC_D] = 'D',
	[KC_F] = 'F',
	[KC_G] = 'G',
	[KC_H] = 'H',
	[KC_J] = 'J',
	[KC_K] = 'K',
	[KC_L] = 'L',

	[KC_Z] = 'W',
	[KC_X] = 'X',
	[KC_C] = 'C',
	[KC_V] = 'V',
	[KC_B] = 'B',
	[KC_N] = 'N',
	[KC_2] = L'É',
	[KC_7] = L'È',
	[KC_9] = L'Ç',
	[KC_0] = L'À',
	[KC_M] = ','
};

static char32_t map_not_shifted[] = {
	[KC_BACKTICK] = L'²',

	[KC_1] = '&',
	[KC_2] = L'é',
	[KC_3] = '"',
	[KC_4] = '\'',
	[KC_5] = '(',
	[KC_6] = '-',
	[KC_7] = L'è',
	[KC_8] = '_',
	[KC_9] = L'ç',
	[KC_0] = L'à',

	[KC_MINUS] = ')',
	[KC_EQUALS] = '=',

	[KC_LBRACKET] = '^',
	[KC_RBRACKET] = '$',

	[KC_SEMICOLON] = 'm',
	[KC_QUOTE] = L'ù',
	[KC_BACKSLASH] = '*',

	[KC_COMMA] = ';',
	[KC_PERIOD] = ':',
	[KC_SLASH] = '!',
};

static char32_t map_shifted[] = {
	[KC_M] = '?',
	[KC_BACKTICK] = '~',

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

	[KC_MINUS] = L'°',
	[KC_EQUALS] = '+',

	[KC_LBRACKET] = L'¨',
	[KC_RBRACKET] = L'£',

	[KC_SEMICOLON] = 'M',
	[KC_QUOTE] = '%',
	[KC_BACKSLASH] = L'µ',

	[KC_COMMA] = '.',
	[KC_PERIOD] = '/',
	[KC_SLASH] = '!',
};

static char32_t map_neutral[] = {
	[KC_BACKSPACE] = '\b',
	[KC_TAB] = '\t',
	[KC_ENTER] = '\n',
	[KC_SPACE] = ' ',

	[KC_NSLASH] = '!',
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

static char32_t translate (unsigned int key, char32_t *map, size_t map_len)
{
	if (key >= map_len)
		return 0;
	return map[key];
}

static errno_t fr_azerty_create (layout_t *s)
{
	return EOK;
}

static void fr_azerty_destroy (layout_t *s)
{

}

static char32_t fr_azerty_parse_ev (layout_t *s, kbd_event_t *e)
{
	if ((e->mods & (KM_CTRL | KM_ALT)))
		return 0; // Produce no characters when Ctrl or Alt is pressed

	char32_t c = translate (e->key, map_neutral, sizeof (map_neutral) / sizeof (char32_t));
	if (c)
		return c;

	if ((e->mods & KM_SHIFT))
		c = translate (e->key, map_shifted, sizeof (map_shifted) / sizeof (char32_t));
	else
		c = translate (e->key, map_not_shifted, sizeof (map_not_shifted) / sizeof (char32_t));

	if (c)
		return c;

	if (((e->mods & KM_SHIFT)) ^ ((e->mods & KM_CAPS_LOCK)))
		c = translate (e->key, map_ucase, sizeof (map_ucase) / sizeof (char32_t));
	else
		c = translate (e->key, map_lcase, sizeof (map_lcase) / sizeof (char32_t));

	if (c)
		return c;

	if ((e->mods & KM_NUM_LOCK))
		c = translate (e->key, map_numeric, sizeof (map_numeric) / sizeof (char32_t));
	else
		c = 0;

	return c;
}

/**
 * @}
 */
