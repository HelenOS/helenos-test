/*
 * Copyright (c) 2006 Josef Cejka
 * Copyright (c) 2011 Jiri Svoboda
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

/**
 * @addtogroup inputgen generic
 * @brief HelenOS input server.
 * @ingroup input
 * @{
 */
/** @file
 */

#include <adt/fifo.h>
#include <adt/list.h>
#include <async.h>
#include <config.h>
#include <errno.h>
#include <fibril.h>
#include <fibril_synch.h>
#include <io/chardev.h>
#include <io/console.h>
#include <io/keycode.h>
#include <ipc/services.h>
#include <ipc/input.h>
#include <loc.h>
#include <macros.h>
#include <ns.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <str.h>
#include <str_error.h>

#include "input.h"
#include "kbd.h"
#include "kbd_port.h"
#include "kbd_ctl.h"
#include "layout.h"
#include "mouse.h"
#include "mouse_proto.h"
#include "serial.h"

static layout_ops_t *layout_active = &us_qwerty_ops;

#define NUM_LAYOUTS  5

static layout_ops_t *layout[NUM_LAYOUTS] = {
	&us_qwerty_ops,
	&us_dvorak_ops,
	&cz_ops,
	&ar_ops,
	&fr_azerty_ops
};

typedef struct {
	/** Link into the list of clients */
	link_t link;

	/** Indicate whether the client is active */
	bool active;

	/** Client callback session */
	async_sess_t *sess;
} client_t;

/** List of clients */
static list_t clients;
static client_t *active_client = NULL;

/** Kernel override */
static bool active = true;

/** Serial console specified by the user */
static char *serial_console;

/** List of keyboard devices */
static list_t kbd_devs;

/** List of mouse devices */
static list_t mouse_devs;

/** List of serial devices */
static list_t serial_devs;

static FIBRIL_MUTEX_INITIALIZE(discovery_lock);

/* changes all kb devices to the given layout */
static void layout_change(layout_ops_t *layout)
{
	list_foreach(kbd_devs, link, kbd_dev_t, kdev) {
		layout_t *ret = layout_create(layout);
		if (ret != NULL) {
			layout_destroy(kdev->active_layout);
			kdev->active_layout = ret;
		}
	}
}

/* similiar like layout_change but takes as an argument the name of the layout */
static errno_t layout_load(const char *layout_name)
{
	/* TODO: change this into a loader for kb layout maps */
	for (int i = 0; i < NUM_LAYOUTS; i++) {
		if (str_cmp(layout[i]->name, layout_name) == 0) {
			layout_active = layout[i];
			layout_change(layout_active);
			return EOK;
		}
	}

	return ENOTSUP;
}

/* Handler for IPC call INPUT_SET_LAYOUT */
static void client_set_layout_srv(ipc_call_t *call)
{
	void *layout_name;
	errno_t ret = async_data_write_accept(&layout_name, true, 0, 0, 0, 0);
	if (ret != EOK) {
		async_answer_0(call, ret);
		return;
	}

	errno_t retval = layout_load((char *)layout_name);
	free(layout_name);
	async_answer_0(call, retval);
}

/* Handler for IPC call INPUT_GET_LAYOUT */
static void client_get_layout_srv(ipc_call_t *call)
{
	const char *layout_name = layout_active->name;
	size_t length = str_size(layout_name) + 1;
	ipc_call_t rec_call;

	size_t max_size;
	if (!async_data_read_receive(&rec_call, &max_size)) {
		async_answer_0(&rec_call, EREFUSED);
		async_answer_0(call, EREFUSED);
		return;
	}

	errno_t rc = async_data_read_finalize(&rec_call, layout_name,
	    min(length, max_size));
	async_answer_0(call, rc);
}

static void *client_data_create(void)
{
	client_t *client = (client_t *) calloc(1, sizeof(client_t));
	if (client == NULL)
		return NULL;

	link_initialize(&client->link);
	client->active = false;
	client->sess = NULL;

	list_append(&client->link, &clients);

	return client;
}

static void client_data_destroy(void *data)
{
	client_t *client = (client_t *) data;

	list_remove(&client->link);
	free(client);
}

