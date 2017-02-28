/*
 * usbuartproxy.cpp
 *
 *  Created on: Oct 21, 2015
 *      Author: lin
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "list"
#include "usbuartproxy_def.h"
#include "misc/cJSON.h"

int usb_dbg_verbos = 0;
int usb_dbg_warn = 0;
int usb_dbg_err = 1;

struct cJSON* usb_serial_query(void)
{
    libusb_context *ctx;
    libusb_device **list = NULL;
    ssize_t count;

    int rv = libusb_init(&ctx);
    if (rv) {
        USB_ERROR("%s: init ctx failed: %s\r\n", __func__,
                libusb_strerror((enum libusb_error)rv));
        return NULL;
    }

    count = libusb_get_device_list(ctx, &list);
    if (count < 0) {
        USB_ERROR("%s: get usb device list failed: %s\r\n", __func__,
                libusb_strerror((enum libusb_error)count));
        libusb_exit(ctx);
        return NULL;
    }

    int myidx = 0;
    cJSON *root = cJSON_CreateArray();
    for (int idx = 0; idx < count; ++idx) {
        libusb_device *device = list[idx];
        libusb_device_handle *handle;
        struct libusb_device_descriptor desc;
        struct libusb_config_descriptor *config;
        const struct libusb_interface_descriptor *itf_desc;
        int num_altsetting;

        rv = libusb_get_device_descriptor(device, &desc);
        if (rv != 0) {
            USB_ERROR("%s: get usb device descriptor failed: %s\r\n", __func__,
                    libusb_strerror((enum libusb_error)rv));
            goto err;
        }

        if (desc.idVendor != 0xf055 || desc.idProduct != 0xc52b)
            continue;

        cJSON *mydev = cJSON_CreateObject();
        cJSON_AddItemToArray(root, mydev);
        cJSON_AddItemToObject(mydev, "index", cJSON_CreateNumber(myidx));

        rv = libusb_open(device, &handle);
        if (rv != 0) {
            USB_ERROR("%s: libusb_open failed: %s\r\n", __func__,
                    libusb_strerror((enum libusb_error)rv));
            goto err;
        }

        unsigned char serial[1024];
        rv = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber,
                serial, sizeof(serial));
        libusb_close(handle);

        if (rv < 0) {
            USB_ERROR("%s: libusb get string descriptor failed: %s\n", __func__,
                    libusb_strerror((enum libusb_error)rv));
            goto err;
        }

        if ((unsigned)rv >= sizeof(serial)) {
            USB_ERROR("%s: get string descriptor overflow(%d)\n", __func__, rv);
            goto err;
        }
        serial[rv] = 0;
        cJSON_AddStringToObject(mydev, "serial", (char*)serial);

        cJSON *myitf = cJSON_CreateArray();
        cJSON_AddItemToObject(mydev, "interface", myitf);

        rv = libusb_get_config_descriptor(device, 0, &config);
        if (rv != 0) {
            USB_ERROR("%s: get usb config descriptor failed: %s\r\n", __func__,
                    libusb_strerror((enum libusb_error)rv));
            goto err;
        }

        for (int itfidx = 0; itfidx < config->bNumInterfaces; ++itfidx) {
            num_altsetting = config->interface[itfidx].num_altsetting;
            if (num_altsetting <= 0)
                continue;

            itf_desc = &config->interface[itfidx].altsetting[0];
            if (itf_desc->bInterfaceClass != USB_INTERFACE_CLASS) {
                USB_ERROR("%s: interface class(%d) not match desired: %d\r\n", __func__,
                        itf_desc->bInterfaceClass, USB_INTERFACE_CLASS);
                libusb_free_config_descriptor(config);
                goto err;
            }

            cJSON_AddItemToArray(myitf, cJSON_CreateNumber(
                    itf_desc->bInterfaceSubClass));
        }

        libusb_free_config_descriptor(config);
        myidx++;
    }

out:
    libusb_free_device_list(list, 1);
    libusb_exit(ctx);
    return root;

err:
    cJSON_Delete(root);
    root = NULL;
    goto out;
}

static void usb_serial_event_handler(
        el_watch_t *watch,
        __attribute__((unused))int events)
{
    libusb_context *ctx = (libusb_context *)watch->data;
    struct timeval tv = {0};
    int rv;

    rv = libusb_handle_events_timeout_completed(ctx, &tv, NULL);
    if (rv != 0) {
        USB_ERROR("%s: handle events failed: %s\n", __func__,
                libusb_strerror((enum libusb_error)rv));
    }
}

static void usb_fd_add_cb(int fd, short events, void *user_data)
{
    fd_event_cared *care = (fd_event_cared*)user_data;
    care->fd = fd;
    care->events_cared = events;
}

static int _usb_under_init(usb_serial_impl_t *serial, fd_event_cared *care)
{
    libusb_device **list = NULL;
    ssize_t count;
    int rv;

    rv = libusb_init(&serial->ctx);
    if (rv) {
        USB_ERROR("%s: init ctx failed: %s\r\n", __func__,
                libusb_strerror((enum libusb_error)rv));
        return -EPERM;
    }

    count = libusb_get_device_list(serial->ctx, &list);
    if (count < 0) {
        USB_ERROR("%s: get usb device list failed: %s\r\n", __func__,
                libusb_strerror((enum libusb_error)count));
        libusb_exit(serial->ctx);
        serial->ctx = NULL;
        return -EPERM;
    }

    for (int idx = 0, mydev_idx = 0; idx < count; ++idx) {
        libusb_device *device = list[idx];
        struct libusb_device_descriptor desc;
        struct libusb_config_descriptor *config;
        const struct libusb_interface_descriptor *itf_desc;
        int num_altsetting;
        bool found;

        rv = libusb_get_device_descriptor(device, &desc);
        if (rv != 0) {
            USB_ERROR("%s: get usb device descriptor failed: %s\r\n", __func__,
                    libusb_strerror((enum libusb_error)rv));
            goto out;
        }

        if (desc.idVendor != 0xf055 || desc.idProduct != 0xc52b)
            continue;

        if (mydev_idx++ != serial->base.enumerated_idx)
            continue;

        rv = libusb_get_config_descriptor(device, 0, &config);
        if (rv != 0) {
            USB_ERROR("%s: get usb config descriptor failed: %s\r\n", __func__,
                    libusb_strerror((enum libusb_error)rv));
            goto out;
        }

        if (config->bNumInterfaces <= serial->base.interface_idx) {
            USB_ERROR("%s: usb device hasn't that interface\r\n", __func__);
            rv = -EPERM;
            goto clean_descriptor;
        }

        num_altsetting = config->interface[serial->base.interface_idx].num_altsetting;
        found = false;
        for (int i = 0; i < num_altsetting; ++i) {
            itf_desc = &config->interface[serial->base.interface_idx].altsetting[i];

            if (itf_desc->bInterfaceClass != USB_INTERFACE_CLASS
                    || itf_desc->bInterfaceSubClass != serial->base.subclass) {
                USB_ERROR("%s: interface class(%d) subclass(%d) not match desired: %d, %d\r\n", __func__,
                        itf_desc->bInterfaceClass, itf_desc->bInterfaceSubClass,
                        USB_INTERFACE_CLASS, serial->base.subclass);
                rv = -EPERM;
                goto clean_descriptor;
            }

            if (itf_desc->bInterfaceProtocol == serial->base.protocol) {
                found = true;
                break;
            }
        }

        if (!found) {
            USB_ERROR("%s: protocol(%d) not found\r\n", __func__,
                    serial->base.protocol);
            rv = -EPERM;
            goto clean_descriptor;
        }

        if (itf_desc->bNumEndpoints != 2) {
            USB_ERROR("%s: num of endpoints(%d) isn't 2\r\n", __func__,
                    itf_desc->bNumEndpoints);
            rv = -EPERM;
            goto clean_descriptor;
        }

        if (((itf_desc->endpoint[0].bEndpointAddress
                ^ itf_desc->endpoint[1].bEndpointAddress) & 0x80) != 0x80) {
            USB_ERROR("%s: endpoints should be 1 in 1 out\r\n", __func__);
            rv = -EPERM;
            goto clean_descriptor;
        }

        if (itf_desc->endpoint[0].bmAttributes != 0x02
                || itf_desc->endpoint[0].wMaxPacketSize != 64) { // check bulk/64
            USB_ERROR("%s: endpoints(%d) isn't bulk with 64 max packet size\r\n", __func__,
                    itf_desc->endpoint[0].bEndpointAddress);
            rv = -EPERM;
            goto clean_descriptor;
        }

        if (itf_desc->endpoint[1].bmAttributes != 0x02
                || itf_desc->endpoint[1].wMaxPacketSize != 64) { // check bulk/64
            USB_ERROR("%s: endpoints(%d) isn't bulk with 64 max packet size\r\n", __func__,
                    itf_desc->endpoint[1].bEndpointAddress);
            rv = -EPERM;
            goto clean_descriptor;
        }

        if (itf_desc->endpoint[0].bEndpointAddress & 0x80) {
            serial->ep_in = itf_desc->endpoint[0].bEndpointAddress;
            serial->ep_out = itf_desc->endpoint[1].bEndpointAddress;
        } else {
            serial->ep_in = itf_desc->endpoint[1].bEndpointAddress;
            serial->ep_out = itf_desc->endpoint[0].bEndpointAddress;
        }

        if (care)
            libusb_set_pollfd_notifiers(serial->ctx, usb_fd_add_cb, NULL, care);
        rv = libusb_open(device, &serial->handler);
        libusb_set_pollfd_notifiers(serial->ctx, NULL, NULL, NULL);

        if (rv != 0) {
            USB_ERROR("%s: open failed: %s\r\n", __func__,
                    libusb_strerror((enum libusb_error)rv));
            goto clean_descriptor;
        }

        rv = libusb_claim_interface(serial->handler, serial->base.interface_idx);
        if (rv != 0) {
            USB_ERROR("%s: claim interface failed: %s\r\n", __func__,
                    libusb_strerror((enum libusb_error)rv));
            libusb_close(serial->handler);
            goto clean_descriptor;
        }

        rv = libusb_set_interface_alt_setting(serial->handler, serial->base.interface_idx,
                itf_desc->bAlternateSetting);
        if (rv != 0) {
            USB_ERROR("%s: set alternate setting(%d) failed: %s\r\n", __func__,
                    itf_desc->bAlternateSetting, libusb_strerror((enum libusb_error)rv));
            libusb_release_interface(serial->handler, serial->base.interface_idx);
            libusb_close(serial->handler);
            goto clean_descriptor;
        }

clean_descriptor:
        libusb_free_config_descriptor(config);
        goto out;
    }

    USB_ERROR("%s: usb device not found\r\n", __func__);
    rv = -EPERM;

out:
    libusb_free_device_list(list, 1);
    if (rv) {
        libusb_exit(serial->ctx);
        serial->ctx = NULL;
    }
    return rv;
}

static void _usb_under_exit(usb_serial_impl_t *serial)
{
    libusb_release_interface(serial->handler, serial->base.interface_idx);
    libusb_close(serial->handler);
    libusb_exit(serial->ctx);
}

static void usb_out_cb(struct libusb_transfer *xfr)
{
    int rv;
    uint8_t *buffer;
    int actual_length;

    switch(xfr->status)
    {
        case LIBUSB_TRANSFER_COMPLETED:
            // Success here, data transfered are inside
            // xfr->buffer and the length is xfr->actual_length
            break;
        case LIBUSB_TRANSFER_CANCELLED:
            USB_VERBOS("%s: tx %d byte(s) cancelled\r\n", __func__, xfr->length);
            libusb_free_transfer(xfr);
            return;
        case LIBUSB_TRANSFER_STALL:
            USB_ERROR("%s: tx %d byte(s) stalled, retry\r\n", __func__, xfr->length);
            rv = libusb_submit_transfer(xfr);
            if (rv) {
                USB_ERROR("%s: usb re-submit after stall failed: %s\r\n", __func__,
                        libusb_strerror((enum libusb_error)rv));
                libusb_free_transfer(xfr);
            }
            return;
        case LIBUSB_TRANSFER_NO_DEVICE:
        case LIBUSB_TRANSFER_TIMED_OUT:
        case LIBUSB_TRANSFER_ERROR:
        case LIBUSB_TRANSFER_OVERFLOW:
        default:
            USB_ERROR("%s: tx %d byte(s) error: %d\r\n", __func__,
                    xfr->length, xfr->status);
            // Various type of errors here
            return;
    }

    USB_VERBOS("%s: tx %d byte(s) completed\r\n", __func__, xfr->actual_length);

    buffer = xfr->buffer;
    actual_length = xfr->actual_length;
    usb_serial_async_impl_t *serial = (usb_serial_async_impl_t*)xfr->user_data;

    pthread_mutex_lock(&serial->mutex);
    serial->out_xfr.pop_front();
    serial->out_xfr_count--;
    pthread_mutex_unlock(&serial->mutex);

    if (serial->write_callback
            && serial->write_callback(serial->data, buffer, actual_length)) {
        pthread_mutex_lock(&serial->mutex);

        rv = libusb_submit_transfer(xfr);
        if (rv) {
            USB_ERROR("%s: usb re-submit transfer failed: %s\r\n", __func__,
                    libusb_strerror((enum libusb_error)rv));
            libusb_free_transfer(xfr);
        } else {
            serial->out_xfr.push_back(xfr);
            serial->out_xfr_count++;
        }

        pthread_mutex_unlock(&serial->mutex);
    } else {
        libusb_free_transfer(xfr);
    }
}

static void usb_in_cb(struct libusb_transfer *xfr)
{
    int rv;
    switch(xfr->status)
    {
        case LIBUSB_TRANSFER_COMPLETED:
            // Success here, data transfered are inside
            // xfr->buffer and the length is xfr->actual_length
            break;
        case LIBUSB_TRANSFER_CANCELLED:
            USB_VERBOS("%s: rx cancelled\r\n", __func__);
            libusb_free_transfer(xfr);
            return;
        case LIBUSB_TRANSFER_STALL:
            USB_ERROR("%s: rx stalled, retry\r\n", __func__);
            rv = libusb_submit_transfer(xfr);
            if (rv) {
                USB_ERROR("%s: usb re-submit after stall failed: %s\r\n", __func__,
                        libusb_strerror((enum libusb_error)rv));
                libusb_free_transfer(xfr);
            }
            return;
        case LIBUSB_TRANSFER_NO_DEVICE:
        case LIBUSB_TRANSFER_TIMED_OUT:
        case LIBUSB_TRANSFER_ERROR:
        case LIBUSB_TRANSFER_OVERFLOW:
        default:
            USB_ERROR("%s: rx error: %d\r\n", __func__, xfr->status);
            // Various type of errors here
            return;
    }

    USB_VERBOS("%s: rx %d bytes completed\r\n", __func__, xfr->actual_length);

    usb_serial_async_impl_t *serial = (usb_serial_async_impl_t*)xfr->user_data;

    pthread_mutex_lock(&serial->mutex);
    serial->in_xfr.pop_front();
    serial->in_xfr_count--;
    pthread_mutex_unlock(&serial->mutex);

    if (serial->read_callback
            && serial->read_callback(serial->data, xfr->buffer, xfr->actual_length)) {
        pthread_mutex_lock(&serial->mutex);

        rv = libusb_submit_transfer(xfr);
        if (rv) {
            USB_ERROR("%s: usb re-submit transfer failed: %s\r\n", __func__,
                    libusb_strerror((enum libusb_error)rv));
            libusb_free_transfer(xfr);
        } else {
            serial->in_xfr.push_back(xfr);
            serial->in_xfr_count++;
        }

        pthread_mutex_unlock(&serial->mutex);
    } else {
        libusb_free_transfer(xfr);
    }
}

static void usb_control_cb(struct libusb_transfer *xfr)
{
    uint8_t *buffer;
    uint8_t *control_data;
    int actual_length;
    int rv;

    switch(xfr->status)
    {
        case LIBUSB_TRANSFER_COMPLETED:
            // Success here, data transfered are inside
            // xfr->buffer and the length is xfr->actual_length
            break;
        case LIBUSB_TRANSFER_CANCELLED:
            USB_ERROR("%s: control req(%d) value(%d) cancelled\r\n", __func__,
                    xfr->buffer[1], libusb_le16_to_cpu(*(uint16_t*)(xfr->buffer + 2)));
            free(xfr->buffer);
            libusb_free_transfer(xfr);
            return;
        case LIBUSB_TRANSFER_STALL:
            USB_ERROR("%s: control req(%d) value(%d) stalled, retry\r\n", __func__,
                    xfr->buffer[1], libusb_le16_to_cpu(*(uint16_t*)(xfr->buffer + 2)));
            rv = libusb_submit_transfer(xfr);
            if (rv) {
                USB_ERROR("%s: usb re-submit after stall failed: %s\r\n", __func__,
                        libusb_strerror((enum libusb_error)rv));
                libusb_free_transfer(xfr);
            }
            return;
        case LIBUSB_TRANSFER_NO_DEVICE:
        case LIBUSB_TRANSFER_TIMED_OUT:
        case LIBUSB_TRANSFER_ERROR:
        case LIBUSB_TRANSFER_OVERFLOW:
        default:
            USB_ERROR("%s: control req(%d) value(%d) error: %d\r\n", __func__,
                    xfr->buffer[1], libusb_le16_to_cpu(*(uint16_t*)(xfr->buffer + 2)),
                    xfr->status);
            // Various type of errors here
            return;
    }

    USB_VERBOS("%s: control req(%d) value(%d) completed\r\n", __func__,
            xfr->buffer[1], libusb_le16_to_cpu(*(uint16_t*)(xfr->buffer + 2)));

    buffer = xfr->buffer;
    actual_length = xfr->actual_length;
    control_data = libusb_control_transfer_get_data(xfr);
    usb_serial_async_impl_t *serial = (usb_serial_async_impl_t*)xfr->user_data;

    pthread_mutex_lock(&serial->mutex);
    serial->control_xfr.pop_front();
    serial->control_xfr_count--;
    pthread_mutex_unlock(&serial->mutex);

    if (serial->control_callback
            && serial->control_callback(serial->data, control_data,
                    actual_length - (control_data - buffer))) {
        pthread_mutex_lock(&serial->mutex);

        rv = libusb_submit_transfer(xfr);
        if (rv) {
            USB_ERROR("%s: usb re-submit transfer failed: %s\r\n", __func__,
                    libusb_strerror((enum libusb_error)rv));
            libusb_free_transfer(xfr);
            free(buffer);
        } else {
            serial->control_xfr.push_back(xfr);
            serial->control_xfr_count++;
        }

        pthread_mutex_unlock(&serial->mutex);
    } else {
        libusb_free_transfer(xfr);
        free(buffer);
    }
}

usb_serial_t* usb_serial_create(uint8_t enumerated_idx, uint8_t interface_idx,
        uint8_t subclass, uint8_t protocol)
{
    int rv;

    usb_serial_impl_t *serial = new usb_serial_impl_t();
    if (serial == NULL) {
        USB_ERROR("%s: calloc usb serial failed\r\n", __func__);
        goto out0;
    }

    serial->base.enumerated_idx = enumerated_idx;
    serial->base.interface_idx = interface_idx;
    serial->base.subclass = subclass;
    serial->base.protocol = protocol;
    serial->base.is_async = 0;

    rv = _usb_under_init(serial, NULL);
    if (rv) {
        USB_ERROR("%s: _usb_under_init failed\r\n", __func__);
        goto out1;
    }

    rv = usb_serial_enable(&serial->base, false, 0);
    if (rv) {
        USB_ERROR("%s: disable usb failed\r\n", __func__);
        goto out2;
    }

    return &serial->base;

out2:
    _usb_under_exit(serial);

out1:
    delete serial;

out0:
    return NULL;
}

usb_serial_t* usb_serial_async_create(uint8_t enumerated_idx, uint8_t interface_idx,
        uint8_t subclass, uint8_t protocol, el_loop_t *el,
        usb_serial_callback read_callback, usb_serial_callback write_callback,
        usb_serial_callback control_callback, unsigned long data)
{
    int rv;

    usb_serial_async_impl_t *serial = new usb_serial_async_impl_t();
    if (serial == NULL) {
        USB_ERROR("%s: calloc usb serial failed\r\n", __func__);
        goto out0;
    }

    serial->base_impl.base.enumerated_idx = enumerated_idx;
    serial->base_impl.base.interface_idx = interface_idx;
    serial->base_impl.base.subclass = subclass;
    serial->base_impl.base.protocol = protocol;
    serial->base_impl.base.is_async = 1;
    serial->el = el;
    serial->read_callback = read_callback;
    serial->write_callback = write_callback;
    serial->control_callback = control_callback;
    serial->data = data;
    serial->in_xfr_count = 0;
    serial->out_xfr_count = 0;
    serial->control_xfr_count = 0;

    fd_event_cared care;
    rv = _usb_under_init(&serial->base_impl, &care);
    if (rv) {
        USB_ERROR("%s: _usb_under_init failed\r\n", __func__);
        goto out1;
    }

    rv = pthread_mutex_init(&serial->mutex, NULL);
    if (rv) {
        USB_ERROR("%s: mutex init failed cause %s\r\n", __func__,
                strerror(rv));
        goto out2;
    }

    serial->watcher = el_watch_create(serial->el, care.fd, care.events_cared);
    if (serial->watcher == NULL) {
        USB_ERROR("%s: event watch create failed\r\n", __func__);
        goto out3;
    }
    serial->watcher->handler = usb_serial_event_handler;
    serial->watcher->data = serial->base_impl.ctx;

    rv = usb_serial_enable(&serial->base_impl.base, false, 0);
    if (rv) {
        USB_ERROR("%s: disable usb failed\r\n", __func__);
        goto out4;
    }

    return &serial->base_impl.base;

out4:
    el_watch_destroy(serial->el, serial->watcher);

out3:
    pthread_mutex_destroy(&serial->mutex);

out2:
    _usb_under_exit(&serial->base_impl);

out1:
    delete serial;

out0:
    return NULL;
}

void usb_serial_destroy(usb_serial_t *serial)
{
    if (serial == NULL) {
        USB_ERROR("%s: invalid serial\r\n", __func__);
        return;
    }

    int rv = libusb_control_transfer(((usb_serial_impl_t*)serial)->handler,
            0b01000001, USB_REQ_CLEAR_FEATURE, USB_FEATURE_ENABLE,
            serial->interface_idx, NULL, 0, 2000);

    if (rv != 0) {
        USB_ERROR("%s: disable interface failed: %s\r\n", __func__,
                libusb_strerror((enum libusb_error)rv));
        return;
    }

    if (serial->is_async) {
        usb_serial_async_impl_t *async = (usb_serial_async_impl_t *)serial;
        usb_serial_cancel_async(serial);

        el_sync(async->el);
        usb_serial_event_handler(async->watcher, 0); // clean canceled transfer;
        el_unsync(async->el);

        el_watch_destroy(async->el, async->watcher);
        pthread_mutex_destroy(&async->mutex);
        _usb_under_exit(&async->base_impl);
        delete async;
    } else {
        _usb_under_exit((usb_serial_impl_t*)serial);
        delete serial;
    }
}

void usb_serial_cancel_async(usb_serial_t *base)
{
    usb_serial_async_impl_t *serial = (usb_serial_async_impl_t*)base;

    if (base == NULL) {
        USB_ERROR("%s: invalid serial\r\n", __func__);
        return;
    }

    if (!base->is_async) {
        USB_ERROR("%s: it is not a async port\r\n", __func__);
        return;
    }

    el_sync(serial->el);

    pthread_mutex_lock(&serial->mutex);

    for (std::list<struct libusb_transfer*>::iterator it = serial->in_xfr.begin();
            it != serial->in_xfr.end(); ++it) {
        int rv = libusb_cancel_transfer(*it);
        if (rv != LIBUSB_SUCCESS && rv != LIBUSB_ERROR_NOT_FOUND) {
            USB_ERROR("%s: cancel in xfr failed: %s\r\n", __func__,
                    libusb_strerror((enum libusb_error)rv));
        }
    }

    for (std::list<struct libusb_transfer*>::iterator it = serial->out_xfr.begin();
            it != serial->out_xfr.end(); ++it) {
        int rv = libusb_cancel_transfer(*it);
        if (rv != LIBUSB_SUCCESS && rv != LIBUSB_ERROR_NOT_FOUND) {
            USB_ERROR("%s: cancel out xfr failed: %s\r\n", __func__,
                    libusb_strerror((enum libusb_error)rv));
        }
    }

    for (std::list<struct libusb_transfer*>::iterator it = serial->control_xfr.begin();
            it != serial->control_xfr.end(); ++it) {
        int rv = libusb_cancel_transfer(*it);
        if (rv != LIBUSB_SUCCESS && rv != LIBUSB_ERROR_NOT_FOUND) {
            USB_ERROR("%s: cancel control xfr failed: %s\r\n", __func__,
                    libusb_strerror((enum libusb_error)rv));
        }
    }

    serial->in_xfr.clear();
    serial->out_xfr.clear();
    serial->control_xfr.clear();
    serial->in_xfr_count = 0;
    serial->out_xfr_count = 0;
    serial->control_xfr_count = 0;

    pthread_mutex_unlock(&serial->mutex);

    el_unsync(serial->el);
}

int usb_serial_async_read(usb_serial_t *base, unsigned char *buf, unsigned len)
{
    usb_serial_async_impl_t *serial = (usb_serial_async_impl_t*)base;

    if (serial == NULL || (buf == NULL && len != 0)) {
        USB_ERROR("%s: invalid serial or buf\r\n", __func__);
        return -EINVAL;
    }

    if (!base->is_async) {
        USB_ERROR("%s: it is not a async port\r\n", __func__);
        return -EINVAL;
    }


    libusb_transfer *xfr;
    pthread_mutex_lock(&serial->mutex);

    xfr = libusb_alloc_transfer(0);
    if (xfr == NULL) {
        pthread_mutex_unlock(&serial->mutex);
        USB_ERROR("%s: alloc transfer failed\r\n", __func__);
        return -ENOMEM;
    }

    libusb_fill_bulk_transfer(xfr, serial->base_impl.handler,
            serial->base_impl.ep_in, buf, len,
            usb_in_cb, serial, 0);

    int rv = libusb_submit_transfer(xfr);
    if (rv) {
        pthread_mutex_unlock(&serial->mutex);
        USB_ERROR("%s: submit transfer failed: %s\r\n", __func__,
                libusb_strerror((enum libusb_error)rv));
        libusb_free_transfer(xfr);
        return -EPERM;
    }

    serial->in_xfr.push_back(xfr);
    serial->in_xfr_count++;
    pthread_mutex_unlock(&serial->mutex);

    return 0;
}

int usb_serial_async_write(usb_serial_t *base, unsigned char *buf, unsigned len)
{
    usb_serial_async_impl_t *serial = (usb_serial_async_impl_t*)base;

    if (serial == NULL || (buf == NULL && len != 0)) {
        USB_ERROR("%s: invalid serial or buf\r\n", __func__);
        return -EINVAL;
    }

    if (!base->is_async) {
        USB_ERROR("%s: it is not a async port\r\n", __func__);
        return -EINVAL;
    }

    libusb_transfer *xfr;
    pthread_mutex_lock(&serial->mutex);

padding_xfr:
    xfr = libusb_alloc_transfer(0);
    if (xfr == NULL) {
        pthread_mutex_unlock(&serial->mutex);
        USB_ERROR("%s: alloc transfer failed\r\n", __func__);
        return -ENOMEM;
    }

    libusb_fill_bulk_transfer(xfr, serial->base_impl.handler,
            serial->base_impl.ep_out, buf, len,
            usb_out_cb, serial, 0);
    xfr->flags = LIBUSB_TRANSFER_SHORT_NOT_OK;

    int rv = libusb_submit_transfer(xfr);
    if (rv) {
        pthread_mutex_unlock(&serial->mutex);
        USB_ERROR("%s: submit transfer failed: %s\r\n", __func__,
                libusb_strerror((enum libusb_error)rv));
        libusb_free_transfer(xfr);
        return -EPERM;
    }

    serial->out_xfr.push_back(xfr);
    serial->out_xfr_count++;

    if (len && len % 64 == 0) {
        len = 0;
        goto padding_xfr;
    }
    pthread_mutex_unlock(&serial->mutex);

    return 0;
}

int usb_serial_async_get_pending(usb_serial_t *base, unsigned *read,
        unsigned *write, unsigned *control)
{
    usb_serial_async_impl_t *serial = (usb_serial_async_impl_t*)base;

    if (serial == NULL) {
        USB_ERROR("%s: invalid serial\r\n", __func__);
        return -EINVAL;
    }

    if (!base->is_async) {
        USB_ERROR("%s: it is not a async port\r\n", __func__);
        return -EINVAL;
    }

    pthread_mutex_lock(&serial->mutex);

    if (read) {
        *read = serial->in_xfr_count;
    }
    if (write){
        *write = serial->out_xfr_count;
    }
    if (control){
        *control = serial->control_xfr_count;
    }

    pthread_mutex_unlock(&serial->mutex);

    return 0;
}

int usb_serial_enable(usb_serial_t *base, int enable, unsigned timeout)
{
    usb_serial_impl_t *serial = (usb_serial_impl_t*)base;

    if (base == NULL) {
        USB_ERROR("%s: invalid serial\r\n", __func__);
        return -EINVAL;
    }

    int rv = libusb_control_transfer(serial->handler, 0b01000001,
            (enable ? USB_REQ_SET_FEATURE : USB_REQ_CLEAR_FEATURE),
            USB_FEATURE_ENABLE, base->interface_idx,
            NULL, 0, timeout);

    if (rv != 0) {
        USB_ERROR("%s: enable/disable interface failed: %s\r\n", __func__,
                libusb_strerror((enum libusb_error)rv));
        return -EPERM;
    }

    return 0;
}

int usb_serial_getpara(usb_serial_t *base, uint8_t para_idx,
        unsigned char *para, unsigned len, unsigned timeout)
{
    usb_serial_impl_t *serial = (usb_serial_impl_t*)base;

    if (serial == NULL || (para == NULL && len != 0)) {
        USB_ERROR("%s: invalid serial or para\r\n", __func__);
        return -EINVAL;
    }

    int rv = libusb_control_transfer(serial->handler, 0b11000001,
            USB_REQ_GET_PARAMETER, para_idx,
            serial->base.interface_idx, para, len, timeout);
    if (rv < 0) {
        USB_ERROR("%s: transfer failed: %s\r\n", __func__,
                libusb_strerror((enum libusb_error)rv));
        return -EPERM;
    } else if ((unsigned)rv != len) {
        USB_ERROR("%s: para len request(%d) but return(%d)\r\n", __func__,
                len, rv);
        return -EPERM;
    }

    return 0;
}

int usb_serial_setpara(usb_serial_t *base, uint8_t para_idx,
        unsigned char *para, unsigned len, unsigned timeout)
{
    usb_serial_impl_t *serial = (usb_serial_impl_t*)base;

    if (base == NULL || para == NULL || len == 0) {
        USB_ERROR("%s: invalid serial or para\r\n", __func__);
        return -EINVAL;
    }

    int rv = libusb_control_transfer(serial->handler, 0b01000001,
            USB_REQ_SET_PARAMETER, para_idx,
            base->interface_idx, para, len, timeout);
    if (rv < 0) {
        USB_ERROR("%s: transfer failed: %s\r\n", __func__,
                libusb_strerror((enum libusb_error)rv));
        return -EPERM;
    } else if ((unsigned)rv != len) {
        USB_ERROR("%s: para len request(%d) but return(%d)\r\n", __func__,
                len, rv);
        return -EPERM;
    }

    return 0;
}

/* already hold mutex */
static libusb_transfer *get_control_xfr(usb_serial_async_impl_t *serial, unsigned len)
{
    libusb_transfer *xfr;

    xfr = libusb_alloc_transfer(0);
    if (xfr == NULL) {
        USB_ERROR("%s: alloc transfer failed\r\n", __func__);
        return NULL;
    }

    uint8_t *buf = (uint8_t*)malloc(8 + len);
    if (buf == NULL) {
        USB_ERROR("%s: not enough memory\r\n", __func__);
        libusb_free_transfer(xfr);
        return NULL;
    }

    xfr->buffer = buf;
    return xfr;
}

