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

#ifndef _CONF_INI_H
#define _CONF_INI_H

#include <adt/hash_table.h>
#include <conf/text_parse.h>
#include <stdbool.h>

/** INI file parsing error code */
typedef enum {
	INI_EOK = 0,
	INI_ETOO_LONG = -101,
	INI_EDUP_SECTION = -102,
	INI_EASSIGN_EXPECTED = -103,
	INI_EBRACKET_EXPECTED = -104
} ini_error_t;

/** Configuration from INI file
 *
 * Configuration consists of (named) sections and each section contains
 * key-value pairs (both strings). Current implementation doesn't specify
 * ordering of pairs and there can be multiple pairs with the same key (i.e.
 * usable for set unions, not overwriting values).
 *
 * Sections are uniquely named and there can be at most one unnamed section,
 * referred to as the default section.
 *
 * Configuration structure is owner of all key/value strings. If you need any
 * of these strings out of the structure's lifespan, you have to make your own
 * copy.
 */
typedef struct {
	hash_table_t sections;
} ini_configuration_t;

/** INI configuration section */
typedef struct {
	ht_link_t ht_link;

	/** Line number where section header is */
	size_t lineno;

	/** Name of the section (or NULL for default section) */
	char *name;
	hash_table_t items;
} ini_section_t;

/** INI configuration item iterator
 *
 * Use ini_item_iterator_* functions to manipulate the iterator.
 *
 * Example:
 * @code
 * ini_item_iterator_t it = ini_section_get_iterator(section, key);
 * for (; ini_item_iterator_valid(&it); ini_item_iterator_inc(&it) {
 * 	const char *value = ini_item_iterator_value(&it);
 * 	...
 * }
 * @endcode
 *
 */
typedef struct {
	/** Section's hash table */
	hash_table_t *table;

	ht_link_t *cur_item;
	ht_link_t *first_item;

	/** Incremented flag
	 *
	 * Iterator implementation wraps hash table's cyclic lists. The flag
	 * and initial hash table item allows linearization for iterator
	 * interface.
	 */
	bool incremented;
} ini_item_iterator_t;

extern void ini_configuration_init(ini_configuration_t *);
extern void ini_configuration_deinit(ini_configuration_t *);

extern int ini_parse_file(const char *, ini_configuration_t *, text_parse_t *);

extern int ini_parse_string(const char *, ini_configuration_t *, text_parse_t *);

extern ini_section_t *ini_get_section(ini_configuration_t *, const char *);

extern ini_item_iterator_t ini_section_get_iterator(ini_section_t *, const char *);

extern bool ini_item_iterator_valid(ini_item_iterator_t *);

extern void ini_item_iterator_inc(ini_item_iterator_t *);

extern const char *ini_item_iterator_value(ini_item_iterator_t *);

extern size_t ini_item_iterator_lineno(ini_item_iterator_t *);

#endif
