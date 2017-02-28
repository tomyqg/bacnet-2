/*
 * usbuartproxy.h
 *
 *  Created on: Oct 21, 2015
 *      Author: lin
 */

#ifndef _USBUARTPROXY_H_
#define _USBUARTPROXY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "misc/eventloop.h"

#define  USB_INTERFACE_SUBCLASS_485         (0x00)
#define  USB_INTERFACE_SUBCLASS_ACT         (0x01)
#define  USB_INTERFACE_SUBCLASS_GPIO        (0x02)
#define  USB_INTERFACE_SUBCLASS_NONE        (0x03)

#define  USB_INTERFACE_PROTOCOL_COMMON      (0x00)
#define  USB_INTERFACE_PROTOCOL_MSTP        (0x01)
#define  USB_INTERFACE_PROTOCOL_ACT         (0x02)
#define  USB_INTERFACE_PROTOCOL_GPIO        (0x03)
#define  USB_INTERFACE_PROTOCOL_IAP         (0x04)

#define  COMMON_MAX_BAUDRATE                (115200)
#define  COMMON_MIN_BAUDRATE                (1200)
#define  COMMON_MIN_TIMEOUT_BITS            (20)
#define  COMMON_MIN_MAX_PK_LEN              (32)
#define  COMMON_MODE_7BITS                  (1)
#define  COMMON_MODE_2STOPS                 (1<<1)
#define  COMMON_MODE_PARITY                 (1<<2)
#define  COMMON_MODE_PARITY_ODD             (1<<3)
#define  COMMON_MODE_HARD_LEN               (1<<4)
#define  COMMON_MODE_POLARITY               (1<<5)

#define  BASE_PARAMETER_IDX                 (0)
#define  MSTP_PTY_PARAMETER_IDX             (1)

typedef struct common_parameter_s {
    uint32_t baudrate;
    uint16_t timeout_bits;
    uint16_t mode;
    uint16_t max_pk_len;
    uint16_t rqst_idle_ms;
    uint16_t resp_idle_ms;
    uint16_t err_cont_ms;
} common_parameter_t;

typedef enum common_event_type {
    COMMON_EVENT_SENT = 0,
    COMMON_EVENT_ERROR,
    COMMON_EVENT_IDLE,
    COMMON_EVENT_RECV,
    COMMON_EVENT_CONT,
    COMMON_EVENT_NOASW,
    COMMON_EVENT_ERRCONT,
} COMMON_EVENT_TYPE;

typedef enum common_req_type {
    COMMON_REQ_SENT = 0,
    COMMON_REQ_LED,
} COMMON_REQ_TYPE;

typedef struct mstp_parameter_s {
    uint8_t  baudrate;
    uint8_t  mac;
    uint8_t  max_master;
    uint8_t  max_info_frames;
    uint16_t reply_timeout;
    uint16_t reply_fast_timeout;
    uint8_t  usage_timeout;
    uint8_t  polarity;
    uint8_t  auto_baud;
    uint8_t  auto_polarity;
    uint8_t  auto_mac;
    uint8_t  usage_fast_timeout;
} mstp_parameter_t;

typedef enum mstp_event_type {
    MSTP_EVENT_BACNET = 0,
    MSTP_EVENT_BACNET_BCST,
    MSTP_EVENT_TEST,
    MSTP_EVENT_INFO,
    MSTP_EVENT_PTY,
    MSTP_EVENT_DEBUG,
} MSTP_EVENT_TYPE;

typedef enum mstp_req_type {
    MSTP_REQ_BACNET = 0,
    MSTP_REQ_TEST,
    MSTP_REQ_PTY,
} MSTP_REQ_TYPE;

typedef struct usb_serial_s {
    uint8_t subclass;
    uint8_t protocol;
    uint8_t enumerated_idx;
    uint8_t interface_idx;
    uint8_t is_async;
} usb_serial_t;

/*
 * return true if need to re-submit transfer
 */
typedef bool (*usb_serial_callback) (unsigned long data, unsigned char *buf, unsigned len);

struct cJSON;
/*
 * return [{"index":0, "interface":[subclass, subclass]},
 * {"index":1, "interface":[subclass, subclass]}]
 */
extern struct cJSON* usb_serial_query(void);

extern usb_serial_t* usb_serial_create(uint8_t enumerated_idx, uint8_t interface_idx,
        uint8_t subclass, uint8_t protocol);

extern usb_serial_t *usb_serial_async_create(uint8_t enumerated_idx, uint8_t interface_idx,
        uint8_t subclass, uint8_t protocol, el_loop_t *el,
        usb_serial_callback read_callback, usb_serial_callback write_callback,
        usb_serial_callback control_callback,  unsigned long data);

/**
 * if usb serial is work in asynchronized mode, be careful to synchronized with
 * event loop
 */
extern void usb_serial_destroy(usb_serial_t *serial);

extern void usb_serial_cancel_async(usb_serial_t *serial);

/*
 * when read transfer completed, read_callback will be called, when callback
 * return, the transfer will be restart automatic
 */
extern int usb_serial_async_read(usb_serial_t *serial, unsigned char *buf, unsigned len);

extern int usb_serial_async_write(usb_serial_t *serial, unsigned char *buf, unsigned len);

extern int usb_serial_async_get_pending(usb_serial_t *serial, unsigned *read,
        unsigned *write, unsigned *control);

extern int usb_serial_enable(usb_serial_t *serial, int enable, unsigned timeout);

extern int usb_serial_getpara(usb_serial_t *serial, uint8_t para_idx,
        unsigned char *para, unsigned len, unsigned timeout);

extern int usb_serial_setpara(usb_serial_t *serial, uint8_t para_idx,
        unsigned char *para, unsigned len, unsigned timeout);

extern int usb_serial_async_enable(usb_serial_t *serial, int enable);

extern int usb_serial_async_getpara(usb_serial_t *serial, uint8_t para_idx,
        unsigned len);

extern int usb_serial_async_setpara(usb_serial_t *serial, uint8_t para_idx,
        unsigned char *para, unsigned len);

/* used in control cb to distinguish whether it is a enable control request
 * @enable: if not null and it's a enable request, return enable value
 * @return: true is it's a enable request
 */
extern bool usb_serial_async_is_enable(uint8_t *buffer, bool *enable);

/* used in control cb to distinguish whether it is a get/set para control request
 * @is_get: if not null and it's a get/set para request, return whether it's get
 * @return: < 0 if not get/set para, >= 0 para_idx
 */
extern int usb_serial_async_is_getsetpara(uint8_t *buffer, bool *is_get);

extern int usb_serial_read(usb_serial_t *serial, unsigned char *buf, unsigned len, unsigned timeout);

extern int usb_serial_write(usb_serial_t *serial, unsigned char *buf, unsigned len, unsigned timeout);

#ifdef __cplusplus
}
#endif

#endif /* _USBUARTPROXY_H_ */

