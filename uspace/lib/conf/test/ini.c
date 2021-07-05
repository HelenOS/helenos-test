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

#include <assert.h>
#include <conf/ini.h>
#include <pcut/pcut.h>
#include <str.h>

PCUT_INIT

PCUT_TEST_SUITE(ini);

static ini_configuration_t ini_conf;
static text_parse_t parse;

PCUT_TEST_BEFORE
{
	ini_configuration_init(&ini_conf);
	text_parse_init(&parse);
}

PCUT_TEST_AFTER
{
	text_parse_deinit(&parse);
	ini_configuration_deinit(&ini_conf);
}

PCUT_TEST(simple_parsing)
{
	const char *data =
	    "[Section]\n"
	    "key = value\n"
	    "key2 = value2\n";

	int rc = ini_parse_string(data, &ini_conf, &parse);

	PCUT_ASSERT_INT_EQUALS(rc, EOK);

	ini_section_t *section = ini_get_section(&ini_conf, "Section");
	PCUT_ASSERT_TRUE(section != NULL);

	ini_item_iterator_t it = ini_section_get_iterator(section, "key");
	PCUT_ASSERT_TRUE(ini_item_iterator_valid(&it));

	PCUT_ASSERT_STR_EQUALS(ini_item_iterator_value(&it), "value");
}

PCUT_TEST(default_section)
{
	const char *data =
	    "key = value\n"
	    "key2 = value2\n";

	int rc = ini_parse_string(data, &ini_conf, &parse);

	PCUT_ASSERT_INT_EQUALS(rc, EOK);

	ini_section_t *section = ini_get_section(&ini_conf, NULL);
	PCUT_ASSERT_TRUE(section != NULL);

	ini_item_iterator_t it = ini_section_get_iterator(section, "key");
	PCUT_ASSERT_TRUE(ini_item_iterator_valid(&it));

	PCUT_ASSERT_STR_EQUALS(ini_item_iterator_value(&it), "value");
}

PCUT_TEST(multikey)
{
	const char *data =
	    "key = value\n"
	    "key = value2\n";

	int rc = ini_parse_string(data, &ini_conf, &parse);

	PCUT_ASSERT_INT_EQUALS(rc, EOK);

	ini_section_t *section = ini_get_section(&ini_conf, NULL);
	PCUT_ASSERT_TRUE(section != NULL);

	ini_item_iterator_t it = ini_section_get_iterator(section, "key");
	PCUT_ASSERT_TRUE(ini_item_iterator_valid(&it));
	const char *first = ini_item_iterator_value(&it);

	ini_item_iterator_inc(&it);
	PCUT_ASSERT_TRUE(ini_item_iterator_valid(&it));
	const char *second = ini_item_iterator_value(&it);

	PCUT_ASSERT_TRUE(
	    (str_cmp(first, "value") == 0 && str_cmp(second, "value2") == 0) ||
	    (str_cmp(first, "value2") == 0 && str_cmp(second, "value") == 0));

	ini_item_iterator_inc(&it);
	PCUT_ASSERT_FALSE(ini_item_iterator_valid(&it));
}

PCUT_TEST(dup_section)
{
	const char *data =
	    "[Section]\n"
	    "key = value\n"
	    "key = value2\n"
	    "[Section]\n"
	    "key = val\n";

	int rc = ini_parse_string(data, &ini_conf, &parse);

	PCUT_ASSERT_INT_EQUALS(rc, EINVAL);
	PCUT_ASSERT_TRUE(parse.has_error);

	link_t *li = list_first(&parse.errors);
	text_parse_error_t *error = list_get_instance(li, text_parse_error_t, link);

	PCUT_ASSERT_INT_EQUALS(error->parse_errno, INI_EDUP_SECTION);
}

PCUT_TEST(empty_section)
{
	const char *data =
	    "[Section1]\n"
	    "[Section2]\n"
	    "key = value\n"
	    "key2 = value2\n";

	int rc = ini_parse_string(data, &ini_conf, &parse);

	PCUT_ASSERT_INT_EQUALS(rc, EOK);

	ini_section_t *section = ini_get_section(&ini_conf, "Section1");
	PCUT_ASSERT_TRUE(section != NULL);

	ini_item_iterator_t it = ini_section_get_iterator(section, "key");
	PCUT_ASSERT_TRUE(!ini_item_iterator_valid(&it));
}

PCUT_EXPORT(ini);
