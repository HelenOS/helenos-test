/*
 * Copyright (c) 2010 Jakub Jermar
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

/**
 * @file
 * @brief Scheduler and load balancing.
 *
 * This file contains the scheduler and kcpulb kernel thread which
 * performs load-balancing of per-CPU run queues.
 */

#include <assert.h>
#include <atomic.h>
#include <proc/scheduler.h>
#include <proc/thread.h>
#include <proc/task.h>
#include <mm/frame.h>
#include <mm/page.h>
#include <mm/as.h>
#include <time/timeout.h>
#include <time/delay.h>
#include <arch/asm.h>
#include <arch/faddr.h>
#include <arch/cycle.h>
#include <atomic.h>
#include <synch/spinlock.h>
#include <config.h>
#include <context.h>
#include <fpu_context.h>
#include <halt.h>
#include <arch.h>
#include <adt/list.h>
#include <panic.h>
#include <cpu.h>
#include <stdio.h>
#include <log.h>
#include <stacktrace.h>

static void scheduler_separated_stack(void);

atomic_size_t nrdy;  /**< Number of ready threads in the system. */

/** Carry out actions before new task runs. */
static void before_task_runs(void)
{
	before_task_runs_arch();
}

/** Take actions before new thread runs.
 *
 * Perform actions that need to be
 * taken before the newly selected
 * thread is passed control.
 *
 * THREAD->lock is locked on entry
 *
 */
static void before_thread_runs(void)
{
	before_thread_runs_arch();

#ifdef CONFIG_FPU_LAZY
	if (THREAD == CPU->fpu_owner)
		fpu_enable();
	else
		fpu_disable();
#elif defined CONFIG_FPU
	fpu_enable();
	if (THREAD->fpu_context_exists)
		fpu_context_restore(THREAD->saved_fpu_context);
	else {
		fpu_init();
		THREAD->fpu_context_exists = true;
	}
#endif

#ifdef CONFIG_UDEBUG
	if (THREAD->btrace) {
		istate_t *istate = THREAD->udebug.uspace_state;
		if (istate != NULL) {
			printf("Thread %" PRIu64 " stack trace:\n", THREAD->tid);
			stack_trace_istate(istate);
		}

		THREAD->btrace = false;
	}
#endif
}

/** Take actions after THREAD had run.
 *
 * Perform actions that need to be
 * taken after the running thread
 * had been preempted by the scheduler.
 *
 * THREAD->lock is locked on entry
 *
 */
static void after_thread_ran(void)
{
	after_thread_ran_arch();
}

#ifdef CONFIG_FPU_LAZY
void scheduler_fpu_lazy_request(void)
{
	fpu_enable();
	irq_spinlock_lock(&CPU->lock, false);

	/* Save old context */
	if (CPU->fpu_owner != NULL) {
		irq_spinlock_lock(&CPU->fpu_owner->lock, false);
		fpu_context_save(CPU->fpu_owner->saved_fpu_context);

		/* Don't prevent migration */
		CPU->fpu_owner->fpu_context_engaged = false;
		irq_spinlock_unlock(&CPU->fpu_owner->lock, false);
		CPU->fpu_owner = NULL;
	}

	irq_spinlock_lock(&THREAD->lock, false);
	if (THREAD->fpu_context_exists) {
		fpu_context_restore(THREAD->saved_fpu_context);
	} else {
		fpu_init();
		THREAD->fpu_context_exists = true;
	}

	CPU->fpu_owner = THREAD;
	THREAD->fpu_context_engaged = true;
	irq_spinlock_unlock(&THREAD->lock, false);

	irq_spinlock_unlock(&CPU->lock, false);
}
#endif /* CONFIG_FPU_LAZY */

/** Initialize scheduler
 *
 * Initialize kernel scheduler.
 *
 */
void scheduler_init(void)
{
}

/** Get thread to be scheduled
 *
 * Get the optimal thread to be scheduled
 * according to thread accounting and scheduler
 * policy.
 *
 * @return Thread to be scheduled.
 *
 */
