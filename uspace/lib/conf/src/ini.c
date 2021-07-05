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

#include <adt/hash.h>
#include <adt/hash_table.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <str.h>

#include "conf/ini.h"

#define LINE_BUFFER 256

/** Representation of key-value pair from INI file
 *
 * @note Structure is owner of its string data.
 */
typedef struct {
	ht_link_t ht_link;

	/** First line from where the item was extracted. */
	size_t lineno;

	char *key;
	char *value;
} ini_item_t;

/** Line reader for generic parsing */
typedef char *(*line_reader_t)(char *, int, void *);

/* Necessary forward declarations */
static void ini_section_destroy(ini_section_t **);
static void ini_item_destroy(ini_item_t **);
static ini_section_t *ini_section_create(void);
static ini_item_t *ini_item_create(void);

/* Hash table functions */
static size_t ini_section_ht_hash(const ht_link_t *item)
{
	ini_section_t *section =
	    hash_table_get_inst(item, ini_section_t, ht_link);
	/* Nameless default section */
	if (section->name == NULL) {
		return 0;
	}

	return hash_string(section->name);
}

static size_t ini_section_ht_key_hash(const void *key)
{
	/* Nameless default section */
	if (key == NULL) {
		return 0;
	}
	return hash_string((const char *)key);
}

static bool ini_section_ht_equal(const ht_link_t *item1, const ht_link_t *item2)
{
	return str_cmp(
	    hash_table_get_inst(item1, ini_section_t, ht_link)->name,
	    hash_table_get_inst(item2, ini_section_t, ht_link)->name) == 0;
}

static bool ini_section_ht_key_equal(const void *key, const ht_link_t *item)
{
	const char *name = key;
	ini_section_t *section =
	    hash_table_get_inst(item, ini_section_t, ht_link);

	if (key == NULL || section->name == NULL) {
		return section->name == key;
	}

	return str_cmp(name, section->name) == 0;
}

static void ini_section_ht_remove(ht_link_t *item)
{
	ini_section_t *section = hash_table_get_inst(item, ini_section_t, ht_link);
	ini_section_destroy(&section);
}

static size_t ini_item_ht_hash(const ht_link_t *item)
{
	ini_item_t *ini_item =
	    hash_table_get_inst(item, ini_item_t, ht_link);
	assert(ini_item->key);
	return hash_string(ini_item->key);
}

static size_t ini_item_ht_key_hash(const void *key)
{
	return hash_string((const char *)key);
}

static bool ini_item_ht_equal(const ht_link_t *item1, const ht_link_t *item2)
{
	return str_cmp(
	    hash_table_get_inst(item1, ini_item_t, ht_link)->key,
	    hash_table_get_inst(item2, ini_item_t, ht_link)->key) == 0;
}

static bool ini_item_ht_key_equal(const void *key, const ht_link_t *item)
{
	return str_cmp((const char *)key,
	    hash_table_get_inst(item, ini_item_t, ht_link)->key) == 0;
}

static void ini_item_ht_remove(ht_link_t *item)
{
	ini_item_t *ini_item = hash_table_get_inst(item, ini_item_t, ht_link);
	ini_item_destroy(&ini_item);
}

static hash_table_ops_t configuration_ht_ops = {
	.hash            = &ini_section_ht_hash,
	.key_hash        = &ini_section_ht_key_hash,
	.equal           = &ini_section_ht_equal,
	.key_equal       = &ini_section_ht_key_equal,
	.remove_callback = &ini_section_ht_remove
};

static hash_table_ops_t section_ht_ops = {
	.hash            = &ini_item_ht_hash,
	.key_hash        = &ini_item_ht_key_hash,
	.equal           = &ini_item_ht_equal,
	.key_equal       = &ini_item_ht_key_equal,
	.remove_callback = &ini_item_ht_remove
};

/*
 * Static functions
 */
static char *read_file(char *buffer, int size, void *data)
{
	return fgets(buffer, size, (FILE *)data);
}

static char *read_string(char *buffer, int size, void *data)
{
	char **string_ptr = (char **)data;
	char *string = *string_ptr;

	int i = 0;
	while (i < size - 1) {
		char c = string[i];
		if (c == '\0') {
			break;
		}

		buffer[i++] = c;

		if (c == '\n') {
			break;
		}
	}

	if (i == 0) {
		return NULL;
	}

	buffer[i] = '\0';
	*string_ptr = string + i;
	return buffer;
}

