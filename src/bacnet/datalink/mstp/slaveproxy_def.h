/*
 * Copyright(C) 2014 SWG. All rights reserved.
 *
 * slaveproxy_def.h
 * Original Author:  lincheng, 2015-5-28
 *
 * BACnet mstp slave proxy
 *
 * History
 */

#ifndef _SLAVEPROXY_DEF_H_
#define _SLAVEPROXY_DEF_H_

#include <pthread.h>

#include "bacnet/slaveproxy.h"
#include "bacnet/bacstr.h"
#include "bacnet/tsm.h"

extern int sp_dbg_verbos;
extern int sp_dbg_warn;
extern int sp_dbg_err;

#define SP_ERROR(fmt, args...)                      \
do {                                                \
    if (sp_dbg_err) {                               \
       printf(fmt, ##args);                         \
    }                                               \
} while (0)

#define SP_WARN(fmt, args...)                       \
do {                                                \
    if (sp_dbg_warn) {                              \
        printf(fmt, ##args);                        \
    }                                               \
} while (0)

#define SP_VERBOS(fmt, args...)                     \
do {                                                \
    if (sp_dbg_verbos) {                            \
       printf(fmt, ##args);                         \
    }                                               \
} while (0)

#define ERROR_ADDR(addr)                            \
do {                                                \
    if (sp_dbg_err) {                               \
        PRINT_BACNET_ADDRESS(addr);                 \
    }                                               \
} while (0)

#define WARN_ADDR(addr)                             \
do {                                                \
    if (sp_dbg_warn) {                              \
        PRINT_BACNET_ADDRESS(addr);                 \
    }                                               \
} while (0)

#define VERBOS_ADDR(addr)                           \
do {                                                \
    if (sp_dbg_verbos) {                            \
        PRINT_BACNET_ADDRESS(addr);                 \
    }                                               \
} while (0)

#define IAM_QUEUE_BITS      (4)
#define IAM_QUEUE_SIZE      (1 << IAM_QUEUE_BITS)

#define IAM_QUEUE_MASK      (IAM_QUEUE_SIZE - 1)
#define IAM_EACH_SECOND     (1)

#define MIN_SCAN_INTERVAL       (120)
#define DEFAULT_SCAN_INTERVAL   (300)


typedef struct mstp_slave_s {
    struct list_head que_node;      /* lists that have whois requests */
    struct rb_node rb_node;         /* 按device_id升序红黑树 */
    uint8_t port;                   /* which mstp port */
    uint8_t mac;
    uint8_t queue_head;             /* queue的头指针 */
    uint8_t queue_tail;             /* queue的尾指针 */
    uint16_t queue[IAM_QUEUE_SIZE]; /* whois requests */
} mstp_slave_t;

typedef struct node_scan_s {
    mstp_slave_t *slave;            /* not NULL时，表明代理生效 */

    uint8_t prev;
    uint8_t next;

    uint8_t manual : 1;
    uint8_t auto_scan : 1;
    uint8_t timeout : 1;
    uint8_t cancel : 1;
    BACNET_SEGMENTATION seg_support : 4;

    enum {
        SCAN_NOT_START = 0,
        SCAN_WHOIS_SUPPORT,         /* send rp of service support */
        SCAN_SEG_SUPPORT,           /* send rp of segmentation support */
        SCAN_VENDOR_ID,             /* send rp of vendor id */
        SCAN_MAX_APDU,              /* send rp of max_apdu */
    } status : 4;

    uint32_t device_id;
    uint16_t max_apdu;
    uint16_t vendor_id;
} node_scan_t;

typedef struct slave_proxy_port_s {
    datalink_mstp_t *mstp;
    uint8_t next_scan_mac;
    uint8_t proxy_enable;
    uint8_t auto_discovery;
    uint16_t net;
    uint16_t scan_interval;
    unsigned request_num;
    unsigned start_timestamp;
    node_scan_t nodes[256];         /* nodes[255] only work as a list head */
} slave_proxy_port_t;

extern int slave_proxy_startup(void);

extern void slave_proxy_stop(void);

extern slave_proxy_port_t *slave_proxy_port_create(datalink_mstp_t *mstp, cJSON *cfg);

extern void slave_proxy_port_delete(datalink_mstp_t *mstp_port);

typedef struct slave_proxy_manager_s {
    slave_proxy_port_t **ports;
    uint32_t port_number;
    pthread_mutex_t lock;
    struct list_head que_head;      /* list for queue not null slave */
    struct rb_root rb_head;         /* device_id 升序红黑树根 */
    el_timer_t *iam_timer;
    int invoke_id;
} slave_proxy_manager_t;

typedef union slave_scan_context_s {
    struct {
        uint16_t port_idx;
        uint8_t mac;
    };
    unsigned _u;
} slave_scan_context_t;

typedef struct slave_data_s {
    bool is_rpm;
    BACNET_BIT_STRING service_support;
    uint32_t device_id;
    BACNET_PROPERTY_ID property_id;
    uint32_t max_apdu;
    uint32_t vendor_id;
    BACNET_SEGMENTATION seg_support;
} slave_data_s_t;

extern cJSON* slave_proxy_get_mib (slave_proxy_port_t *port);

#endif /* _SLAVEPROXY_DEF_H_ */

