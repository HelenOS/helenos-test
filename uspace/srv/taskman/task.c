/*
 * Copyright (c) 2009 Martin Decky
 * Copyright (c) 2009 Jiri Svoboda
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

/** @addtogroup taskman
 * @{
 */

#include <adt/hash_table.h>
#include <assert.h>
#include <async.h>
#include <errno.h>
#include <macros.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>
#include <types/task.h>

#include "task.h"
#include "taskman.h"


/** Task hash table item. */
typedef struct {
	ht_link_t link;
	
	task_id_t id;    /**< Task ID. */
	task_exit_t exit;/**< Task is done. */
	bool have_rval;  /**< Task returned a value. */
	int retval;      /**< The return value. */
} hashed_task_t;


static size_t task_key_hash(void *key)
{
	return *(task_id_t*)key;
}

static size_t task_hash(const ht_link_t  *item)
{
	hashed_task_t *ht = hash_table_get_inst(item, hashed_task_t, link);
	return ht->id;
}

static bool task_key_equal(void *key, const ht_link_t *item)
{
	hashed_task_t *ht = hash_table_get_inst(item, hashed_task_t, link);
	return ht->id == *(task_id_t*)key;
}

/** Perform actions after removal of item from the hash table. */
static void task_remove(ht_link_t *item)
{
	free(hash_table_get_inst(item, hashed_task_t, link));
}

/** Operations for task hash table. */
static hash_table_ops_t task_hash_table_ops = {
	.hash = task_hash,
	.key_hash = task_key_hash,
	.key_equal = task_key_equal,
	.equal = NULL,
	.remove_callback = task_remove
};

/** Task hash table structure. */
static hash_table_t task_hash_table;

typedef struct {
	ht_link_t link;
	sysarg_t in_phone_hash;  /**< Incoming phone hash. */
	task_id_t id;            /**< Task ID. */
} p2i_entry_t;

/* phone-to-id hash table operations */

static size_t p2i_key_hash(void *key)
{
	sysarg_t in_phone_hash = *(sysarg_t*)key;
	return in_phone_hash;
}

static size_t p2i_hash(const ht_link_t *item)
{
	p2i_entry_t *entry = hash_table_get_inst(item, p2i_entry_t, link);
	return entry->in_phone_hash;
}

static bool p2i_key_equal(void *key, const ht_link_t *item)
{
	sysarg_t in_phone_hash = *(sysarg_t*)key;
	p2i_entry_t *entry = hash_table_get_inst(item, p2i_entry_t, link);
	
	return (in_phone_hash == entry->in_phone_hash);
}

/** Perform actions after removal of item from the hash table.
 *
 * @param item Item that was removed from the hash table.
 *
 */
static void p2i_remove(ht_link_t *item)
{
	assert(item);
	free(hash_table_get_inst(item, p2i_entry_t, link));
}

/** Operations for task hash table. */
static hash_table_ops_t p2i_ops = {
	.hash = p2i_hash,
	.key_hash = p2i_key_hash,
	.key_equal = p2i_key_equal,
	.equal = NULL,
	.remove_callback = p2i_remove
};

/** Map phone hash to task ID */
static hash_table_t phone_to_id;

/** Pending task wait structure. */
typedef struct {
	link_t link;
	task_id_t id;         /**< Task ID. */
	ipc_callid_t callid;  /**< Call ID waiting for the connection */
	int flags;            /**< Wait flags TODO */
} pending_wait_t;

static list_t pending_wait;

int task_init(void)
{
	if (!hash_table_create(&task_hash_table, 0, 0, &task_hash_table_ops)) {
		printf(NAME ": No memory available for tasks\n");
		return ENOMEM;
	}
	
	if (!hash_table_create(&phone_to_id, 0, 0, &p2i_ops)) {
		printf(NAME ": No memory available for tasks\n");
		return ENOMEM;
	}
	
	list_initialize(&pending_wait);
	return EOK;
}

