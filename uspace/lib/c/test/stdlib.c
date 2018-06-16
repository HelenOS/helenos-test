/*
 * Copyright (c) 2018 Jiri Svoboda
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
/**
 * @file
 * @brief Test formatted input (scanf family)
 */

#include <pcut/pcut.h>
#include <stdlib.h>

PCUT_INIT;

PCUT_TEST_SUITE(stdlib);

PCUT_TEST(decls)
{
	/* Make sure size_t is defined */
	size_t sz = 0;
	(void) sz;

	/* Make sure wchar_t is defined */
	wchar_t wc = L'\0';
	(void) wc;

	/* Make sure EXIT_FAILURE and EXIT_SUCCESS are defined */
	if (0)
		exit(EXIT_FAILURE);
	if (0)
		exit(EXIT_SUCCESS);

	/* Make sure NULL is defined */
	void *ptr = NULL;
	(void) ptr;
}

/** atoi function */
PCUT_TEST(atoi)
{
	int i;

	i = atoi(" \t42");
	PCUT_ASSERT_TRUE(i == 42);
}

/** atol function */
PCUT_TEST(atol)
{
	long li;

	li = atol(" \t42");
	PCUT_ASSERT_TRUE(li == 42);
}

/** atoll function */
PCUT_TEST(atoll)
{
	long long lli;

	lli = atoll(" \t42");
	PCUT_ASSERT_TRUE(lli == 42);
}

/** strtol function */
PCUT_TEST(strtol)
{
	long li;
	char *ep;

	li = strtol(" \t42x", &ep, 10);
	PCUT_ASSERT_TRUE(li == 42);
	PCUT_ASSERT_TRUE(*ep == 'x');
}

/** strtol function with auto-detected base 10 */
PCUT_TEST(strtol_dec_auto)
{
	long li;
	char *ep;

	li = strtol(" \t42x", &ep, 0);
	PCUT_ASSERT_TRUE(li == 42);
	PCUT_ASSERT_TRUE(*ep == 'x');
}

/** strtol function with octal number */
PCUT_TEST(strtol_oct)
{
	long li;
	char *ep;

	li = strtol(" \t052x", &ep, 8);
	PCUT_ASSERT_TRUE(li == 052);
	PCUT_ASSERT_TRUE(*ep == 'x');
}

/** strtol function with octal number with prefix */
#include <stdio.h>
PCUT_TEST(strtol_oct_prefix)
{
	long li;
	char *ep;

	li = strtol(" \t052x", &ep, 0);
	printf("li=%ld (0%lo)\n", li, li);
	PCUT_ASSERT_TRUE(li == 052);
	PCUT_ASSERT_TRUE(*ep == 'x');
}

/** strtol function with hex number */
PCUT_TEST(strtol_hex)
{
	long li;
	char *ep;

	li = strtol(" \t2ax", &ep, 16);
	PCUT_ASSERT_TRUE(li == 0x2a);
	PCUT_ASSERT_TRUE(*ep == 'x');
}

/** strtol function with hex number with hex prefix */
PCUT_TEST(strtol_hex_prefixed)
{
	long li;
	char *ep;

	li = strtol(" \t0x2ax", &ep, 0);
	PCUT_ASSERT_TRUE(li == 0x2a);
	PCUT_ASSERT_TRUE(*ep == 'x');
}

/** strtol function with base 16 and number with 0x prefix */
PCUT_TEST(strtol_base16_prefix)
{
	long li;
	char *ep;

	li = strtol(" \t0x1y", &ep, 16);
	printf("li=%ld\n", li);
	PCUT_ASSERT_TRUE(li == 1);
	PCUT_ASSERT_TRUE(*ep == 'y');
}

/** strtol function with base 36 number */
PCUT_TEST(strtol_base36)
{
	long li;
	char *ep;

	li = strtol(" \tz1.", &ep, 36);
	PCUT_ASSERT_TRUE(li == 35 * 36 + 1);
	PCUT_ASSERT_TRUE(*ep == '.');
}

/** rand function */
PCUT_TEST(rand)
{
	int i;
	int r;

	for (i = 0; i < 100; i++) {
		r = rand();
		PCUT_ASSERT_TRUE(r >= 0);
		PCUT_ASSERT_TRUE(r <= RAND_MAX);
	}

	PCUT_ASSERT_TRUE(RAND_MAX >= 32767);
}

/** srand function */
PCUT_TEST(srand)
{
	int r1;
	int r2;

	srand(1);
	r1 = rand();
	srand(1);
	r2 = rand();

	PCUT_ASSERT_INT_EQUALS(r2, r1);

	srand(42);
	r1 = rand();
	srand(42);
	r2 = rand();

	PCUT_ASSERT_INT_EQUALS(r2, r1);
}

/** Just make sure we have memory allocation function prototypes */
PCUT_TEST(malloc)
{
	void *p;

#if 0
	// TODO
	p = aligned_alloc(4, 8);
	PCUT_ASSERT_NOT_NULL(p);
	free(p);
#endif
	p = calloc(4, 4);
	PCUT_ASSERT_NOT_NULL(p);
	free(p);

	p = malloc(4);
	PCUT_ASSERT_NOT_NULL(p);
	p = realloc(p, 2);
	PCUT_ASSERT_NOT_NULL(p);
	free(p);
}

/** Just check abort() is defined */
PCUT_TEST(abort)
{
	if (0)
		abort();
}

static void dummy_exit_handler(void)
{
}

/** atexit function */
PCUT_TEST(atexit)
{
	int rc;

	rc = atexit(dummy_exit_handler);
	PCUT_ASSERT_INT_EQUALS(0, rc);
}

/** exit function -- just make sure it is declared */
PCUT_TEST(exit)
{
	if (0)
		exit(0);
}

/** at_quick_exit function */
PCUT_TEST(at_quick_exit)
{
	int rc;

	rc = at_quick_exit(dummy_exit_handler);
	PCUT_ASSERT_INT_EQUALS(0, rc);
}

/** quick_exit function -- just make sure it is declared */
PCUT_TEST(quick_exit)
{
	if (0)
		quick_exit(0);
}

/** Integer division */
PCUT_TEST(div_func)
{
	div_t d;

	d = div(41, 7);
	PCUT_ASSERT_INT_EQUALS(5, d.quot);
	PCUT_ASSERT_INT_EQUALS(6, d.rem);
}

/** Long integer division */
PCUT_TEST(ldiv_func)
{
	ldiv_t d;

	d = ldiv(41, 7);
	PCUT_ASSERT_INT_EQUALS(5, d.quot);
	PCUT_ASSERT_INT_EQUALS(6, d.rem);
}

/** Long long integer division */
PCUT_TEST(lldiv_func)
{
	lldiv_t d;

	d = lldiv(41, 7);
	PCUT_ASSERT_INT_EQUALS(5, d.quot);
	PCUT_ASSERT_INT_EQUALS(6, d.rem);
}

PCUT_EXPORT(stdlib);