void kbd_push_data(kbd_dev_t *kdev, sysarg_t data)
{
	(*kdev->ctl_ops->parse)(data);
}

void mouse_push_data(mouse_dev_t *mdev, sysarg_t data)
{
	(*mdev->proto_ops->parse)(data);
}

void kbd_push_event(kbd_dev_t *kdev, int type, unsigned int key)
{
	kbd_event_t ev;
	unsigned int mod_mask;

	switch (key) {
	case KC_LCTRL:
		mod_mask = KM_LCTRL;
		break;
	case KC_RCTRL:
		mod_mask = KM_RCTRL;
		break;
	case KC_LSHIFT:
		mod_mask = KM_LSHIFT;
		break;
	case KC_RSHIFT:
		mod_mask = KM_RSHIFT;
		break;
	case KC_LALT:
		mod_mask = KM_LALT;
		break;
	case KC_RALT:
		mod_mask = KM_RALT;
		break;
	default:
		mod_mask = 0;
	}

	if (mod_mask != 0) {
		if (type == KEY_PRESS)
			kdev->mods = kdev->mods | mod_mask;
		else
			kdev->mods = kdev->mods & ~mod_mask;
	}

	switch (key) {
	case KC_CAPS_LOCK:
		mod_mask = KM_CAPS_LOCK;
		break;
	case KC_NUM_LOCK:
		mod_mask = KM_NUM_LOCK;
		break;
	case KC_SCROLL_LOCK:
		mod_mask = KM_SCROLL_LOCK;
		break;
	default:
		mod_mask = 0;
	}

	if (mod_mask != 0) {
		if (type == KEY_PRESS) {
			/*
			 * Only change lock state on transition from released
			 * to pressed. This prevents autorepeat from messing
			 * up the lock state.
			 */
			kdev->mods = kdev->mods ^ (mod_mask & ~kdev->lock_keys);
			kdev->lock_keys = kdev->lock_keys | mod_mask;

			/* Update keyboard lock indicator lights. */
			(*kdev->ctl_ops->set_ind)(kdev, kdev->mods);
		} else {
			kdev->lock_keys = kdev->lock_keys & ~mod_mask;
		}
	}

	// TODO: More elegant layout switching

	if ((type == KEY_PRESS) && (kdev->mods & KM_LCTRL)) {
		switch (key) {
		case KC_F1:
			layout_change(layout[0]);
			break;
		case KC_F2:
			layout_change(layout[1]);
			break;
		case KC_F3:
			layout_change(layout[2]);
			break;
		case KC_F4:
			layout_change(layout[3]);
			break;
		case KC_F5:
			layout_change(layout[4]);
			break;
		default: // default: is here to avoid compiler warning about unhandled cases
			break;
		}
	}

	if (type == KEY_PRESS) {
		switch (key) {
		case KC_F12:
			console_kcon();
			break;
		}
	}

	ev.type = type;
	ev.key = key;
	ev.mods = kdev->mods;

	ev.c = layout_parse_ev(kdev->active_layout, &ev);

	list_foreach(clients, link, client_t, client) {
		if (client->active) {
			async_exch_t *exch = async_exchange_begin(client->sess);
			async_msg_4(exch, INPUT_EVENT_KEY, ev.type, ev.key, ev.mods, ev.c);
			async_exchange_end(exch);
		}
	}
}

/** Mouse pointer has moved (relative mode). */
void mouse_push_event_move(mouse_dev_t *mdev, int dx, int dy, int dz)
{
	list_foreach(clients, link, client_t, client) {
		if (client->active) {
			async_exch_t *exch = async_exchange_begin(client->sess);

			if ((dx) || (dy))
				async_msg_2(exch, INPUT_EVENT_MOVE, dx, dy);

			if (dz) {
				// TODO: Implement proper wheel support
				keycode_t code = dz > 0 ? KC_UP : KC_DOWN;

				for (unsigned int i = 0; i < 3; i++)
					async_msg_4(exch, INPUT_EVENT_KEY, KEY_PRESS, code, 0, 0);

				async_msg_4(exch, INPUT_EVENT_KEY, KEY_RELEASE, code, 0, 0);
			}

			async_exchange_end(exch);
		}
	}
}

