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

/** @addtogroup kernel_generic_mm
 * @{
 */
/** @file
 */

#ifndef KERN_SLAB_H_
#define KERN_SLAB_H_

#include <adt/list.h>
#include <synch/spinlock.h>
#include <atomic.h>
#include <mm/frame.h>

/** Initial Magazine size (TODO: dynamically growing magazines) */
#define SLAB_MAG_SIZE  4

/** If object size is less, store control structure inside SLAB */
#define SLAB_INSIDE_SIZE  (PAGE_SIZE >> 3)

/** Maximum wasted space we allow for cache */
#define SLAB_MAX_BADNESS(cache) \
	(FRAMES2SIZE((cache)->frames) >> 2)

/* slab_reclaim constants */

/** Reclaim all possible memory, because we are in memory stress */
#define SLAB_RECLAIM_ALL  0x01

/* cache_create flags */

/** Do not use per-cpu cache */
#define SLAB_CACHE_NOMAGAZINE   0x01
/** Have control structure inside SLAB */
#define SLAB_CACHE_SLINSIDE     0x02
/** We add magazine cache later, if we have this flag */
#define SLAB_CACHE_MAGDEFERRED  (0x04 | SLAB_CACHE_NOMAGAZINE)

typedef struct {
	link_t link;
	size_t busy;  /**< Count of full slots in magazine */
	size_t size;  /**< Number of slots in magazine */
	void *objs[];  /**< Slots in magazine */
} slab_magazine_t;

typedef struct {
	slab_magazine_t *current;
	slab_magazine_t *last;
	IRQ_SPINLOCK_DECLARE(lock);
} slab_mag_cache_t;

typedef struct {
	const char *name;

	link_t link;

	/* Configuration */

	/** Size of slab position - align_up(sizeof(obj)) */
	size_t size;

	errno_t (*constructor)(void *obj, unsigned int kmflag);
	size_t (*destructor)(void *obj);

	/** Flags changing behaviour of cache */
	unsigned int flags;

	/* Computed values */
	size_t frames;   /**< Number of frames to be allocated */
	size_t objects;  /**< Number of objects that fit in */

	/* Statistics */
	atomic_size_t allocated_slabs;
	atomic_size_t allocated_objs;
	atomic_size_t cached_objs;
	/** How many magazines in magazines list */
	atomic_size_t magazine_counter;

	/* Slabs */
	list_t full_slabs;     /**< List of full slabs */
	list_t partial_slabs;  /**< List of partial slabs */
	IRQ_SPINLOCK_DECLARE(slablock);
	/* Magazines */
	list_t magazines;  /**< List o full magazines */
	IRQ_SPINLOCK_DECLARE(maglock);

	/** CPU cache */
	slab_mag_cache_t *mag_cache;
} slab_cache_t;

extern slab_cache_t *slab_cache_create(const char *, size_t, size_t,
    errno_t (*)(void *, unsigned int), size_t (*)(void *), unsigned int);
extern void slab_cache_destroy(slab_cache_t *);

extern void *slab_alloc(slab_cache_t *, unsigned int)
    __attribute__((malloc));
extern void slab_free(slab_cache_t *, void *);
extern size_t slab_reclaim(unsigned int);

/* slab subsytem initialization */
extern void slab_cache_init(void);
extern void slab_enable_cpucache(void);

/* kconsole debug */
extern void slab_print_list(void);

#endif

/** @}
 */