static int submit_control_xfr(usb_serial_async_impl_t *serial, libusb_transfer *xfr)
{
    libusb_fill_control_transfer(xfr, serial->base_impl.handler,
            xfr->buffer, usb_control_cb, serial, 0);

    int rv = libusb_submit_transfer(xfr);
    if (rv) {
        USB_ERROR("%s: submit transfer failed: %s\r\n", __func__,
                libusb_strerror((enum libusb_error)rv));

        free(xfr->buffer);
        libusb_free_transfer(xfr);
        return -EPERM;
    }

    serial->control_xfr.push_back(xfr);
    serial->control_xfr_count++;
    return 0;
}

int usb_serial_async_enable(usb_serial_t *base, int enable)
{
    usb_serial_async_impl_t *serial = (usb_serial_async_impl_t*)base;

    if (base == NULL) {
        USB_ERROR("%s: invalid serial\r\n", __func__);
        return -EINVAL;
    }

    if (!base->is_async) {
        USB_ERROR("%s: it is not a async port\r\n", __func__);
        return -EINVAL;
    }

    pthread_mutex_lock(&serial->mutex);

    libusb_transfer *xfr = get_control_xfr(serial, 0);
    if (xfr == NULL) {
        pthread_mutex_unlock(&serial->mutex);
        USB_ERROR("%s: get control xfr failed\r\n", __func__);
        return -ENOMEM;
    }

    libusb_fill_control_setup(xfr->buffer, 0b01000001,
            (enable ? USB_REQ_SET_FEATURE : USB_REQ_CLEAR_FEATURE),
            USB_FEATURE_ENABLE, base->interface_idx, 0);

    int rv = submit_control_xfr(serial, xfr);
    if (rv) {
        USB_ERROR("%s: submit control xfr failed\r\n", __func__);
    }

    pthread_mutex_unlock(&serial->mutex);
    return rv;
}