static int ini_parse_generic(line_reader_t line_reader, void *reader_data,
    ini_configuration_t *conf, text_parse_t *parse)
{
	int rc = EOK;
	char *line_buffer = NULL;

	line_buffer = malloc(LINE_BUFFER);
	if (line_buffer == NULL) {
		rc = ENOMEM;
		goto finish;
	}

	char *line = NULL;
	ini_section_t *cur_section = NULL;
	size_t lineno = 0;

	while ((line = line_reader(line_buffer, LINE_BUFFER, reader_data))) {
		++lineno;
		size_t line_len = str_size(line);
		if (line[line_len - 1] != '\n') {
			text_parse_raise_error(parse, lineno, INI_ETOO_LONG);
			rc = EINVAL;
			/* Cannot recover terminate parsing */
			goto finish;
		}
		/* Ingore leading/trailing whitespace */
		str_rtrim(line, '\n');
		str_ltrim(line, ' ');
		str_rtrim(line, ' ');

		/* Empty line */
		if (str_size(line) == 0) {
			continue;
		}

		/* Comment line */
		if (line[0] == ';' || line[0] == '#') {
			continue;
		}

		/* Start new section */
		if (line[0] == '[') {
			cur_section = ini_section_create();
			if (cur_section == NULL) {
				rc = ENOMEM;
				goto finish;
			}

			char *close_bracket = str_chr(line, ']');
			if (close_bracket == NULL) {
				ini_section_destroy(&cur_section);
				text_parse_raise_error(parse, lineno,
				    INI_EBRACKET_EXPECTED);
				rc = EINVAL;
				goto finish;
			}

			cur_section->lineno = lineno;
			*close_bracket = '\0';
			cur_section->name = str_dup(line + 1);

			if (!hash_table_insert_unique(&conf->sections,
			    &cur_section->ht_link)) {
				ini_section_destroy(&cur_section);
				text_parse_raise_error(parse, lineno,
				    INI_EDUP_SECTION);
				rc = EINVAL;
				goto finish;
			}

			continue;
		}

		/* Create a default section if none was specified */
		if (cur_section == NULL) {
			cur_section = ini_section_create();
			if (cur_section == NULL) {
				rc = ENOMEM;
				goto finish;
			}
			cur_section->lineno = lineno;

			bool inserted = hash_table_insert_unique(&conf->sections,
			    &cur_section->ht_link);
			assert(inserted);
		}

		/* Parse key-value pairs */
		ini_item_t *item = ini_item_create();
		if (item == NULL) {
			rc = ENOMEM;
			goto finish;
		}
		item->lineno = lineno;

		char *assign_char = str_chr(line, '=');
		if (assign_char == NULL) {
			rc = EINVAL;
			text_parse_raise_error(parse, lineno,
			    INI_EASSIGN_EXPECTED);
			goto finish;
		}

		*assign_char = '\0';
		char *key = line;
		str_ltrim(key, ' ');
		str_rtrim(key, ' ');
		item->key = str_dup(key);
		if (item->key == NULL) {
			ini_item_destroy(&item);
			rc = ENOMEM;
			goto finish;
		}

		char *value = assign_char + 1;
		str_ltrim(value, ' ');
		str_rtrim(value, ' ');
		item->value = str_dup(value);
		if (item->value == NULL) {
			ini_item_destroy(&item);
			rc = ENOMEM;
			goto finish;
		}

		hash_table_insert(&cur_section->items, &item->ht_link);
	}

finish:
	free(line_buffer);

	return rc;
}

/*
 * Actual INI functions
 */

void ini_configuration_init(ini_configuration_t *conf)
{
	hash_table_create(&conf->sections, 0, 0, &configuration_ht_ops);
}

/** INI configuration destructor
 *
 * Release all resources of INI structure but the structure itself.
 */
void ini_configuration_deinit(ini_configuration_t *conf)
{
	hash_table_destroy(&conf->sections);
}

static void ini_section_init(ini_section_t *section)
{
	hash_table_create(&section->items, 0, 0, &section_ht_ops);
	section->name = NULL;
}

static ini_section_t *ini_section_create(void)
{
	ini_section_t *section = malloc(sizeof(ini_section_t));
	if (section != NULL) {
		ini_section_init(section);
	}
	return section;
}

