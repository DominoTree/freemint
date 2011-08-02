/*
 * Modified for the FreeMiNT USB subsystem by David Galvez. 2010 - 2011
 *
 * XaAES - XaAES Ain't the AES (c) 1992 - 1998 C.Graham
 *                                 1999 - 2003 H.Robbers
 *                                        2004 F.Naumann & O.Skancke
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with XaAES; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "global.h"
#include "util.h"
#include "init.h"
#include "usb.h"
#include "hub.h"
#include "ucdload.h"
#include "uddload.h"
#include "udd.h"
#include "udd/udd_defs.h"

long loader_pid = 0;
long loader_pgrp = 0;
short usb_found = 0;

struct usb_module_api usb_api;

//static char usb_display_name[32];

void	setup_usb_module_api(void);

static void
bootmessage(void)
{
	c_conws("USB driver for MiNT\n\r");
	c_conws("EXPERIMENTAL!!!!!!!\n\r");
	c_conws("David Galvez. 2010\n\r");
}

struct kentry *kentry;
#if 0
static bool
sysfile_exists(const char *sd, char *fn)
{
	struct file *check;
	bool flag = false;
	int o = 0;
	char *buf, *tmp;

	buf = kmalloc(strlen(sd)+16);
	if (buf)
	{
		if (sd[0] == '/' || sd[0] == '\\')
		{
			buf[0] = 'u';
			buf[1] = ':';
			o += 2;
		}
		strcpy(buf + o, sd);
		tmp = buf + strlen(buf) - 1;
		if (*tmp != '/' && *tmp != '\\')
		{
			tmp[1] = '\\';
			tmp[2] = '\0';
		}
		strcat(buf + o, fn);
		display("sysfile_exits: '%s'", buf);
		check = kernel_open(buf, O_RDONLY, NULL, NULL);
		if (check)
		{
			kernel_close(check);
			flag = true;
		}
		kfree(buf);
	}
	return flag;
}
#endif

/*
 * Module initialisation
 * - setup internal data
 * - start main kernel thread
 */

Path start_path;		/* The directory that the started binary lives */
static const struct kernel_module *self = NULL;

void usb_storage_init(void);	/* Prototype, this shouldn't be here */

void
setup_usb_module_api(void)
{
	usb_api.udd_register = &udd_register;
	usb_api.udd_unregister = &udd_unregister;
//	usb_api.fname = &fname;


//	usb_api.usb_init = &usb_init;
//	usb_api.usb_stop = &usb_stop;


	usb_api.usb_set_protocol = &usb_set_protocol;
	usb_api.usb_set_idle = &usb_set_idle;
	usb_api.usb_get_dev_index = &usb_get_dev_index;
	usb_api.usb_control_msg = &usb_control_msg;
	usb_api.usb_bulk_msg = &usb_bulk_msg;
	usb_api.usb_submit_int_msg = &usb_submit_int_msg;
	usb_api.usb_disable_asynch = &usb_disable_asynch;
	usb_api.usb_maxpacket = &usb_maxpacket;
	usb_api.usb_get_configuration_no = &usb_get_configuration_no;
	usb_api.usb_get_report = &usb_get_report;
	usb_api.usb_get_class_descriptor = &usb_get_class_descriptor;
	usb_api.usb_clear_halt = &usb_clear_halt;
	usb_api.usb_string = &usb_string;
	usb_api.usb_set_interface = &usb_set_interface;
	usb_api.usb_parse_config = &usb_parse_config;
	usb_api.usb_set_maxpacket = &usb_set_maxpacket;
	usb_api.usb_get_descriptor = &usb_get_descriptor;
	usb_api.usb_set_address = &usb_set_address;
	usb_api.usb_set_configuration = &usb_set_configuration;
	usb_api.usb_get_string = &usb_get_string;
	usb_api.usb_alloc_new_device = &usb_alloc_new_device;
	usb_api.usb_new_device = &usb_new_device;
	
	/* For now we leave this hub specific
	 * stuff out of the api.
	 */
//	usb_api.usb_get_hub_descriptor = &usb_get_hub_descriptor;
//	usb_api.usb_clear_hub_feature = &usb_clear_hub_feature;
//	usb_api.usb_clear_port_feature = &usb_clear_port_feature;
//	usb_api.usb_get_hub_status = &usb_get_hub_status;
//	usb_api.usb_set_port_feature = &usb_set_port_feature;
//	usb_api.usb_get_port_status = &usb_get_port_status;
//	usb_api.usb_hub_allocate = &usb_hub_allocate;
//	usb_api.usb_hub_port_connect_change = &usb_hub_port_connect_change;
//	usb_api.usb_hub_configure = &usb_hub_configure;	
}

long
init(struct kentry *k, const struct kernel_module *km) //const char *path)
{
	long err = 0L;
	bool first = true;
//	struct proc *rootproc;

	/* setup kernel entry */
	kentry = k;
	self = km;

	get_drive_and_path(start_path, sizeof(start_path));

	if (check_kentry_version())
	{
		err = ENOSYS;
		goto error;
	}

	/* remember loader */

	loader_pid = p_getpid();
	loader_pgrp = p_getpgrp();

	/* do some sanity checks of the installation
	 * that are a common source of user problems
	 */
	if (first)
	{
		
	}

	if (first)
		bootmessage();

	usb_stop();

	setup_usb_module_api();

	usb_main(0);

	usb_hub_init();

error:
	return err;
}
