/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bip_def.h
 * Original Author:  linzhixian, 2014-10-10
 *
 * BACNET_IP内部头文件
 *
 * History
 */

#ifndef _BIP_DEF_H_
#define _BIP_DEF_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <netinet/in.h>
#include <net/if.h>

#include "bacnet/bip.h"
#include "misc/list.h"
#include "bacnet/datalink.h"
#include "misc/hashtable.h"

extern bool bip_dbg_verbos;
extern bool bip_dbg_warn;
extern bool bip_dbg_err;

#define BIP_ERROR(fmt, args...)                     \
do {                                                \
    if (bip_dbg_err) {                              \
       printf(fmt, ##args);                         \
    }                                               \
} while (0)

#define BIP_WARN(fmt, args...)                      \
do {                                                \
    if (bip_dbg_warn) {                             \
        printf(fmt, ##args);                        \
    }                                               \
} while (0)

#define BIP_VERBOS(fmt, args...)                    \
do {                                                \
    if (bip_dbg_verbos) {                           \
       printf(fmt, ##args);                         \
    }                                               \
} while (0)

typedef struct bvlc_hdr_s {
    uint8_t type;
    uint8_t function;
    uint16_t length;
} bvlc_hdr_t;

#define BVLC_HDR_LEN                                (sizeof(bvlc_hdr_t))
#define BVLC_DATA_OFFSET                            (BVLC_HDR_LEN)
#define IP_ADDRESS_LEN                              (4)
#define UDP_PORT_LEN                                (2)
#define BIP_ADDRESS_LEN                             (IP_ADDRESS_LEN + UDP_PORT_LEN)
#define FORWARDED_NPDU_HDR_LEN                      (BVLC_HDR_LEN + BIP_ADDRESS_LEN)
#define BVLC_DEL_FDT_ENTRY_SIZE                     (BVLC_HDR_LEN + BIP_ADDRESS_LEN)
#define BCAST_MASK_LEN                              (4)
#define BVLL_TYPE_BACNET_IP                         (0x81)
#define BIP_MAX_DATA_LEN                            (1497)
#define BIP_DEFAULT_UDP_PORT                        (0xBAC0)
#define BIP_RX_BUFF_LEN                             (BVLC_HDR_LEN + BIP_MAX_DATA_LEN)
#define BBMD_TABLE_ENTRY_SIZE                       (BIP_ADDRESS_LEN + BCAST_MASK_LEN)
#define FD_TABLE_ENTRY_SIZE                         (10)
#define FDT_MAX_SIZE                                (BIP_MAX_DATA_LEN/FD_TABLE_ENTRY_SIZE)
#define BDT_MAX_SIZE                                (BIP_MAX_DATA_LEN/BBMD_TABLE_ENTRY_SIZE)
#define FDT_HASH_BIT                                (5)

typedef enum {
    BVLC_RESULT_SUCCESSFUL_COMPLETION = 0x0000,
    BVLC_RESULT_WRITE_BROADCAST_DISTRIBUTION_TABLE_NAK = 0x0010,
    BVLC_RESULT_READ_BROADCAST_DISTRIBUTION_TABLE_NAK = 0x0020,
    BVLC_RESULT_REGISTER_FOREIGN_DEVICE_NAK = 0X0030,
    BVLC_RESULT_READ_FOREIGN_DEVICE_TABLE_NAK = 0x0040,
    BVLC_RESULT_DELETE_FOREIGN_DEVICE_TABLE_ENTRY_NAK = 0x0050,
    BVLC_RESULT_DISTRIBUTE_BROADCAST_TO_NETWORK_NAK = 0x0060
} BACNET_BVLC_RESULT;

typedef enum {
    BVLC_RESULT = 0,
    BVLC_WRITE_BROADCAST_DISTRIBUTION_TABLE = 1,
    BVLC_READ_BROADCAST_DISTRIBUTION_TABLE = 2,
    BVLC_READ_BROADCAST_DISTRIBUTION_TABLE_ACK = 3,
    BVLC_FORWARDED_NPDU = 4,
    BVLC_REGISTER_FOREIGN_DEVICE = 5,
    BVLC_READ_FOREIGN_DEVICE_TABLE = 6,
    BVLC_READ_FOREIGN_DEVICE_TABLE_ACK = 7,
    BVLC_DELETE_FOREIGN_DEVICE_TABLE_ENTRY = 8,
    BVLC_DISTRIBUTE_BROADCAST_TO_NETWORK = 9,
    BVLC_ORIGINAL_UNICAST_NPDU = 10,
    BVLC_ORIGINAL_BROADCAST_NPDU = 11,
    MAX_BVLC_FUNCTION = 12
} BACNET_BVLC_FUNCTION;

typedef struct bdt_entry_s {
    struct in_addr dst_addr;                /* in network format */
    uint16_t dst_port;                      /* not always 0xBAC0, in network format */
    struct in_addr bcast_mask;              /* Broadcast distribution mask, in network format */
} bdt_entry_t;

typedef struct bdt_push_s {
    uint32_t interval;
    uint32_t each_ms;
    int idx;
    el_timer_t *timer;
} bdt_push_t;

typedef struct bbmd_data_s {
    pthread_rwlock_t fdt_lock;
    uint32_t fdt_size;
    DECLARE_HASHTABLE(fdt, FDT_HASH_BIT);

    int local_entry;
    pthread_rwlock_t bdt_lock;
    uint32_t bdt_size;
    bdt_entry_t *bdt;
    bdt_push_t push;
} bbmd_data_t;

typedef struct fdt_entry_s {
    struct in_addr dst_addr;                /* in network format */
    uint16_t dst_port;                      /* not always 0xBAC0, in network format */
    uint16_t time_to_live;                  /* seconds for valid entry lifetime */
    el_timer_t *timer;
    struct hlist_node hnode;
    bbmd_data_t *bbmd;
} fdt_entry_t;

typedef struct fd_client_s {
    el_timer_t *timer;
    uint16_t ttl;                           /* fd time to live when register */
    uint16_t interval;                      /* interval to re-register fd */
    struct sockaddr_in remote;              /* valid if fd_enable is true */
} fd_client_t;

extern int encode_unsigned16(uint8_t *pdu, uint16_t value);

extern int decode_unsigned16(const uint8_t *pdu, uint16_t *value);

extern int bvlc_bdt_forward_npdu(datalink_bip_t *bip, struct sockaddr_in *src, bacnet_buf_t *npdu);

extern int bvlc_fdt_forward_npdu(datalink_bip_t *bip, struct sockaddr_in *src, bacnet_buf_t *npdu);

extern int bvlc_send_read_bdt(datalink_bip_t *bip, struct sockaddr_in *dst_bbmd);

extern int bvlc_send_write_bdt(datalink_bip_t *bip, struct sockaddr_in *dst_bbmd, bdt_entry_t *bdt,
            size_t len);

extern int bvlc_send_read_fdt(datalink_bip_t *bip, struct sockaddr_in *dst_bbmd);

extern int bvlc_receive_bvlc_result(datalink_bip_t *bip, bacnet_buf_t *mpdu);

extern int bvlc_receive_write_bdt(datalink_bip_t *bip, struct sockaddr_in *src, bacnet_buf_t *mpdu);

extern int bvlc_receive_read_bdt(datalink_bip_t *bip, struct sockaddr_in *src, bacnet_buf_t *mpdu);

extern int bvlc_receive_read_bdt_ack(datalink_bip_t *bip, bacnet_buf_t *mpdu);

extern int bvlc_receive_forwarded_npdu(datalink_bip_t *bip, struct sockaddr_in *src, 
            bacnet_buf_t *mpdu);

extern int bvlc_receive_register_foreign_device(datalink_bip_t *bip, struct sockaddr_in *src,
            bacnet_buf_t *mpdu);

extern int bvlc_receive_read_fdt(datalink_bip_t *bip, struct sockaddr_in *src, bacnet_buf_t *mpdu);

extern int bvlc_receive_read_fdt_ack(datalink_bip_t *bip, bacnet_buf_t *mpdu);

extern int bvlc_receive_delete_fdt_entry(datalink_bip_t *bip, struct sockaddr_in *src, 
            bacnet_buf_t *mpdu);

extern int bvlc_receive_distribute_bcast_to_network(datalink_bip_t *bip, struct sockaddr_in *src,
            bacnet_buf_t *mpdu);

extern int bvlc_receive_original_broadcast_npdu(datalink_bip_t *bip, struct sockaddr_in *src,
            bacnet_buf_t *mpdu);

extern int bvlc_register_with_bbmd(datalink_bip_t *bip, struct sockaddr_in *remote_bbmd,
            uint16_t ttl_seconds);

extern bbmd_data_t *bbmd_create(void);

extern void bbmd_destroy(bbmd_data_t *bbmd);

extern void bdt_push_start(datalink_bip_t *bip);

extern void bdt_push_stop(datalink_bip_t *bip);

#endif  /* _BIP_DEF_H_ */