/** Mouse pointer has moved (absolute mode). */
void mouse_push_event_abs_move(mouse_dev_t *mdev, unsigned int x, unsigned int y,
    unsigned int max_x, unsigned int max_y)
{
	list_foreach(clients, link, client_t, client) {
		if (client->active) {
			if ((max_x) && (max_y)) {
				async_exch_t *exch = async_exchange_begin(client->sess);
				async_msg_4(exch, INPUT_EVENT_ABS_MOVE, x, y, max_x, max_y);
				async_exchange_end(exch);
			}
		}
	}
}

/** Mouse button has been pressed. */
void mouse_push_event_button(mouse_dev_t *mdev, int bnum, int press)
{
	list_foreach(clients, link, client_t, client) {
		if (client->active) {
			async_exch_t *exch = async_exchange_begin(client->sess);
			async_msg_2(exch, INPUT_EVENT_BUTTON, bnum, press);
			async_exchange_end(exch);
		}
	}
}

/** Arbitrate client actiovation */
static void client_arbitration(void)
{
	/* Mutual exclusion of active clients */
	list_foreach(clients, link, client_t, client)
		client->active = ((active) && (client == active_client));

	/* Notify clients about the arbitration */
	list_foreach(clients, link, client_t, client) {
		async_exch_t *exch = async_exchange_begin(client->sess);
		async_msg_0(exch, client->active ?
		    INPUT_EVENT_ACTIVE : INPUT_EVENT_DEACTIVE);
		async_exchange_end(exch);
	}
}

/** New client connection */
static void client_connection(ipc_call_t *icall, void *arg)
{
	client_t *client = (client_t *) async_get_client_data();
	if (client == NULL) {
		async_answer_0(icall, ENOMEM);
		return;
	}

	async_accept_0(icall);

	while (true) {
		ipc_call_t call;
		async_get_call(&call);

		if (!ipc_get_imethod(&call)) {
			if (client->sess != NULL) {
				async_hangup(client->sess);
				client->sess = NULL;
			}

			async_answer_0(&call, EOK);
			return;
		}

		async_sess_t *sess =
		    async_callback_receive_start(EXCHANGE_SERIALIZE, &call);
		if (sess != NULL) {
			if (client->sess == NULL) {
				client->sess = sess;
				async_answer_0(&call, EOK);
			} else
				async_answer_0(&call, ELIMIT);
		} else {
			switch (ipc_get_imethod(&call)) {
			case INPUT_ACTIVATE:
				active_client = client;
				client_arbitration();
				async_answer_0(&call, EOK);
				break;
			case INPUT_SET_LAYOUT:
				client_set_layout_srv(&call);
				break;
			case INPUT_GET_LAYOUT:
				client_get_layout_srv(&call);
				break;
			default:
				async_answer_0(&call, EINVAL);
			}
		}
	}
}

static void kconsole_event_handler(ipc_call_t *call, void *arg)
{
	if (ipc_get_arg1(call)) {
		/* Kernel console activated */
		active = false;
	} else {
		/* Kernel console deactivated */
		active = true;
	}

	client_arbitration();
}

static kbd_dev_t *kbd_dev_new(void)
{
	kbd_dev_t *kdev = calloc(1, sizeof(kbd_dev_t));
	if (kdev == NULL) {
		printf("%s: Error allocating keyboard device. "
		    "Out of memory.\n", NAME);
		return NULL;
	}

	link_initialize(&kdev->link);

	kdev->mods = KM_NUM_LOCK;
	kdev->lock_keys = 0;
	kdev->active_layout = layout_create(layout_active);

	return kdev;
}

static mouse_dev_t *mouse_dev_new(void)
{
	mouse_dev_t *mdev = calloc(1, sizeof(mouse_dev_t));
	if (mdev == NULL) {
		printf("%s: Error allocating mouse device. "
		    "Out of memory.\n", NAME);
		return NULL;
	}

	link_initialize(&mdev->link);

	return mdev;
}

