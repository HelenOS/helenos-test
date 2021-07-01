/*
 * Copyright (c) 2012 Jiri Svoboda
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

/** @addtogroup libc
 * @{
 */
/**
 * @file
 * @brief Input protocol client stub
 */

#include <async.h>
#include <assert.h>
#include <errno.h>
#include <io/kbd_event.h>
#include <io/input.h>
#include <ipc/input.h>
#include <stdlib.h>
#include <str.h>

static void input_cb_conn(ipc_call_t *icall, void *arg);

errno_t input_open(async_sess_t *sess, input_ev_ops_t *ev_ops,
    void *arg, input_t **rinput)
{
	input_t *input = calloc(1, sizeof(input_t));
	if (input == NULL)
		return ENOMEM;

	input->sess = sess;
	input->ev_ops = ev_ops;
	input->user = arg;

	async_exch_t *exch = async_exchange_begin(sess);

	port_id_t port;
	errno_t rc = async_create_callback_port(exch, INTERFACE_INPUT_CB, 0, 0,
	    input_cb_conn, input, &port);

	async_exchange_end(exch);

	if (rc != EOK)
		goto error;

	*rinput = input;
	return EOK;

error:
	if (input != NULL)
		free(input);

	return rc;
}

void input_close(input_t *input)
{
	/* XXX Synchronize with input_cb_conn */
	free(input);
}

errno_t input_activate(input_t *input)
{
	async_exch_t *exch = async_exchange_begin(input->sess);
	errno_t rc = async_req_0_0(exch, INPUT_ACTIVATE);
	async_exchange_end(exch);

	return rc;
}

static void input_ev_active(input_t *input, ipc_call_t *call)
{
	errno_t rc = input->ev_ops->active(input);
	async_answer_0(call, rc);
}

static void input_ev_deactive(input_t *input, ipc_call_t *call)
{
	errno_t rc = input->ev_ops->deactive(input);
	async_answer_0(call, rc);
}

static void input_ev_key(input_t *input, ipc_call_t *call)
{
	kbd_event_type_t type;
	keycode_t key;
	keymod_t mods;
	char32_t c;
	errno_t rc;

	type = ipc_get_arg1(call);
	key = ipc_get_arg2(call);
	mods = ipc_get_arg3(call);
	c = ipc_get_arg4(call);

	rc = input->ev_ops->key(input, type, key, mods, c);
	async_answer_0(call, rc);
}

static void input_ev_move(input_t *input, ipc_call_t *call)
{
	int dx;
	int dy;
	errno_t rc;

	dx = ipc_get_arg1(call);
	dy = ipc_get_arg2(call);

	rc = input->ev_ops->move(input, dx, dy);
	async_answer_0(call, rc);
}

static void input_ev_abs_move(input_t *input, ipc_call_t *call)
{
	unsigned x;
	unsigned y;
	unsigned max_x;
	unsigned max_y;
	errno_t rc;

	x = ipc_get_arg1(call);
	y = ipc_get_arg2(call);
	max_x = ipc_get_arg3(call);
	max_y = ipc_get_arg4(call);

	rc = input->ev_ops->abs_move(input, x, y, max_x, max_y);
	async_answer_0(call, rc);
}

static void input_ev_button(input_t *input, ipc_call_t *call)
{
	int bnum;
	int press;
	errno_t rc;

	bnum = ipc_get_arg1(call);
	press = ipc_get_arg2(call);

	rc = input->ev_ops->button(input, bnum, press);
	async_answer_0(call, rc);
}

static void input_cb_conn(ipc_call_t *icall, void *arg)
{
	input_t *input = (input_t *) arg;

	while (true) {
		ipc_call_t call;
		async_get_call(&call);

		if (!ipc_get_imethod(&call)) {
			async_answer_0(&call, EOK);
			return;
		}

		switch (ipc_get_imethod(&call)) {
		case INPUT_EVENT_ACTIVE:
			input_ev_active(input, &call);
			break;
		case INPUT_EVENT_DEACTIVE:
			input_ev_deactive(input, &call);
			break;
		case INPUT_EVENT_KEY:
			input_ev_key(input, &call);
			break;
		case INPUT_EVENT_MOVE:
			input_ev_move(input, &call);
			break;
		case INPUT_EVENT_ABS_MOVE:
			input_ev_abs_move(input, &call);
			break;
		case INPUT_EVENT_BUTTON:
			input_ev_button(input, &call);
			break;
		default:
			async_answer_0(&call, ENOTSUP);
		}
	}
}

/**
 * Retrieves the active keyboard layout
 * @param sess Active session to the input server
 * @param layout The name of the currently active layout,
 *        needs to be freed by the caller
 * @return EOK if sucessful or the corresponding error code.
 *         If a failure occurs the param layout is already freed
 */
errno_t input_layout_get(async_sess_t *sess, char **layout)
{
	*layout = NULL;

	async_exch_t *exch = async_exchange_begin(sess);

	ipc_call_t answer;
	aid_t req = async_send_0(exch, INPUT_GET_LAYOUT, &answer);

	char layout_buf[INPUT_LAYOUT_NAME_MAXLEN + 1];
	ipc_call_t dreply;
	aid_t dreq = async_data_read(exch, layout_buf, INPUT_LAYOUT_NAME_MAXLEN,
	    &dreply);

	errno_t dretval;
	async_wait_for(dreq, &dretval);

	async_exchange_end(exch);

	if (dretval != EOK) {
		async_forget(req);
		return dretval;
	}

	errno_t retval;
	async_wait_for(req, &retval);

	if (retval != EOK)
		return retval;

	size_t length = ipc_get_arg2(&dreply);
	assert(length <= INPUT_LAYOUT_NAME_MAXLEN);
	layout_buf[length] = '\0';

	*layout = str_dup(layout_buf);
	if (*layout == NULL)
		return ENOMEM;

	return EOK;
}

/**
 * Changes the keyboard layout
 * @param sess Active session to the input server
 * @param layout The name of the layout which should be activated
 * @return EOK if sucessful or the corresponding error code.
 */
errno_t input_layout_set(async_sess_t *sess, const char *layout)
{
	errno_t rc;
	ipc_call_t call;
	async_exch_t *exch = async_exchange_begin(sess);

	aid_t mid = async_send_0(exch, INPUT_SET_LAYOUT, &call);
	rc = async_data_write_start(exch, layout, str_size(layout));

	if (rc == EOK)
		async_wait_for(mid, &rc);

	async_exchange_end(exch);
	return rc;
}

/** @}
 */
