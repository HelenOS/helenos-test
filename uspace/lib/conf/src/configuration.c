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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AS IS'' AND ANY EXPRESS OR
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

#include "conf/configuration.h"

#include <assert.h>
#include <errno.h>
#include <str.h>

static int config_load_item(config_item_t *config_item,
    ini_item_iterator_t *it, void *dst, text_parse_t *parse)
{
	size_t cnt = 0;
	void *field_dst = (char *)dst + config_item->offset;
	bool has_error = false;

	for (; ini_item_iterator_valid(it); ini_item_iterator_inc(it), ++cnt) {
		const char *string = ini_item_iterator_value(it);
		const size_t lineno = ini_item_iterator_lineno(it);
		bool success = config_item->parse(string, field_dst, parse, lineno);
		has_error = has_error || !success;
	}

	if (cnt == 0) {
		if (config_item->default_value == NULL) {
			return ENOENT;
		}
		bool result = config_item->parse(config_item->default_value,
		    field_dst, parse, 0);
		/* Default string should be always correct */
		assert(result);
	}

	return has_error ? EINVAL : EOK;
}

/** Process INI section as values to a structure
 *
 * @param[in]  specification  Mark-terminated array of config_item_t specifying
 *                            available configuration values. The mark is value
 *                            whose name is NULL, you can use
 *                            CONFIGURATION_ITEM_SENTINEL.
 * @param[in]  section        INI section with raw string data
 * @param[out] dst            pointer to structure that holds parsed values
 * @param[out] parse          structure for recording any parsing errors
 *
 * @return EOK on success
 * @return EINVAL on any parsing errors (details in parse structure)
 */
int config_load_ini_section(config_item_t *specification,
    ini_section_t *section, void *dst, text_parse_t *parse)
{
	bool has_error = false;

	config_item_t *config_item = specification;
	while (config_item->name != NULL) {
		ini_item_iterator_t iterator =
		    ini_section_get_iterator(section, config_item->name);

		int rc = config_load_item(config_item, &iterator, dst, parse);
		switch (rc) {
		case ENOENT:
			has_error = true;
			text_parse_raise_error(parse, section->lineno,
			    CONFIGURATION_EMISSING_ITEM);
			break;
		case EINVAL:
			has_error = true;
			/* Parser should've raised proper errors */
			break;
		case EOK:
			/* empty (nothing to do) */
			break;
		default:
			assert(false);
		}

		++config_item;
	}

	return has_error ? EINVAL : EOK;
}

/** Parse string (copy) to destination
 *
 * @param[out]  dst      pointer to char * where dedicated copy will be stored
 *
 * @return  true   on success
 * @return  false  on error (typically low memory)
 */
bool config_parse_string(const char *string, void *dst, text_parse_t *parse,
    size_t lineno)
{
	char *my_string = str_dup(string);
	if (my_string == NULL) {
		return false;
	}

	char **char_dst = dst;
	*char_dst = my_string;
	return true;
}

/** Parse boolean value
 *
 * @param[out]  dst      pointer to bool
 *
 * @return  true   on success
 * @return  false  on parse error
 */
bool config_parse_bool(const char *string, void *dst, text_parse_t *parse,
    size_t lineno)
{
	bool value;
	if (str_casecmp(string, "true") == 0 ||
	    str_casecmp(string, "yes") == 0 ||
	    str_casecmp(string, "1") == 0) {
		value = true;
	} else if (str_casecmp(string, "false") == 0 ||
	    str_casecmp(string, "no") == 0 ||
	    str_casecmp(string, "0") == 0) {
		value = false;
	} else {
		text_parse_raise_error(parse, lineno,
		    CONFIGURATION_EINVAL_BOOL);
		return false;
	}

	bool *bool_dst = dst;
	*bool_dst = value;
	return true;
}
