/*
 * Copyright (c) 2006 Jakub Jermar
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

/** @addtogroup libcipc
 * @{
 */
/**
 * @file  services.h
 * @brief List of all known services and their codes.
 */

#ifndef _LIBC_SERVICES_H_
#define _LIBC_SERVICES_H_

#include <abi/fourcc.h>

typedef enum {
	SERVICE_NONE       = 0,
	SERVICE_LOADER     = FOURCC('l', 'o', 'a', 'd'),
	SERVICE_VFS        = FOURCC('v', 'f', 's', ' '),
	SERVICE_LOC        = FOURCC('l', 'o', 'c', ' '),
	SERVICE_LOGGER     = FOURCC('l', 'o', 'g', 'g'),
	SERVICE_DEVMAN     = FOURCC('d', 'e', 'v', 'n'),
} service_t;

#define SERVICE_NAME_CHARDEV_TEST_SMALLX "chardev-test/smallx"
#define SERVICE_NAME_CHARDEV_TEST_LARGEX "chardev-test/largex"
#define SERVICE_NAME_CHARDEV_TEST_PARTIALX "chardev-test/partialx"
#define SERVICE_NAME_CLIPBOARD "clipboard"
#define SERVICE_NAME_CORECFG  "corecfg"
#define SERVICE_NAME_DISPLAY  "hid/display"
#define SERVICE_NAME_DHCP     "net/dhcp"
#define SERVICE_NAME_DNSR     "net/dnsr"
#define SERVICE_NAME_INET     "net/inet"
#define SERVICE_NAME_IPC_TEST "ipc-test"
#define SERVICE_NAME_HID_INPUT    "hid/input"
#define SERVICE_NAME_NETCONF  "net/netconf"
#define SERVICE_NAME_UDP      "net/udp"
#define SERVICE_NAME_TCP      "net/tcp"
#define SERVICE_NAME_VBD      "vbd"
#define SERVICE_NAME_VOLSRV   "volsrv"

#endif

/** @}
 */