static serial_dev_t *serial_dev_new(void)
{
	serial_dev_t *sdev = calloc(1, sizeof(serial_dev_t));
	if (sdev == NULL) {
		printf("%s: Error allocating serial device. "
		    "Out of memory.\n", NAME);
		return NULL;
	}

	sdev->kdev = kbd_dev_new();
	if (sdev->kdev == NULL) {
		free(sdev);
		return NULL;
	}

	link_initialize(&sdev->link);

	return sdev;
}

/** Add new legacy keyboard device. */
static void kbd_add_dev(kbd_port_ops_t *port, kbd_ctl_ops_t *ctl)
{
	kbd_dev_t *kdev = kbd_dev_new();
	if (kdev == NULL)
		return;

	kdev->port_ops = port;
	kdev->ctl_ops = ctl;
	kdev->svc_id = 0;

	/* Initialize port driver. */
	if ((*kdev->port_ops->init)(kdev) != 0)
		goto fail;

	/* Initialize controller driver. */
	if ((*kdev->ctl_ops->init)(kdev) != 0) {
		/* XXX Uninit port */
		goto fail;
	}

	list_append(&kdev->link, &kbd_devs);
	return;

fail:
	free(kdev);
}

/** Add new kbdev device.
 *
 * @param service_id Service ID of the keyboard device
 *
 */
static int kbd_add_kbdev(service_id_t service_id, kbd_dev_t **kdevp)
{
	kbd_dev_t *kdev = kbd_dev_new();
	if (kdev == NULL)
		return -1;

	kdev->svc_id = service_id;
	kdev->port_ops = NULL;
	kdev->ctl_ops = &kbdev_ctl;

	errno_t rc = loc_service_get_name(service_id, &kdev->svc_name);
	if (rc != EOK) {
		kdev->svc_name = NULL;
		goto fail;
	}

	/* Initialize controller driver. */
	if ((*kdev->ctl_ops->init)(kdev) != 0) {
		goto fail;
	}

	list_append(&kdev->link, &kbd_devs);
	*kdevp = kdev;
	return 0;

fail:
	if (kdev->svc_name != NULL)
		free(kdev->svc_name);
	free(kdev);
	return -1;
}

/** Add new mousedev device.
 *
 * @param service_id Service ID of the mouse device
 *
 */
static int mouse_add_mousedev(service_id_t service_id, mouse_dev_t **mdevp)
{
	mouse_dev_t *mdev = mouse_dev_new();
	if (mdev == NULL)
		return -1;

	mdev->svc_id = service_id;
	mdev->port_ops = NULL;
	mdev->proto_ops = &mousedev_proto;

	errno_t rc = loc_service_get_name(service_id, &mdev->svc_name);
	if (rc != EOK) {
		mdev->svc_name = NULL;
		goto fail;
	}

	/* Initialize controller driver. */
	if ((*mdev->proto_ops->init)(mdev) != 0) {
		goto fail;
	}

	list_append(&mdev->link, &mouse_devs);
	*mdevp = mdev;
	return 0;

fail:
	free(mdev);
	return -1;
}

static errno_t serial_consumer(void *arg)
{
	serial_dev_t *sdev = (serial_dev_t *) arg;

	while (true) {
		uint8_t data;
		size_t nread;

		chardev_read(sdev->chardev, &data, sizeof(data), &nread,
		    chardev_f_none);
		/* XXX Handle error */
		kbd_push_data(sdev->kdev, data);
	}

	return EOK;
}

/** Add new serial console device.
 *
 * @param service_id Service ID of the chardev device
 *
 */
static int serial_add_srldev(service_id_t service_id, serial_dev_t **sdevp)
{
	bool match = false;
	errno_t rc;

	serial_dev_t *sdev = serial_dev_new();
	if (sdev == NULL)
		return -1;

	sdev->kdev->svc_id = service_id;

	rc = loc_service_get_name(service_id, &sdev->kdev->svc_name);
	if (rc != EOK)
		goto fail;

	list_append(&sdev->link, &serial_devs);

	/*
	 * Is this the device the user wants to use as a serial console?
	 */
	match = (serial_console != NULL) &&
	    !str_cmp(serial_console, sdev->kdev->svc_name);

	if (match) {
		sdev->kdev->ctl_ops = &stty_ctl;

		/* Initialize controller driver. */
		if ((*sdev->kdev->ctl_ops->init)(sdev->kdev) != 0) {
			list_remove(&sdev->link);
			goto fail;
		}

		sdev->sess = loc_service_connect(service_id, INTERFACE_DDF,
		    IPC_FLAG_BLOCKING);

		rc = chardev_open(sdev->sess, &sdev->chardev);
		if (rc != EOK) {
			async_hangup(sdev->sess);
			sdev->sess = NULL;
			list_remove(&sdev->link);
			goto fail;
		}

		fid_t fid = fibril_create(serial_consumer, sdev);
		fibril_add_ready(fid);
	}

	*sdevp = sdev;
	return 0;

fail:
	if (sdev->kdev->svc_name != NULL)
		free(sdev->kdev->svc_name);
	free(sdev->kdev);
	free(sdev);
	return -1;
}