static thread_t *find_best_thread(void)
{
	assert(CPU != NULL);

loop:

	if (atomic_load(&CPU->nrdy) == 0) {
		/*
		 * For there was nothing to run, the CPU goes to sleep
		 * until a hardware interrupt or an IPI comes.
		 * This improves energy saving and hyperthreading.
		 */
		irq_spinlock_lock(&CPU->lock, false);
		CPU->idle = true;
		irq_spinlock_unlock(&CPU->lock, false);
		interrupts_enable();

		/*
		 * An interrupt might occur right now and wake up a thread.
		 * In such case, the CPU will continue to go to sleep
		 * even though there is a runnable thread.
		 */
		cpu_sleep();
		interrupts_disable();
		goto loop;
	}

	assert(!CPU->idle);

	unsigned int i;
	for (i = 0; i < RQ_COUNT; i++) {
		irq_spinlock_lock(&(CPU->rq[i].lock), false);
		if (CPU->rq[i].n == 0) {
			/*
			 * If this queue is empty, try a lower-priority queue.
			 */
			irq_spinlock_unlock(&(CPU->rq[i].lock), false);
			continue;
		}

		atomic_dec(&CPU->nrdy);
		atomic_dec(&nrdy);
		CPU->rq[i].n--;

		/*
		 * Take the first thread from the queue.
		 */
		thread_t *thread = list_get_instance(
		    list_first(&CPU->rq[i].rq), thread_t, rq_link);
		list_remove(&thread->rq_link);

		irq_spinlock_pass(&(CPU->rq[i].lock), &thread->lock);

		thread->cpu = CPU;
		thread->ticks = us2ticks((i + 1) * 10000);
		thread->priority = i;  /* Correct rq index */

		/*
		 * Clear the stolen flag so that it can be migrated
		 * when load balancing needs emerge.
		 */
		thread->stolen = false;
		irq_spinlock_unlock(&thread->lock, false);

		return thread;
	}

	goto loop;
}

/** Prevent rq starvation
 *
 * Prevent low priority threads from starving in rq's.
 *
 * When the function decides to relink rq's, it reconnects
 * respective pointers so that in result threads with 'pri'
 * greater or equal start are moved to a higher-priority queue.
 *
 * @param start Threshold priority.
 *
 */
static void relink_rq(int start)
{
	list_t list;

	list_initialize(&list);
	irq_spinlock_lock(&CPU->lock, false);

	if (CPU->needs_relink > NEEDS_RELINK_MAX) {
		int i;
		for (i = start; i < RQ_COUNT - 1; i++) {
			/* Remember and empty rq[i + 1] */

			irq_spinlock_lock(&CPU->rq[i + 1].lock, false);
			list_concat(&list, &CPU->rq[i + 1].rq);
			size_t n = CPU->rq[i + 1].n;
			CPU->rq[i + 1].n = 0;
			irq_spinlock_unlock(&CPU->rq[i + 1].lock, false);

			/* Append rq[i + 1] to rq[i] */

			irq_spinlock_lock(&CPU->rq[i].lock, false);
			list_concat(&CPU->rq[i].rq, &list);
			CPU->rq[i].n += n;
			irq_spinlock_unlock(&CPU->rq[i].lock, false);
		}

		CPU->needs_relink = 0;
	}

	irq_spinlock_unlock(&CPU->lock, false);
}

/** The scheduler
 *
 * The thread scheduling procedure.
 * Passes control directly to
 * scheduler_separated_stack().
 *
 */
