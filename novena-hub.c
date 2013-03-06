/*
* Copyright (C) 2006 Free Software Initiative of Japan
*
* Author: NIIBE Yutaka  <gniibe at fsij.org>
* with changes by rss 14.03.2010
*
* This file can be distributed under the terms and conditions of the
* GNU General Public License version 2 (or later).
*
*/

#include <errno.h>
#include <libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>

struct state {
	struct libusb_context *ctx;
	char *progname;
};


#define USB_RT_HUB			(LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_DEVICE)
#define USB_RT_PORT			(LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_OTHER)
#define USB_PORT_FEAT_POWER		8
#define USB_PORT_FEAT_INDICATOR         22

#define COMMAND_SET_NONE  0
#define COMMAND_SET_LED   1
#define COMMAND_SET_POWER 2
#define HUB_LED_GREEN 2


#define HUB_CHAR_LPSM		0x0003
#define HUB_CHAR_PORTIND        0x0080

struct usb_hub_descriptor {
	unsigned char bDescLength;
	unsigned char bDescriptorType;
	unsigned char bNbrPorts;
	unsigned char wHubCharacteristics[2];
	unsigned char bPwrOn2PwrGood;
	unsigned char bHubContrCurrent;
	unsigned char data[0];
};

#define CTRL_TIMEOUT 1000
#define USB_STATUS_SIZE 4

#define MAX_HUBS 128
struct hub_info {
	int busnum, devnum;
	struct libusb_device *dev;
	int nport;
	int indicator_support;
};

static void hub_port_status (libusb_device_handle *uh, int nport) {
	int i;

	printf(" Hub Port Status: (%d)\n", nport);
	for (i = 0; i < nport; i++)
	{
		uint8_t buf[USB_STATUS_SIZE];
		int ret;

		ret = libusb_control_transfer (uh,
			LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_OTHER,
			LIBUSB_REQUEST_GET_STATUS, 
			0, i + 1,
			buf, USB_STATUS_SIZE,
			CTRL_TIMEOUT);
		if (ret < 0)
		{
			fprintf (stderr,
				"Error %d, cannot read port %d status, %s (%d)\n",
				ret, i + 1, strerror(errno), errno);
			continue;
		}

		printf("   Port %d: %02x%02x.%02x%02x", i + 1,
			buf[3], buf [2],
			buf[1], buf [0]);

		printf("%s%s%s%s%s",
			(buf[2] & 0x10) ? " C_RESET" : "",
			(buf[2] & 0x08) ? " C_OC" : "",
			(buf[2] & 0x04) ? " C_SUSPEND" : "",
			(buf[2] & 0x02) ? " C_ENABLE" : "",
			(buf[2] & 0x01) ? " C_CONNECT" : "");

		printf("%s%s%s%s%s%s%s%s%s%s\n",
			(buf[1] & 0x10) ? " indicator" : "",
			(buf[1] & 0x08) ? " test" : "",
			(buf[1] & 0x04) ? " highspeed" : "",
			(buf[1] & 0x02) ? " lowspeed" : "",
			(buf[1] & 0x01) ? " power" : "",
			(buf[0] & 0x10) ? " RESET" : "",
			(buf[0] & 0x08) ? " oc" : "",
			(buf[0] & 0x04) ? " suspend" : "",
			(buf[0] & 0x02) ? " enable" : "",
			(buf[0] & 0x01) ? " connect" : "");
	}
}

static int set_port_power(struct state *st,
				struct libusb_device *hub_dev,
				int port,
				int enabled) {
	int feature = USB_PORT_FEAT_POWER;
	int request;
	int index;

	int ret;
	struct libusb_device_handle *hub;

	if (enabled) {
		request = LIBUSB_REQUEST_SET_FEATURE;
		index = port;
	}
	else {
		request = LIBUSB_REQUEST_CLEAR_FEATURE;
		feature = USB_PORT_FEAT_POWER;
		index = port;
	}

	ret = libusb_open(hub_dev, &hub);
	if (ret) {
		fprintf(stderr, "Unable to open USB hub: %d\n", ret);
		return ret;
	}

	ret = libusb_control_transfer(hub, USB_RT_PORT,
					request, feature, index,
					NULL, 0, CTRL_TIMEOUT);

	if (ret) {
		fprintf(stderr, "Unable to send packet to USB hub: %d\n", ret);
		libusb_close(hub);
		return ret;
	}

	libusb_close(hub);
	return ret;
}

