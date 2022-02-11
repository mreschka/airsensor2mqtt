#pragma once

#include <stdlib.h>
#include <syslog.h>
#include <stdint.h>
#include <stdbool.h>
#include <libusb-1.0/libusb.h>

bool a2m_usb_init();
bool a2m_usb_open();
void a2m_usb_close();

unsigned short a2m_usb_read_voc();
void a2m_usb_flush();
