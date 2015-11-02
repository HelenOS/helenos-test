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

#include <async.h>
#include <errno.h>
#include <stdlib.h>
#include <str.h>
#include <sysman/ctl.h>
#include <sysman/sysman.h>

/*
 * TODO
 * Non-blocking favor of this API is effectively incomplete as it doesn't
 * provide means how to obtain result of the start operation.
 * Probably devise individual API for brokers that could exploit the fact that
 * broker knows when appropriate exposee is created and the request succeeded.
 * Still though, it's necessary to centralize timeout into sysman.
 */
int sysman_unit_start(const char *unit_name, int flags)
{
	async_exch_t *exch = sysman_exchange_begin(SYSMAN_PORT_CTL);

	aid_t req = async_send_1(exch, SYSMAN_CTL_UNIT_START, flags, NULL);
	sysarg_t rc = async_data_write_start(exch, unit_name, str_size(unit_name));
	sysman_exchange_end(exch);

	if (rc != EOK) {
		async_forget(req);
		return rc;
	}

	async_wait_for(req, &rc);
	return rc;
}

static int sysman_get_units_once(sysarg_t *buf, size_t buf_size,
    size_t *act_size)
{
	async_exch_t *exch = sysman_exchange_begin(SYSMAN_PORT_CTL);

	ipc_call_t answer;
	aid_t req = async_send_0(exch, SYSMAN_CTL_GET_UNITS, &answer);
	int rc = async_data_read_start(exch, buf, buf_size);

	sysman_exchange_end(exch);

	if (rc != EOK) {
		async_forget(req);
		return rc;
	}

	sysarg_t retval;
	async_wait_for(req, &retval);

	if (retval != EOK) {
		return retval;
	}

	*act_size = IPC_GET_ARG1(answer);
	return EOK;
}

int sysman_get_units(unit_handle_t **units_ptr, size_t *cnt_ptr)
{
	*units_ptr = NULL;
	*cnt_ptr = 0;

	unit_handle_t *units = NULL;
	size_t alloc_size = 0;
	size_t act_size = 0;

	while (true) {
		int rc = sysman_get_units_once(units, alloc_size, &act_size);
		if (rc != EOK) {
			return rc;
		}

		if (act_size <= alloc_size) {
			break;
		}

		alloc_size = act_size;
		units = realloc(units, alloc_size);
		if (units == NULL) {
			return ENOMEM;
		}
	}

	*units_ptr = units;
	*cnt_ptr = act_size / sizeof(unit_handle_t);
	return EOK;
}

int sysman_unit_get_name(unit_handle_t handle, char *buf, size_t buf_size)
{
	async_exch_t *exch = sysman_exchange_begin(SYSMAN_PORT_CTL);

	ipc_call_t answer;
	aid_t req = async_send_1(exch, SYSMAN_CTL_UNIT_GET_NAME, handle, &answer);
	int rc = async_data_read_start(exch, buf, buf_size);

	sysman_exchange_end(exch);

	if (rc != EOK) {
		async_forget(req);
		return rc;
	}

	sysarg_t retval;
	async_wait_for(req, &retval);

	if (retval != EOK) {
		return retval;
	}

	return EOK;
}

int sysman_unit_get_state(unit_handle_t handle, unit_state_t *state)
{
	async_exch_t *exch = sysman_exchange_begin(SYSMAN_PORT_CTL);
	int rc = async_req_1_1(exch, SYSMAN_CTL_UNIT_GET_STATE, handle, state);
	sysman_exchange_end(exch);

	return rc;
}