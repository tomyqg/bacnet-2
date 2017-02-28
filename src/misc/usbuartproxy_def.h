/*
 * usbuartproxy_def.h
 *
 *  Created on: Oct 21, 2015
 *      Author: lin
 */

#ifndef _USBUARTPROXY_DEF_H_
#define _USBUARTPROXY_DEF_H_

#include <stdio.h>
#include <pthread.h>
#include <libusb-1.0/libusb.h>

#include "misc/usbuartproxy.h"
#include "misc/eventloop.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int usb_dbg_verbos;
extern int usb_dbg_warn;
extern int usb_dbg_err;

#define USB_ERROR(fmt, args...)                    	\
do {                                                \
    if (usb_dbg_err) {                             	\
       printf(fmt, ##args);                         \
    }                                               \
} while (0)

#define USB_WARN(fmt, args...)                     	\
do {                                                \
    if (usb_dbg_warn) {                            	\
        printf(fmt, ##args);                        \
    }                                               \
} while (0)

#define USB_VERBOS(fmt, args...)                   	\
do {                                                \
    if (usb_dbg_verbos) {                          	\
       printf(fmt, ##args);                         \
    }                                               \
} while (0)

#define  USB_INTERFACE_CLASS      		(0xff)
#define  USB_REQ_CLEAR_FEATURE   		(0x01)
#define  USB_REQ_SET_FEATURE        	(0x03)
#define  USB_REQ_SET_PARAMETER      	(0x80)
#define  USB_REQ_GET_PARAMETER      	(0x81)
#define  USB_FEATURE_ENABLE         	(0x80)
#define  RESERVE_TRANSFER           	(4)

struct fd_event_cared {
    int fd;
    short events_cared;
};

typedef struct usb_serial_impl_s {
    usb_serial_t base;
    uint8_t ep_out;
    uint8_t ep_in;
    libusb_context *ctx;
    libusb_device_handle *handler;
} usb_serial_impl_t;

typedef struct usb_serial_async_impl_s {
    usb_serial_impl_t base_impl;
    el_loop_t  *el;
    el_watch_t *watcher;
    pthread_mutex_t mutex;
    usb_serial_callback read_callback;
    usb_serial_callback write_callback;
    usb_serial_callback control_callback;
    unsigned long data;
    unsigned in_xfr_count;
    unsigned out_xfr_count;
    unsigned control_xfr_count;
    std::list<struct libusb_transfer *> in_xfr;
    std::list<struct libusb_transfer *> out_xfr;
    std::list<struct libusb_transfer *> control_xfr;
} usb_serial_async_impl_t;

#ifdef __cplusplus
}
#endif

#endif /* _USBUARTPROXY_DEF_H_ */

