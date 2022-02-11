#include <stdlib.h>
#include <syslog.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <asm/byteorder.h>
#include <sys/types.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>

#include "a2m_usb.h"

libusb_device_handle* usb_devh;
libusb_context* usb_ctx = NULL;

struct libusb_device_handle* a2m_usb_find_device(uint16_t vendor_id, uint16_t product_id);

bool a2m_usb_init()
{
	//init usb
	int ret;

	syslog(LOG_MAKEPRI(LOG_USER, LOG_INFO), "INFO: USB: init");

	ret = libusb_init(&usb_ctx); //initialize a library session
	if (ret < 0) {
		syslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), "ERROR: USB: Init error: %s", libusb_strerror(ret));
		return false;
	}
	syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: USB: Init done");
	return true;
}

bool a2m_usb_open()
{
	int counter = 0, ret = 0;

	uint16_t vendor, product;
	//setup USB data
	vendor = 0x03eb;
	product = 0x2013;
	do {

		syslog(LOG_MAKEPRI(LOG_USER, LOG_INFO), "INFO: USB: Searching VOC Stick");
		usb_devh = a2m_usb_find_device(vendor, product);
		if (usb_devh == NULL)
			syslog(LOG_MAKEPRI(LOG_USER, LOG_INFO), "INFO: USB: No device found, wait 10sec...");

		sleep(10);

		++counter;

		if (counter == 10) {
			syslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), "ERROR: USB: Device not found.");
			return false;
		}

	} while (usb_devh == NULL);
	assert(usb_devh);
	syslog(LOG_MAKEPRI(LOG_USER, LOG_INFO), "INFO: USB: device found");

	if (libusb_kernel_driver_active(usb_devh, 0) == 1) { //find out if kernel driver is attached
		syslog(LOG_MAKEPRI(LOG_USER, LOG_INFO), "INFO: USB: Kernel driver active, detaching...");
		ret = libusb_detach_kernel_driver(usb_devh, 0);
		if (ret == 0) {
			syslog(LOG_MAKEPRI(LOG_USER, LOG_INFO), "INFO: USB: Kernel driver detached.");
		}
		else {
			syslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), "ERROR: USB: Kernel driver could not be detached: %s", libusb_strerror(ret));
			return false;
		}
	}

	syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: USB: claim interface");
	ret = libusb_claim_interface(usb_devh, 0);
	if (ret != 0) {
		syslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), "ERROR: USB: claim failed with error: %s", libusb_strerror(ret));
		return false;
	}
	syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: USB: interface claimed");

	return true;
}

void a2m_usb_close() {
	if (usb_devh != NULL) {
		syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: Closing USB device.");
		libusb_close(usb_devh);
	}
	if (usb_ctx != NULL) {
		syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: Closing LIB USB.");
		libusb_exit(usb_ctx);
	}
}

unsigned short a2m_usb_read_voc()
{
	int ret, cnt;
	unsigned short iresult = 0;
	unsigned char buf[20];

	// USB COMMAND TO REQUEST DATA
	// @h*TR
	syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: USB: Write request to device");
	memset(buf, 0, 20);
	memcpy(buf, "\x40\x68\x2a\x54\x52\x0a\x40\x40\x40\x40\x40\x40\x40\x40\x40\x40", 0x0000010);
	syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: USB: Write to device: %s", buf);
	ret = libusb_interrupt_transfer(usb_devh, 0x00000002, buf, 0x0000010, &cnt, 500);
	//TODO: error handling
	syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: USB: write done, len %d, result: %s", cnt, libusb_strerror(ret));

	usleep(250000);

	syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: USB: Read");
	memset(buf, 0, 20);
	ret = libusb_interrupt_transfer(usb_devh, 0x00000081, buf, 0x0000010, &cnt, 500);
	//TODO: error handling
	syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: USB: read done, len %d, result: %s", cnt, libusb_strerror(ret));

	if (!((cnt == 0) || (cnt == 16))) {
		syslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), "ERROR: USB: read result length wrong (0 or 16): %d", cnt);
		//TODO: error handling
	}

	if (cnt == 0) {
		usleep(250000);

		syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: USB: Length was 0, read again");
		memset(buf, 0, 20);
		ret = libusb_interrupt_transfer(usb_devh, 0x00000081, buf, 0x0000010, &cnt, 500);
		//TODO: error handling
		syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: USB: read done, len %d, result: %s", cnt, libusb_strerror(ret));
	}

	memcpy(&iresult, buf + 2, 2);
	return __le16_to_cpu(iresult);
}

void a2m_usb_flush()
{
	int cnt, ret;
	unsigned char buf[20];
	//libusb_set_debug(usb_ctx, LIBUSB_LOG_LEVEL_DEBUG);
	syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: USB: Read any remaining data from USB");
	ret = libusb_interrupt_transfer(usb_devh, 0x00000081, buf, 0x0000010, &cnt, 500);
	//TODO: error-handling...
	syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: USB: Read done, result: %s", libusb_strerror(ret));
}

struct libusb_device_handle* a2m_usb_find_device(uint16_t vendor_id, uint16_t product_id) {
	struct libusb_device** devs;
	struct libusb_device* found = NULL;
	struct libusb_device* dev;
	struct libusb_device_handle* dev_handle = NULL;
	size_t i = 0;
	int ret;

	syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: USB: getting device list");
	ret = libusb_get_device_list(usb_ctx, &devs);
	if (ret < 0) {
		syslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), "ERROR: USB: Error getting device list: %s", libusb_strerror(ret));
		return NULL;
	}

	while ((dev = devs[i++]) != NULL) {
		struct libusb_device_descriptor desc;
		syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: USB: getting device descriptor");
		ret = libusb_get_device_descriptor(dev, &desc);
		if (ret < 0) {
			syslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), "ERROR: USB: Error getting device descriptor: %s", libusb_strerror(ret));
			goto out;
		}
		if (desc.idVendor == vendor_id && desc.idProduct == product_id) {
			syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: USB: Stick found");
			found = dev;
			break;
		}
	}

	if (found) {
		syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: USB: Open device");
		ret = libusb_open(found, &dev_handle);
		if (ret < 0) {
			syslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), "ERROR: USB: Error opening device: %s", libusb_strerror(ret));
			dev_handle = NULL;
		}
	}

out:
	syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: USB: Free dev list");
	libusb_free_device_list(devs, 1);
	return dev_handle;
}