void scheduler(void)
{
	volatile ipl_t ipl;

	assert(CPU != NULL);

	ipl = interrupts_disable();

	if (atomic_load(&haltstate))
		halt();

	if (THREAD) {
		irq_spinlock_lock(&THREAD->lock, false);

		/* Update thread kernel accounting */
		THREAD->kcycles += get_cycle() - THREAD->last_cycle;

#if (defined CONFIG_FPU) && (!defined CONFIG_FPU_LAZY)
		fpu_context_save(THREAD->saved_fpu_context);
#endif
		if (!context_save(&THREAD->saved_context)) {
			/*
			 * This is the place where threads leave scheduler();
			 */

			/* Save current CPU cycle */
			THREAD->last_cycle = get_cycle();

			irq_spinlock_unlock(&THREAD->lock, false);
			interrupts_restore(THREAD->saved_context.ipl);

			return;
		}

		/*
		 * Interrupt priority level of preempted thread is recorded
		 * here to facilitate scheduler() invocations from
		 * interrupts_disable()'d code (e.g. waitq_sleep_timeout()).
		 *
		 */
		THREAD->saved_context.ipl = ipl;
	}

	/*
	 * Through the 'CURRENT' structure, we keep track of THREAD, TASK, CPU, AS
	 * and preemption counter. At this point CURRENT could be coming either
	 * from THREAD's or CPU's stack.
	 *
	 */
	current_copy(CURRENT, (current_t *) CPU->stack);

	/*
	 * We may not keep the old stack.
	 * Reason: If we kept the old stack and got blocked, for instance, in
	 * find_best_thread(), the old thread could get rescheduled by another
	 * CPU and overwrite the part of its own stack that was also used by
	 * the scheduler on this CPU.
	 *
	 * Moreover, we have to bypass the compiler-generated POP sequence
	 * which is fooled by SP being set to the very top of the stack.
	 * Therefore the scheduler() function continues in
	 * scheduler_separated_stack().
	 *
	 */
	context_save(&CPU->saved_context);
	context_set(&CPU->saved_context, FADDR(scheduler_separated_stack),
	    (uintptr_t) CPU->stack, STACK_SIZE);
	context_restore(&CPU->saved_context);

	/* Not reached */
}

/** Scheduler stack switch wrapper
 *
 * Second part of the scheduler() function
 * using new stack. Handling the actual context
 * switch to a new thread.
 *
 */
