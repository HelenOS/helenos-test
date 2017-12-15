/*
 * Copyright (c) 2011 Jan Vesely
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

/** @addtogroup drvusbehci
 * @{
 */
/** @file
 * @brief EHCI driver
 */

#include <assert.h>
#include <stdlib.h>
#include <usb/host/utils/malloc32.h>
#include <usb/host/bandwidth.h>
#include <usb/debug.h>

#include "ehci_bus.h"
#include "ehci_batch.h"
#include "hc.h"

/** Callback to set toggle on ED.
 *
 * @param[in] hcd_ep hcd endpoint structure
 * @param[in] toggle new value of toggle bit
 */
static void ehci_ep_toggle_set(endpoint_t *ep, bool toggle)
{
	ehci_endpoint_t *instance = ehci_endpoint_get(ep);
	assert(instance);
	assert(instance->qh);
	ep->toggle = toggle;
	if (qh_toggle_from_td(instance->qh))
		usb_log_warning("EP(%p): Setting toggle bit for transfer "
		    "directed EP", instance);
	qh_toggle_set(instance->qh, toggle);
}

/** Callback to get value of toggle bit.
 *
 * @param[in] hcd_ep hcd endpoint structure
 * @return Current value of toggle bit.
 */
static bool ehci_ep_toggle_get(endpoint_t *ep)
{
	ehci_endpoint_t *instance = ehci_endpoint_get(ep);
	assert(instance);
	assert(instance->qh);

	if (qh_toggle_from_td(instance->qh))
		usb_log_warning("EP(%p): Reading useless toggle bit", instance);
	return qh_toggle_get(instance->qh);
}

/** Creates new hcd endpoint representation.
 */
static endpoint_t *ehci_endpoint_create(device_t *dev, const usb_endpoint_desc_t *desc)
{
	assert(dev);

	ehci_endpoint_t *ehci_ep = malloc(sizeof(ehci_endpoint_t));
	if (ehci_ep == NULL)
		return NULL;

	endpoint_init(&ehci_ep->base, dev, desc);

	// TODO: extract USB2 information from desc

	ehci_ep->qh = malloc32(sizeof(qh_t));
	if (ehci_ep->qh == NULL) {
		free(ehci_ep);
		return NULL;
	}

	link_initialize(&ehci_ep->link);
	return &ehci_ep->base;
}

/** Disposes hcd endpoint structure
 *
 * @param[in] hcd driver using this instance.
 * @param[in] ep endpoint structure.
 */
static void ehci_endpoint_destroy(endpoint_t *ep)
{
	assert(ep);
	ehci_endpoint_t *instance = ehci_endpoint_get(ep);

	free32(instance->qh);
	free(instance);
}


static int ehci_register_ep(endpoint_t *ep)
{
	bus_t *bus_base = endpoint_get_bus(ep);
	ehci_bus_t *bus = (ehci_bus_t *) bus_base;
	ehci_endpoint_t *ehci_ep = ehci_endpoint_get(ep);
	assert(fibril_mutex_is_locked(&bus_base->guard));

	const int err = usb2_bus_ops.endpoint_register(ep);
	if (err)
		return err;

	qh_init(ehci_ep->qh, ep);
	hc_enqueue_endpoint(bus->hc, ep);

	return EOK;
}

static int ehci_unregister_ep(endpoint_t *ep)
{
	bus_t *bus_base = endpoint_get_bus(ep);
	ehci_bus_t *bus = (ehci_bus_t *) bus_base;
	assert(bus);
	assert(ep);

	const int err = usb2_bus_ops.endpoint_unregister(ep);
	if (err)
		return err;

	hc_dequeue_endpoint(bus->hc, ep);
	return EOK;
}

static usb_transfer_batch_t *ehci_create_batch(endpoint_t *ep)
{
	ehci_transfer_batch_t *batch = ehci_transfer_batch_create(ep);
	return &batch->base;
}

static void ehci_destroy_batch(usb_transfer_batch_t *batch)
{
	ehci_transfer_batch_destroy(ehci_transfer_batch_get(batch));
}

static const bus_ops_t ehci_bus_ops = {
	.parent = &usb2_bus_ops,

	.endpoint_destroy = ehci_endpoint_destroy,
	.endpoint_create = ehci_endpoint_create,
	.endpoint_register = ehci_register_ep,
	.endpoint_unregister = ehci_unregister_ep,
	.endpoint_set_toggle = ehci_ep_toggle_set,
	.endpoint_get_toggle = ehci_ep_toggle_get,
	.endpoint_count_bw = bandwidth_count_usb11,
	.batch_create = ehci_create_batch,
	.batch_destroy = ehci_destroy_batch,
};

int ehci_bus_init(ehci_bus_t *bus, hcd_t *hcd, hc_t *hc)
{
	assert(hc);
	assert(bus);

	usb2_bus_t *usb2_bus = (usb2_bus_t *) bus;
	bus_t *bus_base = (bus_t *) bus;

	usb2_bus_init(usb2_bus, hcd, BANDWIDTH_AVAILABLE_USB11);
	bus_base->ops = &ehci_bus_ops;

	bus->hc = hc;

	return EOK;
}

/**
 * @}
 */