int usb_serial_async_getpara(usb_serial_t *base, uint8_t para_idx, unsigned len)
{
    usb_serial_async_impl_t *serial = (usb_serial_async_impl_t*)base;

    if (serial == NULL || len == 0) {
        USB_ERROR("%s: invalid serial or len\r\n", __func__);
        return -EINVAL;
    }

    if (!base->is_async) {
        USB_ERROR("%s: it is not a async port\r\n", __func__);
        return -EINVAL;
    }

    pthread_mutex_lock(&serial->mutex);

    libusb_transfer *xfr = get_control_xfr(serial, len);
    if (xfr == NULL) {
        pthread_mutex_unlock(&serial->mutex);
        USB_ERROR("%s: get control xfr failed\r\n", __func__);
        return -ENOMEM;
    }

    libusb_fill_control_setup(xfr->buffer, 0b11000001,
            USB_REQ_GET_PARAMETER, para_idx,
            base->interface_idx, len);

    int rv = submit_control_xfr(serial, xfr);
    if (rv) {
        USB_ERROR("%s: submit control xfr failed\r\n", __func__);
    }

    pthread_mutex_unlock(&serial->mutex);
    return rv;
}

int usb_serial_async_setpara(usb_serial_t *base, uint8_t para_idx,
        unsigned char *para, unsigned len)
{
    usb_serial_async_impl_t *serial = (usb_serial_async_impl_t*)base;

    if (base == NULL || para == NULL || len == 0) {
        USB_ERROR("%s: invalid serial or para\r\n", __func__);
        return -EINVAL;
    }

    if (!base->is_async) {
        USB_ERROR("%s: it is not a async port\r\n", __func__);
        return -EINVAL;
    }

    pthread_mutex_lock(&serial->mutex);

    libusb_transfer *xfr = get_control_xfr(serial, len);
    if (xfr == NULL) {
        pthread_mutex_unlock(&serial->mutex);
        USB_ERROR("%s: get control xfr failed\r\n", __func__);
        return -ENOMEM;
    }

    libusb_fill_control_setup(xfr->buffer, 0b01000001,
            USB_REQ_SET_PARAMETER, para_idx,
            base->interface_idx, len);

    memcpy(libusb_control_transfer_get_data(xfr), para, len);

    int rv = submit_control_xfr(serial, xfr);
    if (rv) {
        USB_ERROR("%s: submit control xfr failed\r\n", __func__);
    }

    pthread_mutex_unlock(&serial->mutex);
    return rv;
}

