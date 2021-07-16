/*
 * Copyright (c) 2006 Ondrej Palkovsky
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

/** @addtogroup kernel_generic_ipc
 * @{
 */
/** @file
 */

#ifndef KERN_IPC_H_
#define KERN_IPC_H_

#include <synch/spinlock.h>
#include <synch/mutex.h>
#include <synch/waitq.h>
#include <abi/ipc/ipc.h>
#include <abi/proc/task.h>
#include <typedefs.h>
#include <mm/slab.h>
#include <cap/cap.h>

struct answerbox;
struct task;
struct call;

typedef enum {
	/** Phone is free and can be allocated */
	IPC_PHONE_FREE = 0,
	/** Phone is connecting somewhere */
	IPC_PHONE_CONNECTING,
	/** Phone is connected */
	IPC_PHONE_CONNECTED,
	/** Phone is hung up, waiting for answers to come */
	IPC_PHONE_HUNGUP,
	/** Phone was hungup from server */
	IPC_PHONE_SLAMMED
} ipc_phone_state_t;

/** Structure identifying phone (in TASK structure) */
typedef struct phone {
	mutex_t lock;
	link_t link;
	struct task *caller;
	struct answerbox *callee;
	/* A call prepared for hangup ahead of time, so that it cannot fail. */
	struct call *hangup_call;
	ipc_phone_state_t state;
	atomic_size_t active_calls;
	/** User-defined label */
	sysarg_t label;
	kobject_t *kobject;
} phone_t;

typedef struct answerbox {
	IRQ_SPINLOCK_DECLARE(lock);

	/** Answerbox is active until it enters cleanup. */
	bool active;

	struct task *task;

	waitq_t wq;

	/**
	 * Number of answers the answerbox is expecting to eventually arrive.
	 */
	atomic_size_t active_calls;

	/** Phones connected to this answerbox. */
	list_t connected_phones;
	/** Received calls. */
	list_t calls;
	list_t dispatched_calls;  /* Should be hash table in the future */

	/** Answered calls. */
	list_t answers;

	IRQ_SPINLOCK_DECLARE(irq_lock);

	/** Notifications from IRQ handlers. */
	list_t irq_notifs;
} answerbox_t;

typedef struct call {
	kobject_t *kobject;

	/**
	 * Task link.
	 * Valid only when the call is not forgotten.
	 * Protected by the task's active_calls_lock.
	 */
	link_t ta_link;

	/** Answerbox link. */
	link_t ab_link;

	unsigned int flags;

	/** Protects the forget member. */
	SPINLOCK_DECLARE(forget_lock);

	/**
	 * True if the caller 'forgot' this call and donated it to the callee.
	 * Forgotten calls are discarded upon answering (the answer is not
	 * delivered) and answered calls cannot be forgotten. Forgotten calls
	 * also do not figure on the task's active call list.
	 *
	 * We keep this separate from the flags so that it is not necessary
	 * to take a lock when accessing them.
	 */
	bool forget;

	/** True if the call is in the active list. */
	bool active;

	/**
	 * Identification of the caller.
	 * Valid only when the call is not forgotten.
	 */
	struct task *sender;

	/*
	 * Answerbox that will receive the answer.
	 * This will most of the times be the sender's answerbox,
	 * but we allow for useful exceptions.
	 */
	answerbox_t *callerbox;

	/** Phone which was used to send the call. */
	phone_t *caller_phone;

	/** Private data to internal IPC. */
	sysarg_t priv;

	/** Data passed from/to userspace. */
	ipc_data_t data;

	/** Method as it was sent in the request. */
	sysarg_t request_method;

	/** Buffer for IPC_M_DATA_WRITE and IPC_M_DATA_READ. */
	uint8_t *buffer;
} call_t;

extern slab_cache_t *phone_cache;

extern answerbox_t *ipc_box_0;

extern kobject_ops_t call_kobject_ops;

extern void ipc_init(void);

extern call_t *ipc_call_alloc(void);

extern errno_t ipc_call_sync(phone_t *, call_t *);
extern errno_t ipc_call(phone_t *, call_t *);
extern errno_t ipc_wait_for_call(answerbox_t *, uint32_t, unsigned int, call_t **);
extern errno_t ipc_forward(call_t *, phone_t *, answerbox_t *, unsigned int);
extern void ipc_answer(answerbox_t *, call_t *);
extern void _ipc_answer_free_call(call_t *, bool);

extern void ipc_phone_init(phone_t *, struct task *);
extern bool ipc_phone_connect(phone_t *, answerbox_t *);
extern errno_t ipc_phone_hangup(phone_t *);

extern void ipc_answerbox_init(answerbox_t *, struct task *);

extern void ipc_cleanup(void);
extern void ipc_backsend_err(phone_t *, call_t *, errno_t);
extern void ipc_answerbox_slam_phones(answerbox_t *, bool);
extern void ipc_cleanup_call_list(answerbox_t *, list_t *);

extern void ipc_print_task(task_id_t);

#endif

/** @}
 */