/** Add legacy drivers/devices. */
static void kbd_add_legacy_devs(void)
{
	/*
	 * Need to add these drivers based on config unless we can probe
	 * them automatically.
	 */
#if defined(UARCH_arm32) && defined(MACHINE_gta02)
	kbd_add_dev(&chardev_port, &stty_ctl);
#endif
#if defined(UARCH_ia64) && defined(MACHINE_ski)
	kbd_add_dev(&chardev_port, &stty_ctl);
#endif
#if defined(MACHINE_msim)
	kbd_add_dev(&chardev_port, &stty_ctl);
#endif
#if defined(UARCH_sparc64) && defined(PROCESSOR_sun4v)
	kbd_add_dev(&chardev_port, &stty_ctl);
#endif
#if defined(UARCH_arm64) && defined(MACHINE_virt)
	kbd_add_dev(&chardev_port, &stty_ctl);
#endif
	/* Silence warning on abs32le about kbd_add_dev() being unused */
	(void) kbd_add_dev;
}

static errno_t dev_check_new_kbdevs(void)
{
	category_id_t keyboard_cat;
	service_id_t *svcs;
	size_t count, i;
	bool already_known;
	errno_t rc;

	rc = loc_category_get_id("keyboard", &keyboard_cat, IPC_FLAG_BLOCKING);
	if (rc != EOK) {
		printf("%s: Failed resolving category 'keyboard'.\n", NAME);
		return ENOENT;
	}

	/*
	 * Check for new keyboard devices
	 */
	rc = loc_category_get_svcs(keyboard_cat, &svcs, &count);
	if (rc != EOK) {
		printf("%s: Failed getting list of keyboard devices.\n",
		    NAME);
		return EIO;
	}

	for (i = 0; i < count; i++) {
		already_known = false;

		/* Determine whether we already know this device. */
		list_foreach(kbd_devs, link, kbd_dev_t, kdev) {
			if (kdev->svc_id == svcs[i]) {
				already_known = true;
				break;
			}
		}

		if (!already_known) {
			kbd_dev_t *kdev;
			if (kbd_add_kbdev(svcs[i], &kdev) == 0) {
				printf("%s: Connected keyboard device '%s'\n",
				    NAME, kdev->svc_name);
			}
		}
	}

	free(svcs);

	/* XXX Handle device removal */

	return EOK;
}

static errno_t dev_check_new_mousedevs(void)
{
	category_id_t mouse_cat;
	service_id_t *svcs;
	size_t count, i;
	bool already_known;
	errno_t rc;

	rc = loc_category_get_id("mouse", &mouse_cat, IPC_FLAG_BLOCKING);
	if (rc != EOK) {
		printf("%s: Failed resolving category 'mouse'.\n", NAME);
		return ENOENT;
	}

	/*
	 * Check for new mouse devices
	 */
	rc = loc_category_get_svcs(mouse_cat, &svcs, &count);
	if (rc != EOK) {
		printf("%s: Failed getting list of mouse devices.\n",
		    NAME);
		return EIO;
	}

	for (i = 0; i < count; i++) {
		already_known = false;

		/* Determine whether we already know this device. */
		list_foreach(mouse_devs, link, mouse_dev_t, mdev) {
			if (mdev->svc_id == svcs[i]) {
				already_known = true;
				break;
			}
		}

		if (!already_known) {
			mouse_dev_t *mdev;
			if (mouse_add_mousedev(svcs[i], &mdev) == 0) {
				printf("%s: Connected mouse device '%s'\n",
				    NAME, mdev->svc_name);
			}
		}
	}

	free(svcs);

	/* XXX Handle device removal */

	return EOK;
}

