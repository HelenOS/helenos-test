/*
 * Copyright (c) 2005 Jakub Vana
 * Copyright (c) 2005 Jakub Jermar
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

#include <test.h>
#include <atomic.h>
#include <stdbool.h>
#include <proc/thread.h>

#include <arch.h>

#define THREADS  5

static atomic_bool finish;
static atomic_size_t threads_finished;

static void threadtest(void *data)
{
	thread_detach(THREAD);

	while (atomic_load(&finish)) {
		TPRINTF("%" PRIu64 " ", THREAD->tid);
		thread_usleep(100000);
	}
	atomic_inc(&threads_finished);
}

const char *test_thread1(void)
{
	unsigned int i;
	size_t total = 0;

	atomic_store(&finish, true);
	atomic_store(&threads_finished, 0);

	for (i = 0; i < THREADS; i++) {
		thread_t *t;
		if (!(t = thread_create(threadtest, NULL, TASK,
		    THREAD_FLAG_NONE, "threadtest"))) {
			TPRINTF("Could not create thread %d\n", i);
			break;
		}
		thread_ready(t);
		total++;
	}

	TPRINTF("Running threads for 10 seconds...\n");
	thread_sleep(10);

	atomic_store(&finish, false);
	while (atomic_load(&threads_finished) < total) {
		TPRINTF("Threads left: %zu\n", total - atomic_load(&threads_finished));
		thread_sleep(1);
	}

	return NULL;
}
