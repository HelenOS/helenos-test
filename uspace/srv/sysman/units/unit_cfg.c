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

#include <adt/list.h>
#include <assert.h>
#include <conf/configuration.h>
#include <conf/ini.h>
#include <conf/text_parse.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <str.h>
#include <sysman/unit.h>

#include "repo.h"
#include "log.h"
#include "unit.h"
#include "util.h"

static const char *section_name = "Configuration";

static config_item_t unit_configuration[] = {
	{ "Path", &config_parse_string, offsetof(unit_cfg_t, path), NULL },
	CONFIGURATION_ITEM_SENTINEL
};

/**
 * TODO refactor path handling and rename to 'load from file'
 *
 * @param[out]  unit_ptr   Unit loaded from the file. Undefined when function fails.
 */
static errno_t cfg_parse_file(const char *dirname, const char *filename,
    unit_t **unit_ptr)
{
	errno_t rc = EOK;
	unit_t *new_unit = NULL;
	char *fn = NULL;
	ini_configuration_t ini_conf;
	text_parse_t text_parse;

	ini_configuration_init(&ini_conf);
	text_parse_init(&text_parse);

	const char *last_sep = str_rchr(filename, UNIT_NAME_SEPARATOR);
	if (last_sep == NULL) {
		rc = EINVAL;
		goto finish;
	}

	const char *unit_name = filename;
	const char *unit_type_name = last_sep + 1;

	unit_type_t unit_type = unit_type_name_to_type(unit_type_name);
	if (unit_type == UNIT_TYPE_INVALID) {
		rc = EINVAL;
		goto finish;
	}

	/* We parse files as part of ongoing repo transaction (locked). */
	unit_t *u = repo_find_unit_by_name_unsafe(unit_name);
	if (u != NULL) {
		// TODO allow updating configuration of existing unit
		rc = EEXIST;
		goto finish;
	} else {
		new_unit = u = unit_create(unit_type);
		new_unit->name = str_dup(unit_name);
		if (new_unit->name == NULL) {
			rc = ENOMEM;
			goto finish;
		}
	}
	if (u == NULL) {
		rc = ENOMEM;
		goto finish;
	}
	assert(u->type == unit_type);

	fn = util_compose_path(dirname, filename);
	if (fn == NULL) {
		rc = ENOMEM;
		goto finish;
	}

	/* Parse INI file to ini_conf structure */
	rc = ini_parse_file(fn, &ini_conf, &text_parse);
	switch (rc) {
	case EOK:
		/* This is fine */
		break;
	case EINVAL:
		goto dump_parse;
		break;
	default:
		sysman_log(LVL_WARN,
		    "Cannot parse '%s' (%i).", fn, rc);
		goto finish;
	}

	/* Parse ini structure */
	rc = unit_load(u, &ini_conf, &text_parse);
	*unit_ptr = u;

	/*
	 * Here we just continue undisturbed by errors, they'll be returned in
	 * 'finish' block and potential parse errors (or none) will be logged
	 * in 'dump_parse' block.
	 */

dump_parse:
	list_foreach(text_parse.errors, link, text_parse_error_t, err) {
		sysman_log(LVL_WARN,
		    "Error (%i) when parsing '%s' on line %zu.",
		    err->parse_errno, fn, err->lineno);
	}

finish:
	free(fn);
	ini_configuration_deinit(&ini_conf);
	text_parse_deinit(&text_parse);
	if (rc != EOK) {
		unit_destroy(&new_unit);
	}

	return rc;
}

static errno_t cfg_load_configuration(const char *path)
{
	DIR *dir;
	struct dirent *de;
	errno_t rc;

	dir = opendir(path);
	if (dir == NULL) {
		sysman_log(LVL_ERROR,
		    "Cannot open configuration directory '%s'", path);
		return EIO;
	}

	repo_begin_update();

	while ((de = readdir(dir))) {
		unit_t *unit = NULL;
		rc = cfg_parse_file(path, de->d_name, &unit);
		if (rc != EOK) {
			sysman_log(LVL_WARN, "Cannot load unit from file %s/%s",
			    path, de->d_name);
			/*
			 * Ignore error for now, we'll fail only when we're
			 * unable to resolve dependency names.
			 */
			continue;
		}

		assert(unit->repo_state == REPO_EMBRYO);
		rc = repo_add_unit(unit);
		if (rc != EOK)
			goto error;

	}
	closedir(dir);

	rc = repo_resolve_references();
	if (rc != EOK)
		goto error;

	repo_commit();
	return EOK;

error:
	repo_rollback();
	return rc;
}

static void unit_cfg_init(unit_t *unit)
{
	unit_cfg_t *u_cfg = CAST_CFG(unit);
	assert(u_cfg);
}

static void unit_cfg_destroy(unit_t *unit)
{
	unit_cfg_t *u_cfg = CAST_CFG(unit);
	assert(u_cfg);

	free(u_cfg->path);
}

static errno_t unit_cfg_load(unit_t *unit, ini_configuration_t *ini_conf,
    text_parse_t *text_parse)
{
	unit_cfg_t *u_cfg = CAST_CFG(unit);
	assert(u_cfg);

	ini_section_t *section = ini_get_section(ini_conf, section_name);
	if (section == NULL) {
		sysman_log(LVL_ERROR,
		    "Expected section '%s' in configuration of unit '%s'",
		    section_name, unit_name(unit));
		return ENOENT;
	}

	return config_load_ini_section(unit_configuration, section, u_cfg,
	    text_parse);
}

static errno_t unit_cfg_start(unit_t *unit)
{
	unit_cfg_t *u_cfg = CAST_CFG(unit);
	assert(u_cfg);

	errno_t rc = cfg_load_configuration(u_cfg->path);

	if (rc == EOK) {
		unit->state = STATE_STARTED;
	} else {
		unit->state = STATE_FAILED;
	}

	return rc;
}

static errno_t unit_cfg_stop(unit_t *unit)
{
	unit_cfg_t *u_cfg = CAST_CFG(unit);
	assert(u_cfg);

	/*
	 * It makes no sense to stop configuration (i.e. unload it), however,
	 * lets virtually stop it not to make obstructions for potential
	 * restart = reload of configuration.
	 */
	unit->state = STATE_STOPPED;
	return EOK;
}

static void unit_cfg_exposee_created(unit_t *unit)
{
	/* Configuration has no exposees. */
	assert(false);
}

static void unit_cfg_fail(unit_t *unit)
{
	/* Configuration cannot async fail. */
	assert(false);
}

DEFINE_UNIT_VMT(unit_cfg);