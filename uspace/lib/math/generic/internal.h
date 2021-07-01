/*
 * Copyright (c) 2015 Jiri Svoboda
 * Copyright (c) 2014 Martin Decky
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

#ifndef MATH_INTERNAL_H_
#define MATH_INTERNAL_H_

#include <stdint.h>

#if __BE__

typedef union
{
	double value;
	struct
	{
		uint32_t msw;
		uint32_t lsw;
	} parts;
	uint64_t word;
} ieee_double_shape_type;

#endif

#if __LE__

typedef union
{
	double value;
	struct
	{
		uint32_t lsw;
		uint32_t msw;
	} parts;
	uint64_t word;
} ieee_double_shape_type;

#endif

#define EXTRACT_WORDS(ix0,ix1,d)                                \
do {                                                                \
  ieee_double_shape_type ew_u;                                        \
  ew_u.value = (d);                                                \
  (ix0) = ew_u.parts.msw;                                        \
  (ix1) = ew_u.parts.lsw;                                        \
} while (0)

/* Get the more significant 32 bit int from a double.  */
#ifndef GET_HIGH_WORD
# define GET_HIGH_WORD(i,d)                                        \
do {                                                                \
  ieee_double_shape_type gh_u;                                        \
  gh_u.value = (d);                                                \
  (i) = gh_u.parts.msw;                                                \
} while (0)
#endif

/* Get the less significant 32 bit int from a double.  */
#ifndef GET_LOW_WORD
# define GET_LOW_WORD(i,d)                                        \
do {                                                                \
  ieee_double_shape_type gl_u;                                        \
  gl_u.value = (d);                                                \
  (i) = gl_u.parts.lsw;                                                \
} while (0)
#endif

/* Get all in one, efficient on 64-bit machines.  */
#ifndef EXTRACT_WORDS64
# define EXTRACT_WORDS64(i,d)                                        \
do {                                                                \
  ieee_double_shape_type gh_u;                                        \
  gh_u.value = (d);                                                \
  (i) = gh_u.word;                                                \
} while (0)
#endif

/* Set a double from two 32 bit ints.  */
#ifndef INSERT_WORDS
# define INSERT_WORDS(d,ix0,ix1)                                \
do {                                                                \
  ieee_double_shape_type iw_u;                                        \
  iw_u.parts.msw = (ix0);                                        \
  iw_u.parts.lsw = (ix1);                                        \
  (d) = iw_u.value;                                                \
} while (0)
#endif

/* Get all in one, efficient on 64-bit machines.  */
#ifndef INSERT_WORDS64
# define INSERT_WORDS64(d,i)                                        \
do {                                                                \
  ieee_double_shape_type iw_u;                                        \
  iw_u.word = (i);                                                \
  (d) = iw_u.value;                                                \
} while (0)
#endif

/* Set the more significant 32 bits of a double from an int.  */
#ifndef SET_HIGH_WORD
#define SET_HIGH_WORD(d,v)                                        \
do {                                                                \
  ieee_double_shape_type sh_u;                                        \
  sh_u.value = (d);                                                \
  sh_u.parts.msw = (v);                                                \
  (d) = sh_u.value;                                                \
} while (0)
#endif

/* Set the less significant 32 bits of a double from an int.  */
#ifndef SET_LOW_WORD
# define SET_LOW_WORD(d,v)                                        \
do {                                                                \
  ieee_double_shape_type sl_u;                                        \
  sl_u.value = (d);                                                \
  sl_u.parts.lsw = (v);                                                \
  (d) = sl_u.value;                                                \
} while (0)
#endif


float __math_base_sin_32(float);
float __math_base_cos_32(float);
double __math_base_sin_64(double);
double __math_base_cos_64(double);

#endif
