/*
 * Copyright (c) 2015 Michal Koutny
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

/**
 *
 * @addtogroup taskman
 * @{
 */

#include <adt/list.h>
#include <assert.h>
#include <errno.h>
#include <ipc/taskman.h>
#include <stdlib.h>
#include <task.h>

#include "event.h"
#include "task.h"
#include "taskman.h"

/** Pending task wait structure. */
typedef struct {
	link_t link;
	task_id_t id;         /**< Task ID who we wait for. */
	task_id_t waiter_id;  /**< Task ID who waits. */
	ipc_call_t *icall;  /**< Call ID waiting for the event. */
	task_wait_flag_t flags; /**< Wait flags. */
} pending_wait_t;

static list_t pending_waits;
static FIBRIL_RWLOCK_INITIALIZE(pending_wait_lock);

static list_t listeners;
static FIBRIL_RWLOCK_INITIALIZE(listeners_lock);

errno_t event_init(void)
{
	list_initialize(&pending_waits);
	list_initialize(&listeners);

	return EOK;
}

static task_wait_flag_t event_flags(task_t *task)
{
	task_wait_flag_t flags = TASK_WAIT_NONE;
	if (task->retval_type == RVAL_SET) {
		flags |= TASK_WAIT_RETVAL;
	}

	if (task->exit != TASK_EXIT_RUNNING) {
		flags |= TASK_WAIT_EXIT;
		if (task->retval_type == RVAL_SET_EXIT) {
			flags |= TASK_WAIT_RETVAL;
		} else {
			/* Don't notify retval of exited task */
			flags &= ~TASK_WAIT_RETVAL;
		}
	}
	return flags;
}

static void event_notify(task_t *sender, async_sess_t *sess)
{
	task_wait_flag_t flags = event_flags(sender);
	if (flags == TASK_WAIT_NONE) {
		return;
	}

	async_exch_t *exch = async_exchange_begin(sess);
	aid_t req = async_send_5(exch, TASKMAN_EV_TASK,
	    LOWER32(sender->id),
	    UPPER32(sender->id),
	    flags,
	    sender->exit,
	    sender->retval,
	    NULL);

	async_exchange_end(exch);

	/* Just send a notification and don't wait for anything */
	async_forget(req);
}

/** Notify all registered listeners about sender's event
 *
 * @note Assumes share lock of task_hash_table is held.
 */
static void event_notify_all(task_t *sender)
{
	task_wait_flag_t flags = event_flags(sender);
	if (flags == TASK_WAIT_NONE) {
		return;
	}

	fibril_rwlock_read_lock(&listeners_lock);
	list_foreach(listeners, listeners, task_t, t) {
		assert(t->sess);
		event_notify(sender, t->sess);
	}
	fibril_rwlock_read_unlock(&listeners_lock);
}

/** Process pending wait requests
 *
 * Assumes task_hash_table_lock is hold (at least read)
 */
static void process_pending_wait(void)
{
	fibril_rwlock_write_lock(&pending_wait_lock);
loop:
	list_foreach(pending_waits, link, pending_wait_t, pr) {
		task_t *t = task_get_by_id(pr->id);
		if (t == NULL) {
			continue; // TODO really when does this happen?
		}
		task_wait_flag_t notify_flags = event_flags(t);

		/*
		 * In current implementation you can wait for single retval,
		 * thus it can be never present in rest flags.
		 */
		task_wait_flag_t rest = (~notify_flags & pr->flags) & ~TASK_WAIT_RETVAL;
		rest &= ~TASK_WAIT_BOTH;
		bool match = notify_flags & pr->flags;
		// TODO why do I even accept such calls?
		bool answer = !(pr->icall->flags & IPC_CALL_NOTIF);

		if (match == 0) {
			if (notify_flags & TASK_WAIT_EXIT) {
				/* Nothing to wait for anymore */
				if (answer) {
					async_answer_0(pr->icall, EINTR);
				}
			} else {
				/* Maybe later */
				continue;
			}
		} else if (answer) {
			if ((pr->flags & TASK_WAIT_BOTH) && match == TASK_WAIT_EXIT) {
				/* No sense to wait for both anymore */
				async_answer_1(pr->icall, EINTR, t->exit);
			} else {
				/*
				 * Send both exit status and retval, caller
				 * should know what is valid
				 */
				async_answer_3(pr->icall, EOK, t->exit,
				    t->retval, rest);
			}

			/* Pending wait has one more chance  */
			if (rest && (pr->flags & TASK_WAIT_BOTH)) {
				pr->flags = rest | TASK_WAIT_BOTH;
				continue;
			}
		}

		list_remove(&pr->link);
		free(pr);
		goto loop;
	}
	fibril_rwlock_write_unlock(&pending_wait_lock);
}

static bool dump_walker(task_t *t, void *arg)
{
	event_notify(t, arg);
	return true;
}

