/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * usbmstp_def.h
 * Original Author:  linzhixian, 2014-6-25
 *
 * USB MSTP
 *
 * History
 */

#ifndef _USBMSTP_DEF_H_
#define _USBMSTP_DEF_H_

#include <stdint.h>
#include <pthread.h>

#include "bacnet/mstp.h"
#include "misc/usbuartproxy.h"
#include "bacnet/bacnet_buf.h"

extern bool mstp_dbg_verbos;
extern bool mstp_dbg_warn;
extern bool mstp_dbg_err;

#define MSTP_ERROR(fmt, args...)                    \
do {                                                \
    if (mstp_dbg_err) {                             \
       printf(fmt, ##args);                         \
    }                                               \
} while (0)

#define MSTP_WARN(fmt, args...)                     \
do {                                                \
    if (mstp_dbg_warn) {                            \
        printf(fmt, ##args);                        \
    }                                               \
} while (0)

#define MSTP_VERBOS(fmt, args...)                   \
do {                                                \
    if (mstp_dbg_verbos) {                          \
       printf(fmt, ##args);                         \
    }                                               \
} while (0)

#define  MSTP_MAX_MASTER                (127)
#define  MSTP_BROADCAST_ADDRESS         (255)
#define  MSTP_MAX_DATA_LEN              (1497)
#define  MSTP_MAX_NE_DATA_LEN           (501)

#define  OUT_PREFIX_SPACE               (4)
#define  OUT_PACKET_HEADER              (3)
#define  OUT_MIN_PACKET_SIZE            (OUT_PACKET_HEADER + 1 + 2)
#define  OUT_MAX_PACKET_SIZE            (MSTP_MAX_DATA_LEN + 6 + 5 + OUT_PACKET_HEADER)
#define  OUT_MAX_NE_PACKET_SIZE         (MSTP_MAX_NE_DATA_LEN + 2 + OUT_PACKET_HEADER)

#define  IN_MAX_PACKET_SIZE             (MSTP_MAX_DATA_LEN + 6 + 2)

#define  MIN_BUFFER_SIZE                (4096)
#define  MAX_BUFFER_SIZE                (262144)

typedef struct usb_mstp_s {
    datalink_mstp_t base;
    usb_serial_t *serial;
    pthread_mutex_t mutex;
    uint8_t *out_buf;
    DECLARE_BACNET_BUF(*in_buf, MSTP_MAX_DATA_LEN);
    unsigned head;
    unsigned tail;
    unsigned not_used;
    unsigned in_xfr_size;
    uint16_t out_size[256];
    uint16_t left_space;
    uint16_t packet_queued;
    uint8_t recv_sn;
    uint8_t now_sn;
    uint8_t sent_sn;
    uint16_t reply_fast_timeout;
    uint16_t usage_fast_timeout;
    uint32_t conflictCount;
    uint32_t noReplyCount;
    uint32_t noTokenCount;
    uint32_t noPassCount;
    uint32_t sendFailCount;
    uint32_t errCount;
    uint32_t dupTokenCount;
    uint32_t noTurnCount;
    uint32_t paddingCount;

    uint8_t polarity;
    uint8_t auto_baud;
    uint8_t auto_polarity;
    uint8_t auto_mac;
    uint8_t auto_busy;
    uint8_t fast_nodes[32];

    struct {
        uint8_t *data;
        uint16_t len;
        uint8_t remote;
        uint8_t sn;
        bool sent;
        bool result;
        void (*callback) (void*, mstp_test_result_t);
        void *context;
    } test;
} usb_mstp_t;

#endif /* _USBMSTP_DEF_H_ */
