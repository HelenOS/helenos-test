/*
 * Copyright (c) 2009 Jakub Jermar
 * Copyright (c) 2018 CZ.NIC, z.s.p.o.
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
 * @brief NS 16550 serial controller driver.
 */

#include <assert.h>
#include <genarch/drivers/ns16550/ns16550.h>
#include <ddi/irq.h>
#include <ddi/ddi.h>
#include <arch/asm.h>
#include <console/chardev.h>
#include <stdlib.h>
#include <align.h>
#include <str.h>

#define LSR_DATA_READY  0x01
#define LSR_TH_READY    0x20

#define RETRY_CNT	100000

#define NOTHING \
	do { \
	} while(0)

#define WHILE_CNT_AND_COND_DO(cnt, expr, statement) \
	do { \
		for (volatile unsigned i = 0; i < (cnt); i++) \
			if ((expr)) \
				statement; \
			else \
				break; \
	} while (0)

static uint8_t ns16550_reg_read(ns16550_instance_t *inst, ns16550_reg_t reg)
{
	return pio_read_8(&inst->ns16550[reg << inst->reg_shift]);
}

static void ns16550_reg_write(ns16550_instance_t *inst, ns16550_reg_t reg,
    uint8_t val)
{
	pio_write_8(&inst->ns16550[reg << inst->reg_shift], val);
}

static irq_ownership_t ns16550_claim(irq_t *irq)
{
	ns16550_instance_t *instance = irq->instance;

	if (ns16550_reg_read(instance, NS16550_REG_LSR) & LSR_DATA_READY)
		return IRQ_ACCEPT;
	else
		return IRQ_DECLINE;
}

static void ns16550_irq_handler(irq_t *irq)
{
	ns16550_instance_t *instance = irq->instance;

	while (ns16550_reg_read(instance, NS16550_REG_LSR) & LSR_DATA_READY) {
		uint8_t data = ns16550_reg_read(instance, NS16550_REG_RBR);
		indev_push_character(instance->input, data);
	}
}

/** Clear input buffer. */
static void ns16550_clear_buffer(ns16550_instance_t *instance)
{
	WHILE_CNT_AND_COND_DO(RETRY_CNT,
	    ns16550_reg_read(instance, NS16550_REG_LSR) & LSR_DATA_READY,
	    (void) ns16550_reg_read(instance, NS16550_REG_RBR));
}

static void ns16550_sendb(ns16550_instance_t *instance, uint8_t byte)
{
	WHILE_CNT_AND_COND_DO(RETRY_CNT,
	    !(ns16550_reg_read(instance, NS16550_REG_LSR) & LSR_TH_READY),
	    NOTHING);

	ns16550_reg_write(instance, NS16550_REG_THR, byte);
}

static void ns16550_putuchar(outdev_t *dev, char32_t ch)
{
	ns16550_instance_t *instance = (ns16550_instance_t *) dev->data;

	if ((!instance->parea.mapped) || (console_override)) {
		if (ch == '\n')
			ns16550_sendb(instance, '\r');

		if (ascii_check(ch))
			ns16550_sendb(instance, (uint8_t) ch);
		else
			ns16550_sendb(instance, U_SPECIAL);
	}
}

static outdev_operations_t ns16550_ops = {
	.write = ns16550_putuchar,
	.redraw = NULL
};

/** Configure ns16550 transmission format.
 *
 * @param instance   NS 16550 driver instance.
 * @param baud_rate  Transmission speed in bits per second, also known as baud,
 *                   maximum value is 115200.
 * @param lcr_format Line Control Register configuration bits, as defined by
 *                   the @c LCR_ macros.  These configure the word width,
 *                   parity type, and stop bit count.
 */
static void ns16550_format_set(ns16550_instance_t *instance,
    unsigned baud_rate, uint8_t lcr_format)
{
	uint16_t divisor;

	divisor = (uint16_t)(NS156440_CLOCK / baud_rate);
	if (divisor == 0)
		divisor = 1;  /* Avoid division by zero. */

	ns16550_reg_write(instance, NS16550_REG_LCR, LCR_DLAB);
	ns16550_reg_write(instance, NS16550_REG_DLL, divisor & 0xFF);
	ns16550_reg_write(instance, NS16550_REG_DLH, (divisor >> 8) & 0xFF);
	ns16550_reg_write(instance, NS16550_REG_LCR, lcr_format & ~LCR_DLAB);
}

/** Initialize ns16550.
 *
 * @param dev        Address of the beginning of the device in I/O space.
 * @param reg_shift  Spacing between individual register addresses, in log2.
 *                   The individual register location is calculated as
 *                   `base + (register offset << reg_shift)`.
 * @param inr        Interrupt number.
 * @param cir        Clear interrupt function.
 * @param cir_arg    First argument to cir.
 * @param output     Where to store pointer to the output device
 *                   or NULL if the caller is not interested in
 *                   writing to the serial port.
 *
 * @return Keyboard instance or NULL on failure.
 *
 */
ns16550_instance_t *ns16550_init(ioport8_t *dev_phys, unsigned reg_shift,
    inr_t inr, cir_t cir, void *cir_arg, outdev_t **output)
{
	size_t size = 6 * (1U << reg_shift);
	ioport8_t *dev = pio_map((void *) dev_phys, size);

	ns16550_instance_t *instance = malloc(sizeof(ns16550_instance_t));
	if (instance) {
		instance->ns16550 = dev;
		instance->reg_shift = reg_shift;
		instance->input = NULL;
		instance->output = NULL;

		ns16550_format_set(instance, 38400,
		    LCR_PARITY_NONE | LCR_STOP_BIT_TWO | LCR_WORD_LEN_8);

		if (output) {
			instance->output = malloc(sizeof(outdev_t));
			if (!instance->output) {
				free(instance);
				pio_unmap((void *) dev_phys, (void *) dev,
				    size);
				return NULL;
			}

			outdev_initialize("ns16550", instance->output,
			    &ns16550_ops);
			instance->output->data = instance;
			*output = instance->output;
		}

		irq_initialize(&instance->irq);
		instance->irq.inr = inr;
		instance->irq.claim = ns16550_claim;
		instance->irq.handler = ns16550_irq_handler;
		instance->irq.instance = instance;
		instance->irq.cir = cir;
		instance->irq.cir_arg = cir_arg;

		ddi_parea_init(&instance->parea);
		instance->parea.pbase = ALIGN_DOWN((uintptr_t) dev_phys,
		    PAGE_SIZE);
		instance->parea.frames = ALIGN_UP(size, PAGE_SIZE);
		instance->parea.unpriv = false;
		instance->parea.mapped = false;
		ddi_parea_register(&instance->parea);
	}

	return instance;
}

void ns16550_wire(ns16550_instance_t *instance, indev_t *input)
{
	assert(instance);
	assert(input);

	instance->input = input;
	irq_register(&instance->irq);

	ns16550_clear_buffer(instance);

	/* Enable interrupts */
	ns16550_reg_write(instance, NS16550_REG_IER, IER_ERBFI);
	ns16550_reg_write(instance, NS16550_REG_MCR, MCR_OUT2);
}

/** @}
 */