static int get_port(struct state *st, char *port, int *portnum,
			struct libusb_device **hub_dev) {
	struct libusb_device **devs;
	struct libusb_device *dev;
	int current;
	int ret = 0;
	int bus_offset = 0;
	int hubs_seen = 0;

	if (*port == 'i')
		bus_offset = 0;
	else if (*port == 'e')
		bus_offset = 1;
	else
		return -EINVAL;

	*portnum = strtoul(port+1, NULL, 0);
	if (*portnum < 1 || *portnum > 4)
		return -EINVAL;

	if (libusb_get_device_list(st->ctx, &devs) < 0){
		perror ("failed to access USB");
		return 0;
	}

	for (current=0; (dev = devs[current]) != NULL; current++) {
		struct libusb_device_descriptor device_desc;
		struct libusb_config_descriptor *config_desc;
		libusb_device_handle *uh;
		uint16_t dev_vid, dev_pid;

		ret = libusb_get_device_descriptor(dev, &device_desc);
		if (ret)
			goto err;

		ret = libusb_get_active_config_descriptor(dev, &config_desc);

		dev_vid = libusb_le16_to_cpu(device_desc.idVendor);
		dev_pid = libusb_le16_to_cpu(device_desc.idProduct);

		if (device_desc.bDeviceClass != LIBUSB_CLASS_HUB) {
			continue;
		}

		if (libusb_open(dev, &uh) != 0 )
			continue;

		if (dev_vid == 0x05e3 && dev_pid == 0x0614) {
			if (hubs_seen++ == bus_offset) {
				libusb_ref_device(dev);
				*hub_dev = dev;
			}
		}

		libusb_free_config_descriptor(config_desc);
		libusb_close(uh);
	}
err:
	libusb_free_device_list(devs, 1);

	return ret;
}


static int set_port(struct state *st, char *port, int val) {
	struct libusb_device *hub;
	int ret;
	int portnum;
	ret = get_port(st, port, &portnum, &hub);
	if (ret == -EINVAL) {
		fprintf(stderr, "Error: Must specify port as 'eN' or 'iN' "
				"for either internal or external port.\n"
				"Port number must be 1, 2, 3, or 4.\n");
		return ret;
	}
	else if (ret)
		return ret;

	ret = set_port_power(st, hub, portnum, val);
	libusb_unref_device(hub);
	return ret;
}

static int port_enable(struct state *st, char *port) {
	return set_port(st, port, 1);
}

static int port_disable(struct state *st, char *port) {
	return set_port(st, port, 0);
}

static int list_ports(struct state *st) {
	struct libusb_device **devs;
	struct libusb_device *dev;
	int current;
	int ret = 0;

	if (libusb_get_device_list(st->ctx, &devs) < 0){
		perror ("failed to access USB");
		return 0;
	}

	for (current=0; (dev = devs[current]) != NULL; current++) {
		struct libusb_device_descriptor device_desc;
		struct libusb_config_descriptor *config_desc;
		libusb_device_handle *uh;
		uint16_t dev_vid, dev_pid;

		ret = libusb_get_device_descriptor(dev, &device_desc);
		if (ret)
			goto err;

		ret = libusb_get_active_config_descriptor(dev, &config_desc);

		dev_vid = libusb_le16_to_cpu(device_desc.idVendor);
		dev_pid = libusb_le16_to_cpu(device_desc.idProduct);
		printf("Bus %03d Device %03d: ID %04x:%04x",
				libusb_get_bus_number(dev),
			       	libusb_get_device_address(dev),
			       	dev_vid, dev_pid);

		if (device_desc.bDeviceClass != LIBUSB_CLASS_HUB) {
			printf("  Device is not a hub\n");
			continue;
		}

		if (libusb_open(dev, &uh) != 0 ) {
			printf("\n");
			continue;
		}


		if (dev_vid == 0x05e3) 
			hub_port_status(uh, 4);


		libusb_free_config_descriptor(config_desc);
		libusb_close(uh);

		printf("\n");
	}
err:
	libusb_free_device_list(devs, 1);

	return ret;
}

static int print_help(struct state *st) {
	fprintf (stderr,
		"Usage: %s [-e PORT] [-d PORT] \n"
		"\n"
		"Where PORT is defined as 'i' or 'e' followed by the port number.\n"
		"For example, 'i4' for internal port 4, or 'e2' for external port 2.\n"
		"",
		st->progname);
	return 0;
}


static int novena_hub_init(struct state *st) {
	int ret;
	ret = libusb_init(&st->ctx);
	if (ret)
		return ret;

	libusb_set_debug(st->ctx, 3);

	return ret;
}

static int novena_hub_deinit(struct state *st) {
	libusb_exit(st->ctx);
	return 0;
}

/*
* HUB-CTRL  -  program to control port power/led of USB hub
*
*   # hub-ctrl                    // List hubs available
*   # hub-ctrl -P 1               // Power off at port 1
*   # hub-ctrl -P 1 -p 1          // Power on at port 1
*   # hub-ctrl -P 2 -l            // LED on at port 2
*
* Requirement: USB hub which implements port power control / indicator control
*
*      Work fine:
*         Elecom's U2H-G4S: www.elecom.co.jp (indicator depends on power)
*         04b4:6560
*
*	   Sanwa Supply's USB-HUB14GPH: www.sanwa.co.jp (indicators don't)
*
*	   Targus, Inc.'s PAUH212: www.targus.com (indicators don't)
*         04cc:1521
*
*	   Hawking Technology's UH214: hawkingtech.com (indicators don't)
*
*/