void scheduler_separated_stack(void)
{
	DEADLOCK_PROBE_INIT(p_joinwq);
	task_t *old_task = TASK;
	as_t *old_as = AS;

	assert((!THREAD) || (irq_spinlock_locked(&THREAD->lock)));
	assert(CPU != NULL);
	assert(interrupts_disabled());

	/*
	 * Hold the current task and the address space to prevent their
	 * possible destruction should thread_destroy() be called on this or any
	 * other processor while the scheduler is still using them.
	 */
	if (old_task)
		task_hold(old_task);

	if (old_as)
		as_hold(old_as);

	if (THREAD) {
		/* Must be run after the switch to scheduler stack */
		after_thread_ran();

		switch (THREAD->state) {
		case Running:
			irq_spinlock_unlock(&THREAD->lock, false);
			thread_ready(THREAD);
			break;

		case Exiting:
		repeat:
			if (THREAD->detached) {
				thread_destroy(THREAD, false);
			} else {
				/*
				 * The thread structure is kept allocated until
				 * somebody calls thread_detach() on it.
				 */
				if (!irq_spinlock_trylock(&THREAD->join_wq.lock)) {
					/*
					 * Avoid deadlock.
					 */
					irq_spinlock_unlock(&THREAD->lock, false);
					delay(HZ);
					irq_spinlock_lock(&THREAD->lock, false);
					DEADLOCK_PROBE(p_joinwq,
					    DEADLOCK_THRESHOLD);
					goto repeat;
				}
				_waitq_wakeup_unsafe(&THREAD->join_wq,
				    WAKEUP_FIRST);
				irq_spinlock_unlock(&THREAD->join_wq.lock, false);

				THREAD->state = Lingering;
				irq_spinlock_unlock(&THREAD->lock, false);
			}
			break;

		case Sleeping:
			/*
			 * Prefer the thread after it's woken up.
			 */
			THREAD->priority = -1;

			/*
			 * We need to release wq->lock which we locked in
			 * waitq_sleep(). Address of wq->lock is kept in
			 * THREAD->sleep_queue.
			 */
			irq_spinlock_unlock(&THREAD->sleep_queue->lock, false);

			irq_spinlock_unlock(&THREAD->lock, false);
			break;

		default:
			/*
			 * Entering state is unexpected.
			 */
			panic("tid%" PRIu64 ": unexpected state %s.",
			    THREAD->tid, thread_states[THREAD->state]);
			break;
		}

		THREAD = NULL;
	}

	THREAD = find_best_thread();

	irq_spinlock_lock(&THREAD->lock, false);
	int priority = THREAD->priority;
	irq_spinlock_unlock(&THREAD->lock, false);

	relink_rq(priority);

	/*
	 * If both the old and the new task are the same,
	 * lots of work is avoided.
	 */
	if (TASK != THREAD->task) {
		as_t *new_as = THREAD->task->as;

		/*
		 * Note that it is possible for two tasks
		 * to share one address space.
		 */
		if (old_as != new_as) {
			/*
			 * Both tasks and address spaces are different.
			 * Replace the old one with the new one.
			 */
			as_switch(old_as, new_as);
		}

		TASK = THREAD->task;
		before_task_runs();
	}

	if (old_task)
		task_release(old_task);

	if (old_as)
		as_release(old_as);

	irq_spinlock_lock(&THREAD->lock, false);
	THREAD->state = Running;

#ifdef SCHEDULER_VERBOSE
	log(LF_OTHER, LVL_DEBUG,
	    "cpu%u: tid %" PRIu64 " (priority=%d, ticks=%" PRIu64
	    ", nrdy=%zu)", CPU->id, THREAD->tid, THREAD->priority,
	    THREAD->ticks, atomic_load(&CPU->nrdy));
#endif

	/*
	 * Some architectures provide late kernel PA2KA(identity)
	 * mapping in a page fault handler. However, the page fault
	 * handler uses the kernel stack of the running thread and
	 * therefore cannot be used to map it. The kernel stack, if
	 * necessary, is to be mapped in before_thread_runs(). This
	 * function must be executed before the switch to the new stack.
	 */
	before_thread_runs();

	/*
	 * Copy the knowledge of CPU, TASK, THREAD and preemption counter to
	 * thread's stack.
	 */
	current_copy(CURRENT, (current_t *) THREAD->kstack);

	context_restore(&THREAD->saved_context);

	/* Not reached */
}

#ifdef CONFIG_SMP
/** Load balancing thread
 *
 * SMP load balancing thread, supervising thread supplies
 * for the CPU it's wired to.
 *
 * @param arg Generic thread argument (unused).
 *
 */
