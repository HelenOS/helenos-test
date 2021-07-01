/*
 * Copyright (c) 2019 Matthieu Riolo
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

/** @addtogroup layout
 * @{
 */
/**
 * @file
 */

#include <stdio.h>
#include <stdlib.h>
#include <str.h>
#include <str_error.h>
#include <ipc/services.h>
#include <ipc/input.h>
#include <io/input.h>
#include <abi/ipc/interfaces.h>
#include <loc.h>

static const char *cmdname = "layout";

/* Dispays help for layout in various levels */
static void print_help(void)
{
	printf("Changes, lists or displays the current keyboard layout.\n");

	printf(
	    "Usage: %s\n"
	    "\t%s list             lists all layouts\n"
	    "\t%s get              displays currently set layout\n"
	    "\t%s set <layout>     changes to the new layout\n",
	    cmdname, cmdname, cmdname, cmdname);
}

static async_sess_t *hid_exchange_start()
{
	service_id_t svcid;
	errno_t rc = loc_service_get_id(SERVICE_NAME_HID_INPUT, &svcid, 0);
	if (rc != EOK) {
		printf("%s: Failing to find service `%s` (%s)\n",
		    cmdname,
		    SERVICE_NAME_HID_INPUT,
		    str_error(rc));
		return NULL;
	}

	async_sess_t *sess = loc_service_connect(svcid, INTERFACE_ANY, 0);
	if (sess == NULL) {
		printf("%s: Failing to connect to service `%s`\n",
		    cmdname,
		    SERVICE_NAME_HID_INPUT);
	}

	return sess;
}

/* lists all available kb layouts */
static errno_t list_layout(void)
{
	/* TODO: replace this with the content of the folder containing the keymaps */

	printf("ar\n");
	printf("cz\n");
	printf("fr_azerty\n");
	printf("us_dvorak\n");
	printf("us_qwerty\n");

	return EOK;
}

/* displays active keyboard layout */
static errno_t get_layout(void)
{
	async_sess_t *sess = hid_exchange_start();
	if (sess == NULL)
		return EREFUSED;

	char *layout_name;
	errno_t rc = input_layout_get(sess, &layout_name);

	if (rc == EOK) {
		printf("%s\n", layout_name);
		free(layout_name);
	} else {
		printf("%s: Failing to retrive keyboard layout (%s)\n",
		    cmdname,
		    str_error(rc));
	}

	async_hangup(sess);
	return rc;
}

/* changes the keyboard layout */
static errno_t set_layout(char *layout_name)
{
	async_sess_t *sess = hid_exchange_start();
	if (sess == NULL)
		return EREFUSED;

	errno_t rc = input_layout_set(sess, layout_name);

	if (rc != EOK)
		printf("%s: Cannot activate keyboard layout `%s`\n (%s)\n",
		    cmdname,
		    layout_name,
		    str_error(rc));

	async_hangup(sess);
	return rc;
}

int main(int argc, char *argv[])
{
	errno_t rc = EINVAL;
	if (argc == 1) {
		rc = EOK;
	} else if (argc == 2) {
		if (str_cmp(argv[1], "list") == 0) {
			return list_layout();
		} else if (str_cmp(argv[1], "get") == 0) {
			return get_layout();
		}
	} else if (argc == 3) {
		if (str_cmp(argv[1], "set") == 0 && argc == 3) {
			return set_layout(argv[2]);
		}
	}

	print_help();
	return rc;
}
