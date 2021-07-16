/*
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

/** @addtogroup kernel_sparc64
 * @{
 */
/** @file
 */

#ifndef KERN_sparc64_sun4v_CPU_H_
#define KERN_sparc64_sun4v_CPU_H_

/** Maximum number of virtual processors. */
#define MAX_NUM_STRANDS  64

/** Maximum number of logical processors in a processor core */
#define MAX_CORE_STRANDS  8

#ifndef __ASSEMBLER__

#include <atomic.h>
#include <synch/spinlock.h>

struct cpu;

typedef struct {
	uint64_t exec_unit_id;
	uint8_t strand_count;
	uint64_t cpuids[MAX_CORE_STRANDS];
	struct cpu *cpus[MAX_CORE_STRANDS];
	atomic_size_t nrdy;
	SPINLOCK_DECLARE(proposed_nrdy_lock);
} exec_unit_t;

typedef struct cpu_arch {
	/** Virtual processor ID */
	uint64_t id;
	/** Processor frequency in Hz */
	uint32_t clock_frequency;
	/** Next clock interrupt should be generated when the TICK register
	 * matches this value.
	 */
	uint64_t next_tick_cmpr;
	/** Physical core. */
	exec_unit_t *exec_unit;
	/** Proposed No. of ready threads so that cores are equally balanced. */
	unsigned long proposed_nrdy;
} cpu_arch_t;

#endif

#endif

/** @}
 */
