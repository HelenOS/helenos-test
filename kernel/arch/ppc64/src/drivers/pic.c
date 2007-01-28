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

/** @addtogroup ppc64
 * @{
 */
/** @file
 */


#include <arch/drivers/pic.h>
#include <mm/page.h>
#include <byteorder.h>
#include <bitops.h>

static volatile uint32_t *pic;

void pic_init(uintptr_t base, size_t size)
{
	pic = (uint32_t *) hw_map(base, size);
}



void pic_enable_interrupt(int intnum)
{
	if (intnum < 32) {
		pic[PIC_MASK_LOW] = pic[PIC_MASK_LOW] | (1 << intnum);
	} else {
		pic[PIC_MASK_HIGH] = pic[PIC_MASK_HIGH] | (1 << (intnum - 32));
	}
	
}

void pic_disable_interrupt(int intnum)
{
	if (intnum < 32) {
		pic[PIC_MASK_LOW] = pic[PIC_MASK_LOW] & (~(1 << intnum));
	} else {
		pic[PIC_MASK_HIGH] = pic[PIC_MASK_HIGH] & (~(1 << (intnum - 32)));
	}
}

void pic_ack_interrupt(int intnum)
{
	if (intnum < 32) 
		pic[PIC_ACK_LOW] = 1 << intnum;
	else 
		pic[PIC_ACK_HIGH] = 1 << (intnum - 32);
}

/** Return number of pending interrupt */
int pic_get_pending(void)
{
	int pending;

	pending = pic[PIC_PENDING_LOW];
	if (pending)
		return fnzb32(pending);
	
	pending = pic[PIC_PENDING_HIGH];
	if (pending)
		return fnzb32(pending) + 32;
	
	return -1;
}

/** @}
 */