static void ini_section_destroy(ini_section_t **section_ptr)
{
	ini_section_t *section = *section_ptr;
	if (section == NULL) {
		return;
	}
	hash_table_destroy(&section->items);
	free(section->name);
	free(section);
	*section_ptr = NULL;
}

static void ini_item_init(ini_item_t *item)
{
	item->key = NULL;
	item->value = NULL;
}

static ini_item_t *ini_item_create(void)
{
	ini_item_t *item = malloc(sizeof(ini_item_t));
	if (item != NULL) {
		ini_item_init(item);
	}
	return item;
}

static void ini_item_destroy(ini_item_t **item_ptr)
{
	ini_item_t *item = *item_ptr;
	if (item == NULL) {
		return;
	}
	free(item->key);
	free(item->value);
	free(item);
	*item_ptr = NULL;
}

/** Parse file contents to INI structure
 *
 * @param[in]    filename
 * @param[out]   conf      initialized structure for configuration
 * @param[out]   parse     initialized structure to keep parsing errors
 *
 * @return EOK on success
 * @return EIO when file cannot be opened
 * @return ENOMEM
 * @return EINVAL on parse error (details in parse structure)
 */
int ini_parse_file(const char *filename, ini_configuration_t *conf,
    text_parse_t *parse)
{
	FILE *f = NULL;
	f = fopen(filename, "r");
	if (f == NULL) {
		return EIO;
	}

	int rc = ini_parse_generic(&read_file, f, conf, parse);
	fclose(f);
	return rc;
}

/** Parse string to INI structure
 *
 * @param[in]    string
 * @param[out]   conf      initialized structure for configuration
 * @param[out]   parse     initialized structure to keep parsing errors
 *
 * @return EOK on success
 * @return ENOMEM
 * @return EINVAL on parse error (details in parse structure)
 */
int ini_parse_string(const char *string, ini_configuration_t *conf,
    text_parse_t *parse)
{
	const char *string_ptr = string;

	return ini_parse_generic(&read_string, &string_ptr, conf, parse);
}

/** Get a section from configuration
 *
 * @param[in]  ini_configuration
 * @param[in]  section_name       name of section or NULL for default section
 *
 * @return   Section with given name
 * @return   NULL when no such section exits
 */
ini_section_t *ini_get_section(ini_configuration_t *ini_conf,
    const char *section_name)
{
	ht_link_t *item = hash_table_find(&ini_conf->sections,
	    (void *)section_name);
	if (item == NULL) {
		return NULL;
	}

	return hash_table_get_inst(item, ini_section_t, ht_link);
}

/** Get item iterator to items with given key in the section
 *
 * @param[in]  section
 * @param[in]  key
 *
 * @return Always return iterator (even when there's no item with given key)
 */
ini_item_iterator_t ini_section_get_iterator(ini_section_t *section,
    const char *key)
{
	ini_item_iterator_t result;
	result.first_item = hash_table_find(&section->items, (void *)key);
	result.cur_item = result.first_item;
	result.table = &section->items;
	result.incremented = false;

	return result;
}

bool ini_item_iterator_valid(ini_item_iterator_t *iterator)
{
	bool empty = (iterator->cur_item == NULL);
	bool maybe_looped = (iterator->cur_item == iterator->first_item);
	return !(empty || (maybe_looped && iterator->incremented));
}

/** Move iterator to next item (of the same key)
 *
 * @param[in]  iterator  valid iterator
 */
void ini_item_iterator_inc(ini_item_iterator_t *iterator)
{
	iterator->cur_item =
	    hash_table_find_next(iterator->table, iterator->first_item, iterator->cur_item);
	iterator->incremented = true;
}

/** Get item value for current iterator
 *
 * @param[in]  iterator  valid iterator
 */
const char *ini_item_iterator_value(ini_item_iterator_t *iterator)
{
	ini_item_t *ini_item =
	    hash_table_get_inst(iterator->cur_item, ini_item_t, ht_link);
	return ini_item->value;
}

/** Get item line number for current iterator
 *
 * @param[in]  iterator  valid iterator
 *
 * @return Line number of input where item was originally defined.
 */
size_t ini_item_iterator_lineno(ini_item_iterator_t *iterator)
{
	ini_item_t *ini_item =
	    hash_table_get_inst(iterator->cur_item, ini_item_t, ht_link);
	return ini_item->lineno;
}
