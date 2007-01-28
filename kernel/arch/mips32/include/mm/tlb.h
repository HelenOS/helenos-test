/*
 * Copyright (c) 2003-2004 Jakub Jermar
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

/** @addtogroup mips32mm	
 * @{
 */
/** @file
 */

#ifndef KERN_mips32_TLB_H_
#define KERN_mips32_TLB_H_

#include <arch/exception.h>

#ifdef TLBCNT
#	define TLB_ENTRY_COUNT		TLBCNT
#else
#	define TLB_ENTRY_COUNT		48
#endif

#define TLB_WIRED		1
#define TLB_KSTACK_WIRED_INDEX	0

#define TLB_PAGE_MASK_16K	(0x3<<13)

#define PAGE_UNCACHED			2
#define PAGE_CACHEABLE_EXC_WRITE	5

typedef union {
	struct {
#ifdef BIG_ENDIAN
		unsigned : 2;		/* zero */
		unsigned pfn : 24;	/* frame number */
		unsigned c : 3; 	/* cache coherency attribute */
		unsigned d : 1; 	/* dirty/write-protect bit */
		unsigned v : 1; 	/* valid bit */
		unsigned g : 1; 	/* global bit */
#else
		unsigned g : 1; 	/* global bit */
		unsigned v : 1; 	/* valid bit */
		unsigned d : 1; 	/* dirty/write-protect bit */
		unsigned c : 3; 	/* cache coherency attribute */
		unsigned pfn : 24;	/* frame number */
		unsigned : 2;		/* zero */
#endif
	} __attribute__ ((packed));
	uint32_t value;
} entry_lo_t;

typedef union {
	struct {
#ifdef BIG_ENDIAN
		unsigned vpn2 : 19;
		unsigned : 5;
		unsigned asid : 8;
#else
		unsigned asid : 8;
		unsigned : 5;
		unsigned vpn2 : 19;
#endif
	} __attribute__ ((packed));
	uint32_t value;
} entry_hi_t;

typedef union {
	struct {
#ifdef BIG_ENDIAN
		unsigned : 7;
		unsigned mask : 12;
		unsigned : 13;
#else
		unsigned : 13;
		unsigned mask : 12;
		unsigned : 7;
#endif
	} __attribute__ ((packed));
	uint32_t value;
} page_mask_t;

typedef union {
	struct {
#ifdef BIG_ENDIAN
		unsigned p : 1;
		unsigned : 27;
		unsigned index : 4;
#else
		unsigned index : 4;
		unsigned : 27;
		unsigned p : 1;
#endif
	} __attribute__ ((packed));
	uint32_t value;
} tlb_index_t;

/** Probe TLB for Matching Entry
 *
 * Probe TLB for Matching Entry.
 */
static inline void tlbp(void)
{
	asm volatile ("tlbp\n\t");
}


/** Read Indexed TLB Entry
 *
 * Read Indexed TLB Entry.
 */
static inline void tlbr(void)
{
	asm volatile ("tlbr\n\t");
}

/** Write Indexed TLB Entry
 *
 * Write Indexed TLB Entry.
 */
static inline void tlbwi(void)
{
	asm volatile ("tlbwi\n\t");
}

/** Write Random TLB Entry
 *
 * Write Random TLB Entry.
 */
static inline void tlbwr(void)
{
	asm volatile ("tlbwr\n\t");
}

#define tlb_invalidate(asid)	tlb_invalidate_asid(asid)

extern void tlb_invalid(istate_t *istate);
extern void tlb_refill(istate_t *istate);
extern void tlb_modified(istate_t *istate);

#endif

/** @}
 */