/* used in control cb to distinguish whether it is a enable control request
 * @enable: if not null and it's a enable request, return enable value
 * @return: true is it's a enable request
 */
bool usb_serial_async_is_enable(uint8_t *buffer, bool *enable)
{
    uint8_t request;
    uint16_t idx;

    if (buffer == NULL) {
        USB_ERROR("%s: null argument\r\n", __func__);
        return false;
    }

    buffer -= 8;
    request = buffer[1];
    idx = libusb_le16_to_cpu(*(uint16_t*)(buffer + 2));

    if (request != USB_REQ_SET_FEATURE && request != USB_REQ_CLEAR_FEATURE)
        return false;

    if (idx != USB_FEATURE_ENABLE)
        return false;

    if (enable)
        *enable = request == USB_REQ_SET_FEATURE;

    return true;
}

/* used in control cb to distinguish whether it is a get/set para control request
 * @is_get: if not null and it's a get/set para request, return whether it's get
 * @return: true is it's a get/set para request
 */
int usb_serial_async_is_getsetpara(uint8_t *buffer, bool *is_get)
{
    uint8_t request;
    uint16_t idx;

    if (buffer == NULL) {
        USB_ERROR("%s: null argument\r\n", __func__);
        return -EINVAL;
    }

    buffer -= 8;
    request = buffer[1];
    idx = libusb_le16_to_cpu(*(uint16_t*)(buffer + 2));

    if (request != USB_REQ_SET_PARAMETER && request != USB_REQ_GET_PARAMETER)
        return -EINVAL;

    if (is_get)
        *is_get = request == USB_REQ_GET_PARAMETER;

    return idx;
}

