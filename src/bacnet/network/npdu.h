/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * npdu.h
 * Original Author:  linzhixian, 2014-7-8
 *
 * BACnet网络层协议数据单元头文件
 *
 * History
 */

#ifndef _NPDU_H_
#define _NPDU_H_

#include "bacnet/bacdef.h"
#include "bacnet/bacnet_buf.h"

#define MIN_NPCI_LEN                    (2)
#define MAX_NPCI_LEN                    (12 + MAX_MAC_LEN + MAX_MAC_LEN)
#define NETWORK_PROTOCOL_VERSION        (1)
#define HOP_COUNT_LEN                   (1)
#define DEFAULT_HOP_COUNT               (255)
#define DEFAULT_VENDOR_ID               (0)

/* 网络层协议报文类型 */
typedef enum network_msg_type_e {
    WHO_IS_ROUTER_TO_NETWORK = 0,
    I_AM_ROUTER_TO_NETWORK = 1,
    I_COULD_BE_ROUTER_TO_NETWORK = 2,
    REJECT_MESSAGE_TO_NETWORK = 3,
    ROUTER_BUSY_TO_NETWORK = 4,
    ROUTER_AVAILABLE_TO_NETWORK = 5,
    INITIALIZE_ROUTING_TABLE = 6,
    INITIALIZE_ROUTING_TABLE_ACK = 7,
    ESTABLISH_CONNECTION_TO_NETWORK = 8,
    DISCONNECT_CONNECTION_TO_NETWORK = 9,
    /* X'0A' to X'7F': Reserved for use by ASHRAE, */
    /* X'80' to X'FF': Available for vendor proprietary messages */
    INVALID_NETWORK_MESSAGE_TYPE = 0x100
} network_msg_type_t;

/* The Network Reject Reasons for NETWORK_MESSAGE_REJECT_MESSAGE_TO_NETWORK */
typedef enum network_reject_reason_s {
    NETWORK_REJECT_UNKNOWN_ERROR = 0,
    NETWORK_REJECT_NO_ROUTE = 1,
    NETWORK_REJECT_ROUTER_BUSY = 2,
    NETWORK_REJECT_UNKNOWN_MESSAGE_TYPE = 3,
    NETWORK_REJECT_MESSAGE_TOO_LONG = 4,
    /* Reasons this value or above we don't know about */
    NETWORK_REJECT_REASON_INVALID
} network_reject_reason_t;

#define NETWORK_MESSAGE_TYPE_IS_VALID(type)     \
        (((type) >= WHO_IS_ROUTER_TO_NETWORK) && ((type) < INVALID_NETWORK_MESSAGE_TYPE))

/* 网络层协议数据信息 */
typedef struct npci_info_s {
    uint8_t version;                /* 协议版本号，取固定值1 */
    uint8_t control;                /* 控制字段 */
    uint8_t hop_count;              /* 路由转发计数，如果DNET存在，需指定该域，且初始值为0xFF */
    uint8_t msg_type;               /* 网络层协议报文类型，如果是网络层协议报文，则需要指定该域 */
    uint16_t vendor_id;             /* 厂商标识，如果报文类型域在0x80至0xFF时，需要指定该域 */
    uint16_t hop_count_offset;      /* 转发计数的偏移量 */
    uint16_t nud_offset;            /* 数据段偏移量 */
    bacnet_addr_t dst;              /* 最终目的地址，DNET域存在需要指定该域 */
    bacnet_addr_t src;              /* 源地址，SNET域存在需要指定该域 */
} npci_info_t;

extern int encode_unsigned16(uint8_t *pdu, uint16_t value);

extern int decode_unsigned16(const uint8_t *pdu, uint16_t *value);

extern int npdu_get_npci_info(npci_info_t *pci, bacnet_addr_t *dst, bacnet_addr_t *src, 
            bacnet_prio_t prio, bool der, network_msg_type_t type);

extern int npdu_encode_pci(bacnet_buf_t *npdu, npci_info_t *pci);

extern int npdu_decode_pci(bacnet_buf_t *npdu, npci_info_t *pci);

extern int npdu_get_src_address(bacnet_buf_t *npdu, bacnet_addr_t *src);

extern int npdu_get_dst_address(bacnet_buf_t *npdu, bacnet_addr_t *dst);

extern int npdu_add_src_field(bacnet_buf_t *npdu, bacnet_addr_t *src);

extern int npdu_remove_dst_field(bacnet_buf_t *npdu);

#endif  /* _NPDU_H_ */

