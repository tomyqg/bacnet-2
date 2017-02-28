/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * npdu.h
 * Original Author:  linzhixian, 2014-7-8
 *
 * BACnet�����Э�����ݵ�Ԫͷ�ļ�
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

/* �����Э�鱨������ */
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

/* �����Э��������Ϣ */
typedef struct npci_info_s {
    uint8_t version;                /* Э��汾�ţ�ȡ�̶�ֵ1 */
    uint8_t control;                /* �����ֶ� */
    uint8_t hop_count;              /* ·��ת�����������DNET���ڣ���ָ�������ҳ�ʼֵΪ0xFF */
    uint8_t msg_type;               /* �����Э�鱨�����ͣ�����������Э�鱨�ģ�����Ҫָ������ */
    uint16_t vendor_id;             /* ���̱�ʶ�����������������0x80��0xFFʱ����Ҫָ������ */
    uint16_t hop_count_offset;      /* ת��������ƫ���� */
    uint16_t nud_offset;            /* ���ݶ�ƫ���� */
    bacnet_addr_t dst;              /* ����Ŀ�ĵ�ַ��DNET�������Ҫָ������ */
    bacnet_addr_t src;              /* Դ��ַ��SNET�������Ҫָ������ */
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

