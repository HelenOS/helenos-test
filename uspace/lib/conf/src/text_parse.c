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

#include "conf/text_parse.h"

#include <assert.h>
#include <stdlib.h>

/** Constructor of parse structure */
void text_parse_init(text_parse_t *parse)
{
	assert(parse);
	list_initialize(&parse->errors);
	parse->has_error = false;
}

/** Destructor of parse structure
 *
 * It must be called before parse structure goes out of scope in order to avoid
 * memory leaks.
 */
void text_parse_deinit(text_parse_t *parse)
{
	assert(parse);
	list_foreach_safe(parse->errors, cur_link, next_link) {
		list_remove(cur_link);
		text_parse_error_t *error =
		    list_get_instance(cur_link, text_parse_error_t, link);
		free(error);
	}
}

/** Add an error to parse structure
 *
 * @param[in]  parse        the parse structure
 * @param[in]  lineno       line number where error originated (or zero)
 * @param[in]  parse_errno  user error code
 */
void text_parse_raise_error(text_parse_t *parse, size_t lineno,
    int parse_errno)
{
	assert(parse);

	parse->has_error = true;

	text_parse_error_t *error = malloc(sizeof(text_parse_error_t));
	/* Silently ignore failed malloc, has_error flag is set anyway */
	if (!error) {
		return;
	}
	error->lineno = lineno;
	error->parse_errno = parse_errno;
	list_append(&error->link, &parse->errors);
}