void kcpulb(void *arg)
{
	size_t average;
	size_t rdy;

	/*
	 * Detach kcpulb as nobody will call thread_join_timeout() on it.
	 */
	thread_detach(THREAD);

loop:
	/*
	 * Work in 1s intervals.
	 */
	thread_sleep(1);

not_satisfied:
	/*
	 * Calculate the number of threads that will be migrated/stolen from
	 * other CPU's. Note that situation can have changed between two
	 * passes. Each time get the most up to date counts.
	 *
	 */
	average = atomic_load(&nrdy) / config.cpu_active + 1;
	rdy = atomic_load(&CPU->nrdy);

	if (average <= rdy)
		goto satisfied;

	size_t count = average - rdy;

	/*
	 * Searching least priority queues on all CPU's first and most priority
	 * queues on all CPU's last.
	 */
	size_t acpu;
	size_t acpu_bias = 0;
	int rq;

	for (rq = RQ_COUNT - 1; rq >= 0; rq--) {
		for (acpu = 0; acpu < config.cpu_active; acpu++) {
			cpu_t *cpu = &cpus[(acpu + acpu_bias) % config.cpu_active];

			/*
			 * Not interested in ourselves.
			 * Doesn't require interrupt disabling for kcpulb has
			 * THREAD_FLAG_WIRED.
			 *
			 */
			if (CPU == cpu)
				continue;

			if (atomic_load(&cpu->nrdy) <= average)
				continue;

			irq_spinlock_lock(&(cpu->rq[rq].lock), true);
			if (cpu->rq[rq].n == 0) {
				irq_spinlock_unlock(&(cpu->rq[rq].lock), true);
				continue;
			}

			thread_t *thread = NULL;

			/* Search rq from the back */
			link_t *link = list_last(&cpu->rq[rq].rq);

			while (link != NULL) {
				thread = (thread_t *) list_get_instance(link,
				    thread_t, rq_link);

				/*
				 * Do not steal CPU-wired threads, threads
				 * already stolen, threads for which migration
				 * was temporarily disabled or threads whose
				 * FPU context is still in the CPU.
				 */
				irq_spinlock_lock(&thread->lock, false);

				if ((!thread->wired) && (!thread->stolen) &&
				    (!thread->nomigrate) &&
				    (!thread->fpu_context_engaged)) {
					/*
					 * Remove thread from ready queue.
					 */
					irq_spinlock_unlock(&thread->lock,
					    false);

					atomic_dec(&cpu->nrdy);
					atomic_dec(&nrdy);

					cpu->rq[rq].n--;
					list_remove(&thread->rq_link);

					break;
				}

				irq_spinlock_unlock(&thread->lock, false);

				link = list_prev(link, &cpu->rq[rq].rq);
				thread = NULL;
			}

			if (thread) {
				/*
				 * Ready thread on local CPU
				 */

				irq_spinlock_pass(&(cpu->rq[rq].lock),
				    &thread->lock);

#ifdef KCPULB_VERBOSE
				log(LF_OTHER, LVL_DEBUG,
				    "kcpulb%u: TID %" PRIu64 " -> cpu%u, "
				    "nrdy=%ld, avg=%ld", CPU->id, thread->tid,
				    CPU->id, atomic_load(&CPU->nrdy),
				    atomic_load(&nrdy) / config.cpu_active);
#endif

				thread->stolen = true;
				thread->state = Entering;

				irq_spinlock_unlock(&thread->lock, true);
				thread_ready(thread);

				if (--count == 0)
					goto satisfied;

				/*
				 * We are not satisfied yet, focus on another
				 * CPU next time.
				 *
				 */
				acpu_bias++;

				continue;
			} else
				irq_spinlock_unlock(&(cpu->rq[rq].lock), true);

		}
	}

	if (atomic_load(&CPU->nrdy)) {
		/*
		 * Be a little bit light-weight and let migrated threads run.
		 *
		 */
		scheduler();
	} else {
		/*
		 * We failed to migrate a single thread.
		 * Give up this turn.
		 *
		 */
		goto loop;
	}

	goto not_satisfied;

satisfied:
	goto loop;
}
#endif /* CONFIG_SMP */

/** Print information about threads & scheduler queues
 *
 */
void sched_print_list(void)
{
	size_t cpu;
	for (cpu = 0; cpu < config.cpu_count; cpu++) {
		if (!cpus[cpu].active)
			continue;

		irq_spinlock_lock(&cpus[cpu].lock, true);

		printf("cpu%u: address=%p, nrdy=%zu, needs_relink=%zu\n",
		    cpus[cpu].id, &cpus[cpu], atomic_load(&cpus[cpu].nrdy),
		    cpus[cpu].needs_relink);

		unsigned int i;
		for (i = 0; i < RQ_COUNT; i++) {
			irq_spinlock_lock(&(cpus[cpu].rq[i].lock), false);
			if (cpus[cpu].rq[i].n == 0) {
				irq_spinlock_unlock(&(cpus[cpu].rq[i].lock), false);
				continue;
			}

			printf("\trq[%u]: ", i);
			list_foreach(cpus[cpu].rq[i].rq, rq_link, thread_t,
			    thread) {
				printf("%" PRIu64 "(%s) ", thread->tid,
				    thread_states[thread->state]);
			}
			printf("\n");

			irq_spinlock_unlock(&(cpus[cpu].rq[i].lock), false);
		}

		irq_spinlock_unlock(&cpus[cpu].lock, true);
	}
}

/** @}
 */
