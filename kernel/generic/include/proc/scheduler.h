/*
 * Copyright (c) 2001-2004 Jakub Jermar
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

/** @addtogroup kernel_generic_proc
 * @{
 */
/** @file
 */

#ifndef KERN_SCHEDULER_H_
#define KERN_SCHEDULER_H_

#include <stddef.h>
#include <synch/spinlock.h>
#include <time/clock.h>
#include <atomic.h>
#include <adt/list.h>

#define RQ_COUNT          16
#define NEEDS_RELINK_MAX  (HZ)

/** Scheduler run queue structure. */
typedef struct {
	IRQ_SPINLOCK_DECLARE(lock);
	list_t rq;			/**< List of ready threads. */
	size_t n;			/**< Number of threads in rq_ready. */
} runq_t;

extern atomic_size_t nrdy;
extern void scheduler_init(void);

extern void scheduler_fpu_lazy_request(void);
extern void scheduler(void);
extern void kcpulb(void *arg);

extern void sched_print_list(void);

/*
 * To be defined by architectures.
 */
extern void before_task_runs_arch(void);
extern void before_thread_runs_arch(void);
extern void after_thread_ran_arch(void);

#endif

/** @}
 */