/** Process pending wait requests */
void process_pending_wait(void)
{
	task_exit_t texit;
	
loop:
	// W lock
	list_foreach(pending_wait, link, pending_wait_t, pr) {
		// R lock task_hash_table
		ht_link_t *link = hash_table_find(&task_hash_table, &pr->id);
		// R unlock task_hash_table
		if (!link)
			continue;
		
		hashed_task_t *ht = hash_table_get_inst(link, hashed_task_t, link);
		if (ht->exit == TASK_EXIT_RUNNING)
			continue;
		
		if (!(pr->callid & IPC_CALLID_NOTIFICATION)) {
			texit = ht->exit;
			async_answer_2(pr->callid, EOK, texit,
			    ht->retval);
		}
		
		list_remove(&pr->link);
		free(pr);
		goto loop;
	}
	// W unlock
}

void wait_for_task(task_id_t id, int flags, ipc_callid_t callid, ipc_call_t *call)
{
	// R lock
	ht_link_t *link = hash_table_find(&task_hash_table, &id);
	// R unlock
	hashed_task_t *ht = (link != NULL) ?
	    hash_table_get_inst(link, hashed_task_t, link) : NULL;
	
	if (ht == NULL) {
		/* No such task exists. */
		async_answer_0(callid, ENOENT);
		return;
	}
	
	if (ht->exit != TASK_EXIT_RUNNING) {
		task_exit_t texit = ht->exit;
		async_answer_2(callid, EOK, texit, ht->retval);
		return;
	}
	
	/* Add to pending list */
	pending_wait_t *pr =
	    (pending_wait_t *) malloc(sizeof(pending_wait_t));
	if (!pr) {
		// TODO why IPC_CALLID_NOTIFICATION? explain!
		if (!(callid & IPC_CALLID_NOTIFICATION))
			async_answer_0(callid, ENOMEM);
		return;
	}
	
	link_initialize(&pr->link);
	pr->id = id;
	pr->flags = flags;
	pr->callid = callid;
	// W lock
	list_append(&pr->link, &pending_wait);
	// W unlock
}

int task_id_intro(ipc_call_t *call)
{
	// TODO think about task_id reuse and this
	// R lock
	ht_link_t *link = hash_table_find(&task_hash_table, &call->in_task_id);
	// R unlock
	if (link != NULL)
		return EEXISTS;
	
	hashed_task_t *ht = (hashed_task_t *) malloc(sizeof(hashed_task_t));
	if (ht == NULL)
		return ENOMEM;

	/*
	 * Insert into the main table.
	 */
	ht->id = call->in_task_id;
	ht->exit = TASK_EXIT_RUNNING;
	ht->have_rval = false;
	ht->retval = -1;
	// W lock
	hash_table_insert(&task_hash_table, &ht->link);
	// W unlock
	
	return EOK;
}

int task_set_retval(ipc_call_t *call)
{
	task_id_t id = call->in_task_id;
	
	// R lock
	ht_link_t *link = hash_table_find(&task_hash_table, &id);
	// R unlock
	hashed_task_t *ht = (link != NULL) ?
	    hash_table_get_inst(link, hashed_task_t, link) : NULL;
	
	if ((ht == NULL) || (ht->exit != TASK_EXIT_RUNNING))
		return EINVAL;
	
	// TODO process additional flag to retval
	ht->have_rval = true;
	ht->retval = IPC_GET_ARG1(*call);
	
	process_pending_wait();
	
	return EOK;
}

void task_terminated(task_id_t id, task_exit_t texit)
{
	/* Mark task as finished. */
	// R lock
	ht_link_t *link = hash_table_find(&task_hash_table, &id);
	// R unlock
	if (link == NULL)
		return;

	hashed_task_t *ht = hash_table_get_inst(link, hashed_task_t, link);
	
	ht->exit = texit;
	process_pending_wait();

	// W lock
	hash_table_remove_item(&task_hash_table, &ht->link);
	// W unlock
}

/**
 * @}
 */