int usb_serial_read(usb_serial_t *base, unsigned char *buf, unsigned len, unsigned timeout)
{
    usb_serial_impl_t *serial = (usb_serial_impl_t*)base;

    if (serial == NULL || (buf == NULL && len != 0)) {
        USB_ERROR("%s: invalid serial or buf\r\n", __func__);
        return -EINVAL;
    }

    int byteDone;
    int rv = libusb_bulk_transfer(serial->handler, serial->ep_in, buf, len, &byteDone, timeout);
    if (rv == LIBUSB_ERROR_TIMEOUT) {
        USB_VERBOS("%s: timeout request bytes(%d), done bytes(%d)\r\n", __func__,
                len, byteDone);
    } else if (rv) {
        USB_ERROR("%s: transfer failed: %s\r\n", __func__,
                libusb_strerror((enum libusb_error)rv));
        return -EPERM;
    }

    return byteDone;
}

int usb_serial_write(usb_serial_t *base, unsigned char *buf, unsigned len, unsigned timeout)
{
    usb_serial_impl_t *serial = (usb_serial_impl_t*)base;

    if (serial == NULL || (buf == NULL && len != 0)) {
        USB_ERROR("%s: invalid serial or buf\r\n", __func__);
        return -EINVAL;
    }

    int byteDone;
    int rv = libusb_bulk_transfer(serial->handler, serial->ep_out, buf, len, &byteDone, timeout);
    if (rv == LIBUSB_ERROR_TIMEOUT) {
        USB_VERBOS("%s: timeout(%d) request bytes(%d), done bytes(%d)\r\n", __func__,
                timeout, len, byteDone);
        return byteDone;
    } else if (rv) {
        USB_ERROR("%s: transfer failed: %s\r\n", __func__,
                libusb_strerror((enum libusb_error)rv));
        return -EPERM;
    }

    if (len && len % 64 == 0) {
        rv = libusb_bulk_transfer(serial->handler, serial->ep_out, buf, len, (int*)&len, timeout);
        if (rv == LIBUSB_ERROR_TIMEOUT) {
            USB_VERBOS("%s: padding frame timeout(%d)\r\n", __func__,
                    timeout);
        } else if (rv) {
            USB_ERROR("%s: transfer failed: %s\r\n", __func__,
                    libusb_strerror((enum libusb_error)rv));
            return -EPERM;
        }
    }

    return byteDone;
}
