/*
 * Copyright (c) 2012 Jiri Svoboda
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

/** @addtogroup ethip
 * @{
 */
/**
 * @file
 * @brief IP link provider for Ethernet
 *
 * Based on the IETF RFC 894 standard.
 */

#include <async.h>
#include <errno.h>
#include <inet/iplink_srv.h>
#include <io/log.h>
#include <loc.h>
#include <stdio.h>
#include <stdlib.h>
#include <task.h>
#include "arp.h"
#include "ethip.h"
#include "ethip_nic.h"
#include "pdu.h"
#include "std.h"

#define NAME "ethip"

static errno_t ethip_open(iplink_srv_t *srv);
static errno_t ethip_close(iplink_srv_t *srv);
static errno_t ethip_send(iplink_srv_t *srv, iplink_sdu_t *sdu);
static errno_t ethip_send6(iplink_srv_t *srv, iplink_sdu6_t *sdu);
static errno_t ethip_get_mtu(iplink_srv_t *srv, size_t *mtu);
static errno_t ethip_get_mac48(iplink_srv_t *srv, addr48_t *mac);
static errno_t ethip_set_mac48(iplink_srv_t *srv, addr48_t *mac);
static errno_t ethip_addr_add(iplink_srv_t *srv, inet_addr_t *addr);
static errno_t ethip_addr_remove(iplink_srv_t *srv, inet_addr_t *addr);

static void ethip_client_conn(ipc_call_t *icall, void *arg);

static iplink_ops_t ethip_iplink_ops = {
	.open = ethip_open,
	.close = ethip_close,
	.send = ethip_send,
	.send6 = ethip_send6,
	.get_mtu = ethip_get_mtu,
	.get_mac48 = ethip_get_mac48,
	.set_mac48 = ethip_set_mac48,
	.addr_add = ethip_addr_add,
	.addr_remove = ethip_addr_remove
};

static errno_t ethip_init(void)
{
	async_set_fallback_port_handler(ethip_client_conn, NULL);

	errno_t rc = loc_server_register(NAME);
	if (rc != EOK) {
		log_msg(LOG_DEFAULT, LVL_ERROR, "Failed registering server.");
		return rc;
	}

	rc = ethip_nic_discovery_start();
	if (rc != EOK)
		return rc;

	return EOK;
}

errno_t ethip_iplink_init(ethip_nic_t *nic)
{
	errno_t rc;
	service_id_t sid;
	category_id_t iplink_cat;
	static unsigned link_num = 0;
	char *svc_name = NULL;

	log_msg(LOG_DEFAULT, LVL_DEBUG, "ethip_iplink_init()");

	iplink_srv_init(&nic->iplink);
	nic->iplink.ops = &ethip_iplink_ops;
	nic->iplink.arg = nic;

	if (asprintf(&svc_name, "net/eth%u", ++link_num) < 0) {
		log_msg(LOG_DEFAULT, LVL_ERROR, "Out of memory.");
		rc = ENOMEM;
		goto error;
	}

	rc = loc_service_register(svc_name, &sid);
	if (rc != EOK) {
		log_msg(LOG_DEFAULT, LVL_ERROR, "Failed registering service %s.", svc_name);
		goto error;
	}

	nic->iplink_sid = sid;

	rc = loc_category_get_id("iplink", &iplink_cat, IPC_FLAG_BLOCKING);
	if (rc != EOK) {
		log_msg(LOG_DEFAULT, LVL_ERROR, "Failed resolving category 'iplink'.");
		goto error;
	}

	rc = loc_service_add_to_cat(sid, iplink_cat);
	if (rc != EOK) {
		log_msg(LOG_DEFAULT, LVL_ERROR, "Failed adding %s to category.", svc_name);
		goto error;
	}

	return EOK;

error:
	if (svc_name != NULL)
		free(svc_name);
	return rc;
}

static void ethip_client_conn(ipc_call_t *icall, void *arg)
{
	ethip_nic_t *nic;
	service_id_t sid;

	sid = (service_id_t) ipc_get_arg2(icall);
	log_msg(LOG_DEFAULT, LVL_DEBUG, "ethip_client_conn(%u)", (unsigned)sid);
	nic = ethip_nic_find_by_iplink_sid(sid);
	if (nic == NULL) {
		log_msg(LOG_DEFAULT, LVL_WARN, "Uknown service ID.");
		return;
	}

	iplink_conn(icall, &nic->iplink);
}

static errno_t ethip_open(iplink_srv_t *srv)
{
	log_msg(LOG_DEFAULT, LVL_DEBUG, "ethip_open()");
	return EOK;
}

static errno_t ethip_close(iplink_srv_t *srv)
{
	log_msg(LOG_DEFAULT, LVL_DEBUG, "ethip_close()");
	return EOK;
}