void event_register_listener(task_id_t id, bool past_events, async_sess_t *sess,
    ipc_call_t *icall)
{
	errno_t rc = EOK;
	/*
	 * We have lock of tasks structures so that we can guarantee
	 * that dump receiver will receive tasks correctly ordered (retval,
	 * exit updates are serialized via exclusive lock).
	 */
	fibril_rwlock_write_lock(&task_hash_table_lock);
	fibril_rwlock_write_lock(&listeners_lock);

	task_t *t = task_get_by_id(id);
	if (t == NULL) {
		rc = ENOENT;
		goto finish;
	}
	assert(t->sess == NULL);
	list_append(&t->listeners, &listeners);
	t->sess = sess;

	/*
	 * Answer caller first, so that they are not unnecessarily waiting
	 * while we dump events.
	 */
	async_answer_0(icall, rc);
	if (past_events) {
		task_foreach(&dump_walker, t->sess);
	}

finish:
	fibril_rwlock_write_unlock(&listeners_lock);
	fibril_rwlock_write_unlock(&task_hash_table_lock);
	if (rc != EOK) {
		async_answer_0(icall, rc);
	}
}

void wait_for_task(task_id_t id, task_wait_flag_t flags, ipc_call_t *icall,
    task_id_t waiter_id)
{
	assert(!(flags & TASK_WAIT_BOTH) ||
	    ((flags & TASK_WAIT_RETVAL) && (flags & TASK_WAIT_EXIT)));

	fibril_rwlock_read_lock(&task_hash_table_lock);
	task_t *t = task_get_by_id(id);
	fibril_rwlock_read_unlock(&task_hash_table_lock);

	if (t == NULL) {
		/* No such task exists. */
		async_answer_0(icall, ENOENT);
		return;
	}

	if (t->exit != TASK_EXIT_RUNNING) {
		//TODO are flags BOTH processed correctly here?
		async_answer_3(icall, EOK, t->exit, t->retval, 0);
		return;
	}

	/*
	 * Add request to pending list or reuse existing item for a second
	 * wait.
	 */
	fibril_rwlock_write_lock(&pending_wait_lock);
	pending_wait_t *pr = NULL;
	list_foreach(pending_waits, link, pending_wait_t, it) {
		if (it->id == id && it->waiter_id == waiter_id) {
			pr = it;
			break;
		}
	}

	errno_t rc = EOK;
	if (pr == NULL) {
		pr = malloc(sizeof(pending_wait_t));
		if (!pr) {
			rc = ENOMEM;
			goto finish;
		}

		link_initialize(&pr->link);
		pr->id = id;
		pr->waiter_id = waiter_id;
		pr->flags = flags;
		pr->icall = icall;

		list_append(&pr->link, &pending_waits);
		rc = EOK;
	} else if (!(pr->flags & TASK_WAIT_BOTH)) {
		/*
		 * One task can wait for another task only once (per task, not
		 * fibril).
		 */
		rc = EEXIST;
	} else {
		/*
		 * Reuse pending wait for the second time.
		 */
		pr->flags &= ~TASK_WAIT_BOTH; // TODO maybe new flags should be set?
		pr->icall = icall;
	}

finish:
	fibril_rwlock_write_unlock(&pending_wait_lock);
	// TODO why IPC_CALLID_NOTIFICATION? explain!
	if (rc != EOK && !(icall->flags & IPC_CALL_NOTIF)) {
		async_answer_0(icall, rc);
	}
}

errno_t task_set_retval(task_id_t sender, int retval, bool wait_for_exit)
{
	errno_t rc = EOK;

	fibril_rwlock_write_lock(&task_hash_table_lock);
	task_t *t = task_get_by_id(sender);

	if ((t == NULL) || (t->exit != TASK_EXIT_RUNNING)) {
		rc = EINVAL;
		goto finish;
	}

	t->retval = retval;
	t->retval_type = wait_for_exit ? RVAL_SET_EXIT : RVAL_SET;

	event_notify_all(t);
	process_pending_wait();

finish:
	fibril_rwlock_write_unlock(&task_hash_table_lock);
	return rc;
}

void task_terminated(task_id_t id, exit_reason_t exit_reason)
{
	/* Mark task as finished. */
	fibril_rwlock_write_lock(&task_hash_table_lock);
	task_t *t = task_get_by_id(id);
	if (t == NULL) {
		goto finish;
	}

	/*
	 * If daemon returns a value and then fails/is killed, it's an
	 * unexpected termination.
	 */
	if (t->retval_type == RVAL_UNSET || exit_reason == EXIT_REASON_KILLED) {
		t->exit = TASK_EXIT_UNEXPECTED;
	} else if (t->failed) {
		t->exit = TASK_EXIT_UNEXPECTED;
	} else  {
		t->exit = TASK_EXIT_NORMAL;
	}

	/*
	 * First remove terminated task from listeners and only after that
	 * notify all others.
	 */
	fibril_rwlock_write_lock(&listeners_lock);
	list_remove(&t->listeners);
	fibril_rwlock_write_unlock(&listeners_lock);

	event_notify_all(t);
	process_pending_wait();

	/* Eventually, get rid of task_t. */
	task_remove(&t);

finish:
	fibril_rwlock_write_unlock(&task_hash_table_lock);
}

void task_failed(task_id_t id)
{
	/* Mark task as failed. */
	fibril_rwlock_write_lock(&task_hash_table_lock);
	task_t *t = task_get_by_id(id);
	if (t == NULL) {
		goto finish;
	}

	t->failed = true;
	// TODO design substitution for taskmon (monitoring) = invoke dump
	// utility or pass event to registered tasks

finish:
	fibril_rwlock_write_unlock(&task_hash_table_lock);
}

/**
 * @}
 */