static errno_t dev_check_new_serialdevs(void)
{
	category_id_t serial_cat;
	service_id_t *svcs;
	size_t count, i;
	bool already_known;
	errno_t rc;

	rc = loc_category_get_id("serial", &serial_cat, IPC_FLAG_BLOCKING);
	if (rc != EOK) {
		printf("%s: Failed resolving category 'serial'.\n", NAME);
		return ENOENT;
	}

	/*
	 * Check for new serial devices
	 */
	rc = loc_category_get_svcs(serial_cat, &svcs, &count);
	if (rc != EOK) {
		printf("%s: Failed getting list of serial devices.\n",
		    NAME);
		return EIO;
	}

	for (i = 0; i < count; i++) {
		already_known = false;

		/* Determine whether we already know this device. */
		list_foreach(serial_devs, link, serial_dev_t, sdev) {
			if (sdev->kdev->svc_id == svcs[i]) {
				already_known = true;
				break;
			}
		}

		if (!already_known) {
			serial_dev_t *sdev;
			if (serial_add_srldev(svcs[i], &sdev) == 0) {
				printf("%s: Connected serial device '%s'\n",
				    NAME, sdev->kdev->svc_name);
			}
		}
	}

	free(svcs);

	/* XXX Handle device removal */

	return EOK;
}

static errno_t dev_check_new(void)
{
	errno_t rc;

	fibril_mutex_lock(&discovery_lock);

	if (!serial_console) {
		rc = dev_check_new_kbdevs();
		if (rc != EOK) {
			fibril_mutex_unlock(&discovery_lock);
			return rc;
		}

		rc = dev_check_new_mousedevs();
		if (rc != EOK) {
			fibril_mutex_unlock(&discovery_lock);
			return rc;
		}
	} else {
		rc = dev_check_new_serialdevs();
		if (rc != EOK) {
			fibril_mutex_unlock(&discovery_lock);
			return rc;
		}
	}

	fibril_mutex_unlock(&discovery_lock);

	return EOK;
}

static void cat_change_cb(void *arg)
{
	dev_check_new();
}

/** Start listening for new devices. */
static errno_t input_start_dev_discovery(void)
{
	errno_t rc = loc_register_cat_change_cb(cat_change_cb, NULL);
	if (rc != EOK) {
		printf("%s: Failed registering callback for device discovery: "
		    "%s\n", NAME, str_error(rc));
		return rc;
	}

	return dev_check_new();
}

static void usage(char *name)
{
	printf("Usage: %s <service_name>\n", name);
}

int main(int argc, char **argv)
{
	errno_t rc;

	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	printf("%s: HelenOS input service\n", NAME);

	list_initialize(&clients);
	list_initialize(&kbd_devs);
	list_initialize(&mouse_devs);
	list_initialize(&serial_devs);

	serial_console = config_get_value("console");

	/* Add legacy keyboard devices. */
	kbd_add_legacy_devs();

	/* Register driver */
	async_set_client_data_constructor(client_data_create);
	async_set_client_data_destructor(client_data_destroy);
	async_set_fallback_port_handler(client_connection, NULL);

	rc = loc_server_register(NAME);
	if (rc != EOK) {
		printf("%s: Unable to register server\n", NAME);
		return rc;
	}

	service_id_t service_id;
	rc = loc_service_register(argv[1], &service_id);
	if (rc != EOK) {
		printf("%s: Unable to register service %s\n", NAME, argv[1]);
		return rc;
	}

	/* Receive kernel notifications */
	rc = async_event_subscribe(EVENT_KCONSOLE, kconsole_event_handler, NULL);
	if (rc != EOK)
		printf("%s: Failed to register kconsole notifications (%s)\n",
		    NAME, str_error(rc));

	/* Start looking for new input devices */
	input_start_dev_discovery();

	printf("%s: Accepting connections\n", NAME);
	task_retval(0);
	async_manager();

	/* Not reached. */
	return 0;
}

/**
 * @}
 */