static errno_t ethip_send(iplink_srv_t *srv, iplink_sdu_t *sdu)
{
	log_msg(LOG_DEFAULT, LVL_DEBUG, "ethip_send()");

	ethip_nic_t *nic = (ethip_nic_t *) srv->arg;
	eth_frame_t frame = { 0 };

	errno_t rc = arp_translate(nic, sdu->src, sdu->dest, frame.dest);
	if (rc != EOK) {
		log_msg(LOG_DEFAULT, LVL_WARN, "Failed to look up IPv4 address 0x%"
		    PRIx32, sdu->dest);
		return rc;
	}

	addr48(nic->mac_addr, frame.src);
	frame.etype_len = ETYPE_IP;
	frame.data = sdu->data;
	frame.size = sdu->size;

	void *data;
	size_t size;
	rc = eth_pdu_encode(&frame, &data, &size);
	if (rc != EOK)
		return rc;

	rc = ethip_nic_send(nic, data, size);
	free(data);

	return rc;
}

static errno_t ethip_send6(iplink_srv_t *srv, iplink_sdu6_t *sdu)
{
	log_msg(LOG_DEFAULT, LVL_DEBUG, "ethip_send6()");

	ethip_nic_t *nic = (ethip_nic_t *) srv->arg;
	eth_frame_t frame;

	addr48(sdu->dest, frame.dest);
	addr48(nic->mac_addr, frame.src);
	frame.etype_len = ETYPE_IPV6;
	frame.data = sdu->data;
	frame.size = sdu->size;

	void *data;
	size_t size;
	errno_t rc = eth_pdu_encode(&frame, &data, &size);
	if (rc != EOK)
		return rc;

	rc = ethip_nic_send(nic, data, size);
	free(data);

	return rc;
}

errno_t ethip_received(iplink_srv_t *srv, void *data, size_t size)
{
	log_msg(LOG_DEFAULT, LVL_DEBUG, "ethip_received(): srv=%p", srv);
	ethip_nic_t *nic = (ethip_nic_t *) srv->arg;

	log_msg(LOG_DEFAULT, LVL_DEBUG, " - eth_pdu_decode");

	eth_frame_t frame;
	errno_t rc = eth_pdu_decode(data, size, &frame);
	if (rc != EOK) {
		log_msg(LOG_DEFAULT, LVL_DEBUG, " - eth_pdu_decode failed");
		return rc;
	}

	iplink_recv_sdu_t sdu;

	switch (frame.etype_len) {
	case ETYPE_ARP:
		arp_received(nic, &frame);
		break;
	case ETYPE_IP:
		log_msg(LOG_DEFAULT, LVL_DEBUG, " - construct SDU");
		sdu.data = frame.data;
		sdu.size = frame.size;
		log_msg(LOG_DEFAULT, LVL_DEBUG, " - call iplink_ev_recv");
		rc = iplink_ev_recv(&nic->iplink, &sdu, ip_v4);
		break;
	case ETYPE_IPV6:
		log_msg(LOG_DEFAULT, LVL_DEBUG, " - construct SDU IPv6");
		sdu.data = frame.data;
		sdu.size = frame.size;
		log_msg(LOG_DEFAULT, LVL_DEBUG, " - call iplink_ev_recv");
		rc = iplink_ev_recv(&nic->iplink, &sdu, ip_v6);
		break;
	default:
		log_msg(LOG_DEFAULT, LVL_DEBUG, "Unknown ethertype 0x%" PRIx16,
		    frame.etype_len);
	}

	free(frame.data);
	return rc;
}

static errno_t ethip_get_mtu(iplink_srv_t *srv, size_t *mtu)
{
	log_msg(LOG_DEFAULT, LVL_DEBUG, "ethip_get_mtu()");
	*mtu = 1500;
	return EOK;
}

static errno_t ethip_get_mac48(iplink_srv_t *srv, addr48_t *mac)
{
	log_msg(LOG_DEFAULT, LVL_DEBUG, "ethip_get_mac48()");

	ethip_nic_t *nic = (ethip_nic_t *) srv->arg;
	addr48(nic->mac_addr, *mac);

	return EOK;
}

static errno_t ethip_set_mac48(iplink_srv_t *srv, addr48_t *mac)
{
	log_msg(LOG_DEFAULT, LVL_DEBUG, "ethip_set_mac48()");

	ethip_nic_t *nic = (ethip_nic_t *) srv->arg;
	addr48(*mac, nic->mac_addr);

	return EOK;
}

static errno_t ethip_addr_add(iplink_srv_t *srv, inet_addr_t *addr)
{
	ethip_nic_t *nic = (ethip_nic_t *) srv->arg;

	return ethip_nic_addr_add(nic, addr);
}

static errno_t ethip_addr_remove(iplink_srv_t *srv, inet_addr_t *addr)
{
	ethip_nic_t *nic = (ethip_nic_t *) srv->arg;

	return ethip_nic_addr_remove(nic, addr);
}

int main(int argc, char *argv[])
{
	errno_t rc;

	printf(NAME ": HelenOS IP over Ethernet service\n");

	if (log_init(NAME) != EOK) {
		printf(NAME ": Failed to initialize logging.\n");
		return 1;
	}

	rc = ethip_init();
	if (rc != EOK)
		return 1;

	printf(NAME ": Accepting connections.\n");
	task_retval(0);
	async_manager();

	/* Not reached */
	return 0;
}

/** @}
 */
