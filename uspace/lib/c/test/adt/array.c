/*
 * Copyright (c) 2015 Michal Koutny
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

#include <adt/array.h>
#include <assert.h>
#include <pcut/pcut.h>

PCUT_INIT

PCUT_TEST_SUITE(array);

typedef int data_t;
static array_t da;

PCUT_TEST_BEFORE
{
	array_initialize(&da, data_t);
	errno_t rc = array_reserve(&da, 3);
	assert(rc == EOK);
}

PCUT_TEST_AFTER
{
	array_destroy(&da);
}

PCUT_TEST(initialization)
{
	PCUT_ASSERT_INT_EQUALS(da.capacity, 3);
	PCUT_ASSERT_INT_EQUALS(da.size, 0);
}

PCUT_TEST(append)
{
	array_append(&da, data_t, 42);
	array_append(&da, data_t, 666);

	PCUT_ASSERT_INT_EQUALS(2, da.size);
	PCUT_ASSERT_INT_EQUALS(42, array_at(&da, data_t, 0));
	PCUT_ASSERT_INT_EQUALS(666, array_at(&da, data_t, 1));
}

PCUT_TEST(assign)
{
	array_append(&da, data_t, 42);
	array_at(&da, data_t, 0) = 112;

	PCUT_ASSERT_INT_EQUALS(112, array_at(&da, data_t, 0));
}

PCUT_TEST(remove)
{
	array_append(&da, data_t, 10);
	array_append(&da, data_t, 11);

	array_remove(&da, 0);

	PCUT_ASSERT_INT_EQUALS(1, da.size);
	PCUT_ASSERT_INT_EQUALS(11, array_at(&da, data_t, 0));
}

PCUT_TEST(insert)
{
	array_append(&da, data_t, 10);
	array_append(&da, data_t, 11);
	array_append(&da, data_t, 12);
	array_insert(&da, data_t, 1, 99);

	PCUT_ASSERT_INT_EQUALS(4, da.size);
	PCUT_ASSERT_INT_EQUALS(10, array_at(&da, data_t, 0));
	PCUT_ASSERT_INT_EQUALS(99, array_at(&da, data_t, 1));
	PCUT_ASSERT_INT_EQUALS(11, array_at(&da, data_t, 2));
	PCUT_ASSERT_INT_EQUALS(12, array_at(&da, data_t, 3));
}

PCUT_TEST(capacity_grow)
{
	array_append(&da, data_t, 42);
	array_append(&da, data_t, 666);
	array_append(&da, data_t, 42);
	array_append(&da, data_t, 666);

	PCUT_ASSERT_TRUE(da.capacity > 3);
}

PCUT_TEST(capacity_shrink)
{
	array_append(&da, data_t, 42);
	array_append(&da, data_t, 666);
	array_append(&da, data_t, 42);

	array_remove(&da, 0);
	array_remove(&da, 0);
	array_remove(&da, 0);

	PCUT_ASSERT_TRUE(da.capacity < 3);
}

PCUT_TEST(iterator)
{
	for (int i = 0; i < 10; ++i) {
		array_append(&da, data_t, i * i);
	}

	int i = 0;
	array_foreach(da, data_t, it) {
		PCUT_ASSERT_INT_EQUALS(i * i, *it);
		++i;
	}
}

PCUT_TEST(find)
{
	array_append(&da, data_t, 10);
	array_append(&da, data_t, 11);
	array_append(&da, data_t, 12);
	array_append(&da, data_t, 99);

	PCUT_ASSERT_INT_EQUALS(0, array_find(&da, data_t, 10));
	PCUT_ASSERT_INT_EQUALS(3, array_find(&da, data_t, 99));
	PCUT_ASSERT_INT_EQUALS(4, array_find(&da, data_t, 666));
}

PCUT_TEST(clear_range_middle)
{
	array_append(&da, data_t, 10);
	array_append(&da, data_t, 11);
	array_append(&da, data_t, 12);
	array_append(&da, data_t, 99);

	array_clear_range(&da, 1, 3);
	PCUT_ASSERT_INT_EQUALS(2, da.size);
	PCUT_ASSERT_INT_EQUALS(10, array_at(&da, data_t, 0));
	PCUT_ASSERT_INT_EQUALS(99, array_at(&da, data_t, 1));
}

PCUT_TEST(clear_range_begin)
{
	array_append(&da, data_t, 10);
	array_append(&da, data_t, 11);
	array_append(&da, data_t, 12);
	array_append(&da, data_t, 99);

	array_clear_range(&da, 0, 2);
	PCUT_ASSERT_INT_EQUALS(2, da.size);
	PCUT_ASSERT_INT_EQUALS(12, array_at(&da, data_t, 0));
	PCUT_ASSERT_INT_EQUALS(99, array_at(&da, data_t, 1));
}

PCUT_TEST(clear_range_end)
{
	array_append(&da, data_t, 10);
	array_append(&da, data_t, 11);
	array_append(&da, data_t, 12);
	array_append(&da, data_t, 99);

	array_clear_range(&da, 2, 4);
	PCUT_ASSERT_INT_EQUALS(2, da.size);
	PCUT_ASSERT_INT_EQUALS(10, array_at(&da, data_t, 0));
	PCUT_ASSERT_INT_EQUALS(11, array_at(&da, data_t, 1));
}

PCUT_TEST(clear_range_empty)
{
	array_append(&da, data_t, 10);
	array_append(&da, data_t, 99);

	array_clear_range(&da, 0, 0);
	PCUT_ASSERT_INT_EQUALS(2, da.size);
	PCUT_ASSERT_INT_EQUALS(10, array_at(&da, data_t, 0));
	PCUT_ASSERT_INT_EQUALS(99, array_at(&da, data_t, 1));
}

PCUT_TEST(concat_simple)
{
	array_append(&da, data_t, 10);
	array_append(&da, data_t, 99);

	array_t da2;
	array_initialize(&da2, data_t);
	array_append(&da2, data_t, 30);
	array_append(&da2, data_t, 31);

	array_concat(&da, &da2);
	PCUT_ASSERT_INT_EQUALS(4, da.size);
	PCUT_ASSERT_INT_EQUALS(2, da2.size);

	PCUT_ASSERT_INT_EQUALS(10, array_at(&da, data_t, 0));
	PCUT_ASSERT_INT_EQUALS(99, array_at(&da, data_t, 1));
	PCUT_ASSERT_INT_EQUALS(30, array_at(&da, data_t, 2));
	PCUT_ASSERT_INT_EQUALS(31, array_at(&da, data_t, 3));

	array_destroy(&da2);
}

PCUT_TEST(concat_self)
{
	array_append(&da, data_t, 10);
	array_append(&da, data_t, 99);

	array_concat(&da, &da);
	PCUT_ASSERT_INT_EQUALS(4, da.size);

	PCUT_ASSERT_INT_EQUALS(10, array_at(&da, data_t, 0));
	PCUT_ASSERT_INT_EQUALS(99, array_at(&da, data_t, 1));
	PCUT_ASSERT_INT_EQUALS(10, array_at(&da, data_t, 2));
	PCUT_ASSERT_INT_EQUALS(99, array_at(&da, data_t, 3));
}

PCUT_EXPORT(array);