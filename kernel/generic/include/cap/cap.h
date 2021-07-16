/*
 * Copyright (c) 2017 Jakub Jermar
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

/** @addtogroup kernel_generic
 * @{
 */
/** @file
 */

#ifndef KERN_CAP_H_
#define KERN_CAP_H_

#include <abi/cap.h>
#include <typedefs.h>
#include <adt/list.h>
#include <adt/hash.h>
#include <adt/hash_table.h>
#include <lib/ra.h>
#include <synch/mutex.h>
#include <atomic.h>

typedef enum {
	CAP_STATE_FREE,
	CAP_STATE_ALLOCATED,
	CAP_STATE_PUBLISHED
} cap_state_t;

typedef enum {
	KOBJECT_TYPE_CALL,
	KOBJECT_TYPE_IRQ,
	KOBJECT_TYPE_PHONE,
	KOBJECT_TYPE_WAITQ,
	KOBJECT_TYPE_MAX
} kobject_type_t;

struct task;

struct call;
struct irq;
struct phone;
struct waitq;

typedef struct kobject_ops {
	void (*destroy)(void *);
} kobject_ops_t;

extern kobject_ops_t *kobject_ops[];

#define KOBJECT_OP(k)	kobject_ops[(k)->type]

/*
 * Everything in kobject_t except for the atomic reference count, the capability
 * list and its lock is imutable.
 */
typedef struct kobject {
	kobject_type_t type;
	atomic_size_t refcnt;

	/** Mutex protecting caps_list */
	mutex_t caps_list_lock;
	/** List of published capabilities associated with the kobject */
	list_t caps_list;

	union {
		void *raw;
		struct call *call;
		struct irq *irq;
		struct phone *phone;
		struct waitq *waitq;
	};
} kobject_t;

/*
 * A cap_t may only be accessed under the protection of the cap_info_t lock.
 */
typedef struct cap {
	cap_state_t state;

	struct task *task;
	cap_handle_t handle;

	/** Link to the kobject's list of capabilities. */
	link_t kobj_link;

	/* Link to the task's capabilities of the same kobject type. */
	link_t type_link;

	ht_link_t caps_link;

	/* The underlying kernel object. */
	kobject_t *kobject;
} cap_t;

typedef struct cap_info {
	mutex_t lock;

	list_t type_list[KOBJECT_TYPE_MAX];

	hash_table_t caps;
	ra_arena_t *handles;
} cap_info_t;

extern void caps_init(void);
extern errno_t caps_task_alloc(struct task *);
extern void caps_task_free(struct task *);
extern void caps_task_init(struct task *);
extern bool caps_apply_to_kobject_type(struct task *, kobject_type_t,
    bool (*)(cap_t *, void *), void *);

extern errno_t cap_alloc(struct task *, cap_handle_t *);
extern void cap_publish(struct task *, cap_handle_t, kobject_t *);
extern kobject_t *cap_unpublish(struct task *, cap_handle_t, kobject_type_t);
extern void cap_revoke(kobject_t *);
extern void cap_free(struct task *, cap_handle_t);

extern kobject_t *kobject_alloc(unsigned int);
extern void kobject_free(kobject_t *);
extern void kobject_initialize(kobject_t *, kobject_type_t, void *);
extern kobject_t *kobject_get(struct task *, cap_handle_t, kobject_type_t);
extern void kobject_add_ref(kobject_t *);
extern void kobject_put(kobject_t *);

#endif

/** @}
 */
