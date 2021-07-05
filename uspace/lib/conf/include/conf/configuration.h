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

#ifndef _CONF_CONFIGURATION_H
#define _CONF_CONFIGURATION_H

#include <conf/ini.h>
#include <conf/text_parse.h>
#include <stdbool.h>

/** Structure represents a declaration of a configuration value */
typedef struct {
	/** Value name */
	const char *name;

	/** Parse and store given string
	 *
	 * @param[in]  string  string to parse (it's whitespace-trimmed)
	 * @param[out] dst     pointer where to store parsed result (must be
	 *                     preallocated)
	 * @param[out] parse   parse struct to store parsing errors
	 * @param[in]  lineno  line number of the original string
	 *
	 * @return  true on success
	 * @return  false on error (e.g. format error, low memory)
	 */
	bool (*parse)(const char *, void *, text_parse_t *, size_t);

	/** Offset in structure where parsed value ought be stored */
	size_t offset;

	/** String representation of default value
	 *
	 * Format is same as in input. NULL value denotes required
	 * configuration value.
	 */
	const char *default_value;
} config_item_t;

/** Code of configuration processing error */
typedef enum {
	CONFIGURATION_EMISSING_ITEM = -1,
	CONFIGURATION_EINVAL_BOOL = -2,
	CONFIGURATION_ELIMIT = -3
} config_error_t;

#define CONFIGURATION_ITEM_SENTINEL {NULL}

extern int config_load_ini_section(config_item_t *, ini_section_t *, void *,
    text_parse_t *);

extern bool config_parse_string(const char *, void *, text_parse_t *, size_t);
extern bool config_parse_bool(const char *, void *, text_parse_t *, size_t);

#endif