int main(int argc, char * const *argv) {
	int power_up = 0;
	int power_down = 0;
	int action_taken = 0;
	int ret;

	struct state st;

	int ch;

	struct option longopts[] = {
		{ "list-ports",	no_argument,		NULL,		'l' },
		{ "port-enable",required_argument,	&power_up,	'e' },
		{ "port-disable",required_argument,	&power_down,	'd' },
		{ "help",	no_argument,		NULL,		'h' },
		{ NULL,		0,			NULL,		0 }
	};

	memset(&st, 0, sizeof(st));

	st.progname = argv[0];
	ret = novena_hub_init(&st);
	if (ret)
		return ret;

	while ((ch = getopt_long(argc, argv, "le:d:h", longopts, NULL)) != -1) {
		switch (ch) {
			case 'h':
				print_help(&st);
				action_taken = 1;
				break;

			case 'e':
				port_enable(&st, optarg);
				action_taken = 1;
				break;

			case 'd':
				port_disable(&st, optarg);
				action_taken = 1;
				break;

			case 'l':
				list_ports(&st);
				action_taken = 1;
				break;

			default:
				break;
		}
	}

	if (!action_taken)
		print_help(&st);

#if 0
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-')
			switch (argv[i][1])
		{
			case 'h':
				if (++i >= argc || busnum > 0 || devnum > 0)
					exit_with_usage (argv[0]);
				hub = atoi (argv[i]);
				break;

			case 'b':
				if (++i >= argc || hub >= 0)
					exit_with_usage (argv[0]);
				busnum = atoi (argv[i]);
				break;

			case 'd':
				if (++i >= argc || hub >= 0)
					exit_with_usage (argv[0]);
				devnum = atoi (argv[i]);
				break;

			case 'P':
				if (++i >= argc)
					exit_with_usage (argv[0]);
				port = atoi (argv[i]);
				break;

			case 'l':
				if (cmd != COMMAND_SET_NONE)
					exit_with_usage (argv[0]);
				if (++i < argc)
					value = atoi (argv[i]);
				else
					value = HUB_LED_GREEN;
				cmd = COMMAND_SET_LED;
				break;

			case 'p':
				if (cmd != COMMAND_SET_NONE)
					exit_with_usage (argv[0]);
				if (++i < argc)
					value = atoi (argv[i]);
				else
					value= 0;
				cmd = COMMAND_SET_POWER;
				break;

			case 'v':
				verbose = 1;
				if (argc == 2)
					listing = 1;
				break;

			default:
				exit_with_usage (argv[0]);
		}
		else
			exit_with_usage (argv[0]);
	}

	if ((busnum > 0 && devnum <= 0) || (busnum <= 0 && devnum > 0))
		/* BUS is specified, but DEV is'nt, or ... */
		exit_with_usage (argv[0]);

	/* Default is the hub #0 */
	if (hub < 0 && busnum == 0)
		hub = 0;

	/* Default is POWER */
	if (cmd == COMMAND_SET_NONE)
		cmd = COMMAND_SET_POWER;


	if (usb_find_hubs (listing, verbose, busnum, devnum, hub) <= 0)
	{
		fprintf (stderr, "No hubs found.\n");
		libusb_exit( ctx );
		exit (1);
	}

	if (listing){
		libusb_exit( ctx );
		exit (0);
	}

	if (hub < 0)
		hub = get_hub (busnum, devnum);

	if (hub >= 0 && hub < number_of_hubs_with_feature){
		if ( libusb_open (hubs[hub].dev, &uh ) || uh == NULL)
		{
			fprintf (stderr, "Device not found.\n");
			result = 1;
		}
		else
		{
			if (cmd == COMMAND_SET_POWER){
				if (value){
					request = LIBUSB_REQUEST_SET_FEATURE;
					feature = USB_PORT_FEAT_POWER;
					index = port;
				}else{
					request = LIBUSB_REQUEST_CLEAR_FEATURE;
					feature = USB_PORT_FEAT_POWER;
					index = port;
				}
			}else{
				request = LIBUSB_REQUEST_SET_FEATURE;
				feature = USB_PORT_FEAT_INDICATOR;
				index = (value << 8) | port;
			}

			if (verbose)
				printf ("Send control message (REQUEST=%d, FEATURE=%d, INDEX=%d)\n",
				request, feature, index);
/*
			if(libusb_detach_kernel_driver( uh, 0 ))
				perror("libusb_detach_kernel_driver");
			if(libusb_claim_interface( uh, 0 ))
				perror("libusb_claim_interface");
*/
			if (libusb_control_transfer (uh, USB_RT_PORT, request, feature, index,
				NULL, 0, CTRL_TIMEOUT) < 0)
			{
				perror ("failed to control.\n");
				result = 1;
			}

			if (verbose)
				hub_port_status (uh, hubs[hub].nport);
/*
			libusb_release_interface( uh,0 );
			libusb_attach_kernel_driver( uh, 0); 
*/

			libusb_close (uh);
		}
	}
#endif
	novena_hub_deinit(&st);

	return 0;
}
