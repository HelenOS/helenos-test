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

/** @addtogroup kernel_genarch
 * @{
 */
/**
 * @file
 * @brief Headers for NS 16550 serial controller.
 */

#ifndef KERN_NS16550_H_
#define KERN_NS16550_H_

#include <ddi/ddi.h>
#include <ddi/irq.h>
#include <typedefs.h>
#include <console/chardev.h>

#define NS156440_CLOCK    115200 /** Internal clock speed, max. baud rate. */

#define IER_ERBFI         0x01   /** Enable Receive Buffer Full Interrupt. */
#define MCR_OUT2          0x08   /** OUT2. */

#define LCR_DLAB          0x80   /** Divisor Latch Access bit. */
#define LCR_SBE           0x40   /** RS-232 Break Signal bit. */

#define LCR_PARITY_NONE   0x00   /** No parity bit. */
#define LCR_PARITY_ODD    0x08   /** Odd parity. */
#define LCR_PARITY_EVEN   0x18   /** Even parity. */
#define LCR_PARITY_MARK   0x28   /** Parity bit always one. */
#define LCR_PARITY_SPACE  0x38   /** Parity bit always zero. */

#define LCR_STOP_BIT_ONE  0x00   /** One stop bit. */
#define LCR_STOP_BIT_TWO  0x04   /** Two stop bits. */

#define LCR_WORD_LEN_5    0x00   /** 5-bit word length. */
#define LCR_WORD_LEN_6    0x01   /** 6-bit word length. */
#define LCR_WORD_LEN_7    0x02   /** 7-bit word length. */
#define LCR_WORD_LEN_8    0x03   /** 8-bit word length. */

/** NS16550 registers. */
typedef enum {
	NS16550_REG_RBR = 0,  /**< Receiver Buffer Register (read). */
	NS16550_REG_THR = 0,  /**< Transmitter Holder Register (write). */
	NS16550_REG_DLL = 0,  /**< Baud rate divisor latch low byte (write). */
	NS16550_REG_IER = 1,  /**< Interrupt Enable Register. */
	NS16550_REG_DLH = 1,  /**< Baud rate divisor latch high byte (write). */
	NS16550_REG_IIR = 2,  /**< Interrupt Ident Register (read). */
	NS16550_REG_FCR = 2,  /**< FIFO control register (write). */
	NS16550_REG_LCR = 3,  /**< Line Control register. */
	NS16550_REG_MCR = 4,  /**< Modem Control Register. */
	NS16550_REG_LSR = 5,  /**< Line Status Register. */
} ns16550_reg_t;

/** Structure representing the ns16550 device. */
typedef struct {
	irq_t irq;
	volatile ioport8_t *ns16550;
	indev_t *input;
	outdev_t *output;
	parea_t parea;
	int reg_shift;
} ns16550_instance_t;

extern ns16550_instance_t *ns16550_init(ioport8_t *, unsigned, inr_t, cir_t,
    void *, outdev_t **);
extern void ns16550_wire(ns16550_instance_t *, indev_t *);

#endif

/** @}
 */
