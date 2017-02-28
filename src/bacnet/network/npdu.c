/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * npdu.c
 * Original Author:  linzhixian, 2014-7-8
 *
 * BACnet网络层报文解析模块
 *
 * History
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "npdu.h"
#include "network_def.h"
#include "bacnet/bacint.h"
#include "misc/bits.h"

/**
 * npdu_get_npci_info - 构造网络层协议控制信息
 *
 * @pci: 返回协议控制信息
 * @dst: 目的网络地址
 * @src: 源网络地址
 * @prio: 网络优先级
 * @der: 是否需要应答
 * @type: 网络报文类型
 *
 * @return: 成功返回0，失败返回负数；
 *
 */
int npdu_get_npci_info(npci_info_t *pci, bacnet_addr_t *dst, bacnet_addr_t *src, 
        bacnet_prio_t prio, bool der, network_msg_type_t type)
{
    int len;
    
    if (pci == NULL) {
        NETWORK_ERROR("%s: invalid npdu_info point\r\n", __func__);
        return -EINVAL;
    }

    if (dst) {
        if (dst->len > MAX_MAC_LEN) {
            NETWORK_ERROR("%s: invalid dst_len(%d)\r\n", __func__, dst->len);
            return -EINVAL;
        }

        if ((dst->net == BACNET_BROADCAST_NETWORK) && (dst->len != 0)) {
            NETWORK_ERROR("%s: invalid global bcast npdu len(%d)\r\n", __func__, dst->len);
            return -EINVAL;
        }
    }

    if (src && (src->len > MAX_MAC_LEN)) {
        NETWORK_ERROR("%s: invalid src_len(%d)\r\n", __func__, src->len);
        return -EINVAL;
    }

    memset(pci, 0, sizeof(npci_info_t));
    pci->version = (uint8_t)NETWORK_PROTOCOL_VERSION;
    pci->hop_count = (uint8_t)DEFAULT_HOP_COUNT;
    pci->vendor_id = (uint16_t)DEFAULT_VENDOR_ID;
    
    pci->control |= (prio & 0x03);
    if (der == true) {
        pci->control |= BIT2;
    }

    len = 2;
    /* set src before dst, actually in header src is after dst */
    if (src && (src->net) && (src->len)) {
        pci->control |= BIT3;
        pci->src.net = src->net;
        pci->src.len = src->len;
        if (src->len) {
            memcpy(pci->src.adr, src->adr, src->len);
        }

        len += (3 + src->len);
    }

    if (dst && (dst->net)) {
        pci->control |= BIT5;
        pci->dst.net = dst->net;
        pci->dst.len = dst->len;
        if (dst->len) {
            memcpy(pci->dst.adr, dst->adr, dst->len);
        }

        pci->hop_count_offset = len + 3 + dst->len;
        len = pci->hop_count_offset + 1;
    }

    if (NETWORK_MESSAGE_TYPE_IS_VALID(type)) {
        pci->control |= BIT7;
        pci->msg_type = (uint8_t)type;
        len++;
        if (type >= 0x80) {
            len += 2;
        }
    } else {
        pci->msg_type = (uint8_t)INVALID_NETWORK_MESSAGE_TYPE;
    }

    pci->nud_offset = len;

    return OK;
}

/**
 * npdu_encode_pci - 封装网络层协议控制信息NPCI
 *
 * @npdu: 返回封装的网络层协议数据单元
 * @pci: 协议控制信息
 *
 * @return: 成功返回0，失败返回负数；
 *
 */
int npdu_encode_pci(bacnet_buf_t *npdu, npci_info_t *pci)
{
    uint8_t *pdu;
    int len;
    int i;

    if ((pci == NULL) || (npdu == NULL) || (npdu->data == NULL) 
            || (npdu->data_len <= pci->nud_offset)) {
        NETWORK_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    pdu = npdu->data;
    pdu[0] = pci->version;
    pdu[1] = pci->control;

    len = 2;

    /* 填充DNET、DLEN、DADR */
    if (pci->control & BIT5) {
        len += encode_unsigned16(&pdu[len], pci->dst.net);
        pdu[len++] = pci->dst.len;
        
        for (i = 0; i < pci->dst.len; i++) {
            pdu[len++] = pci->dst.adr[i];
        }
    }

    /* 填充SNET、SLEN、SADR，SLEN = 0 表示无效 */
    if (pci->control & BIT3) {
        len += encode_unsigned16(&pdu[len], pci->src.net);
        pdu[len++] = pci->src.len;
        for (i = 0; i < pci->src.len; i++) {
            pdu[len++] = pci->src.adr[i];
        }
    }

    /* 填充Hop Count */
    if (pci->control & BIT5) {
        pdu[len++] = pci->hop_count;
    }

    /* 填充Message Type */
    if (pci->control & BIT7) {
        pdu[len++] = pci->msg_type;
        if (pci->msg_type >= 0x80) {
            len += encode_unsigned16(&pdu[len], pci->vendor_id);       /* 填充Vendor ID */
        }
    }

    if (len != pci->nud_offset) {
        NETWORK_ERROR("%s: encode failed cause error pci\r\n", __func__);
        return -EPERM;
    }

    return OK;
}

/**
 * npdu_decode_pci - 网络协议数据单元解码
 *
 * @npdu: 待解码的网络协议数据单元
 * @pci: 返回网络协议控制信息
 *
 * @return: 成功返回0，失败返回负数
 *
 */
int npdu_decode_pci(bacnet_buf_t *npdu, npci_info_t *pci)
{
    uint8_t *pdu;
    uint16_t net;
    uint8_t adr_len;
    int len;

    if ((pci == NULL) || (npdu == NULL) || (npdu->data == NULL) || (npdu->data_len < 3)) {
        NETWORK_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    pdu = npdu->data;
    if (pdu[0] != NETWORK_PROTOCOL_VERSION) {
        NETWORK_ERROR("%s: unknown network protocol version(%d)\r\n", __func__, pdu[0]);
        return -EPERM;
    }
    
    memset(pci, 0, sizeof(npci_info_t));
    pci->version = pdu[0];
    pci->control = pdu[1];
    len = 2;

    /* 解析DNET/DLEN/DADR */
    if (pdu[1] & BIT5) {
        len += decode_unsigned16(&pdu[len], &net);
        if (net == 0) {
            NETWORK_ERROR("%s: invalid DNET(%d)\r\n", __func__, net);
            return -EPERM;
        }
        
        pci->dst.net = net;
        pci->dst.len = pdu[len++];
        adr_len = pci->dst.len;
        if (adr_len) {
            if (adr_len > MAX_MAC_LEN) {
                NETWORK_ERROR("%s: invalid DLEN(%d)\r\n", __func__, adr_len);
                return -EPERM;
            }

            if (net == BACNET_BROADCAST_NETWORK) {
                NETWORK_ERROR("%s: invalid global bcast npdu DLEN(%d)\r\n", __func__, adr_len);
                return -EPERM;
            }
            
            memcpy(pci->dst.adr, &pdu[len], adr_len);
            len += adr_len;
        }
    } else {
        pci->dst.net = 0;
        pci->dst.len = 0;
        memset(pci->dst.adr, 0, MAX_MAC_LEN);
    }

    /* 解析SNET/SLEN/SADR */
    if (pdu[1] & BIT3) {
        len += decode_unsigned16(&pdu[len], &net);
        if ((net == 0) || (net == BACNET_BROADCAST_NETWORK)) {
            NETWORK_ERROR("%s: invalid SNET(%d)\r\n", __func__, net);
            return -EPERM;
        }
        
        pci->src.net = net;
        pci->src.len = pdu[len++];
        adr_len = pci->src.len;
        if ((adr_len == 0) || (adr_len > MAX_MAC_LEN)) {
            NETWORK_ERROR("%s: invalid SLEN(%d)\r\n", __func__, adr_len);
            return -EPERM;
        }

        memcpy(pci->src.adr, &pdu[len], adr_len);
        len += adr_len;
    } else {
        pci->src.net = 0;
        pci->src.len = 0;
        memset(pci->src.adr, 0, MAX_MAC_LEN);
    }

    /* 解析Hop count */
    if (pdu[1] & BIT5) {
        pci->hop_count_offset = len;
        pci->hop_count = pdu[len++];
    } else {
        pci->hop_count = 0;
    }

    /* 解析Message Type */
    if (pdu[1] & BIT7) {
        pci->msg_type = pdu[len++];
        if (pci->msg_type >= 0x80) {
            len += decode_unsigned16(&pdu[len], &(pci->vendor_id));
        }
    } else {
        pci->msg_type = (uint8_t)INVALID_NETWORK_MESSAGE_TYPE;
    }

    if (npdu->data_len < len) {
        NETWORK_ERROR("%s: invalid npdu_len(%d)\r\n", __func__, npdu->data_len);
        return -EPERM;
    }

    pci->nud_offset = len;
    
    return OK;
}

/**
 * npdu_get_src_address - 获取NPDU的原始网络地址
 *
 * @npdu: 待解码的网络协议数据单元
 * @src: 返回源网络地址
 *
 * @return: 成功返回0，失败返回负数
 *
 */
int npdu_get_src_address(bacnet_buf_t *npdu, bacnet_addr_t *src)
{
    uint8_t *pdu;
    int src_offset;
    
    if ((src == NULL) || (npdu == NULL) || (npdu->data == NULL) || (npdu->data_len == 0)) {
        NETWORK_ERROR("%s: invalid argument\r\n", __func__);
        return -EPERM;
    }

    pdu = npdu->data;
    if (pdu[0] != NETWORK_PROTOCOL_VERSION) {
        NETWORK_ERROR("%s: invalid network version(%d)\r\n", __func__, pdu[0]);
        return -EPERM;
    }

    if (pdu[1] & BIT3) {
        src_offset = 2;
        if (pdu[1] & BIT5) {
            src_offset =  5 + pdu[4];
        }

        src_offset += decode_unsigned16(&pdu[src_offset], &(src->net));
        if (pdu[src_offset] > MAX_MAC_LEN) {
            NETWORK_ERROR("%s: invalid SLEN(%d)\r\n", __func__, pdu[src_offset]);
            return -EPERM;
        }
        
        src->len = pdu[src_offset];
        src_offset++;
        memcpy(src->adr, &pdu[src_offset], src->len);

        return OK;
    }

    NETWORK_ERROR("%s: SRC field is absent\r\n", __func__);
    
    return -EPERM;    
}

/**
 * npdu_get_dst_address - 获取NPDU的最终目的网络地址
 *
 * @npdu: 待解码的网络协议数据单元
 * @dst: 返回最终目的网络地址
 *
 * @return: 成功返回0，失败返回负数
 *
 */
int npdu_get_dst_address(bacnet_buf_t *npdu, bacnet_addr_t *dst)
{
    uint8_t *pdu;
    int dst_offset;
    
    if ((dst == NULL) || (npdu == NULL) || (npdu->data == NULL) || (npdu->data_len == 0)) {
        NETWORK_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    pdu = npdu->data;
    if (pdu[0] != NETWORK_PROTOCOL_VERSION) {
        NETWORK_ERROR("%s: invalid network version(%d)\r\n", __func__, pdu[0]);
        return -EPERM;
    }

    if (pdu[1] & BIT5) {
        dst_offset = 2;
        dst_offset += decode_unsigned16(&pdu[dst_offset], &(dst->net));
        if (pdu[dst_offset] > MAX_MAC_LEN) {
            NETWORK_ERROR("%s: invalid DLEN(%d)\r\n", __func__, pdu[dst_offset]);
            return -EPERM;
        }
        
        dst->len = pdu[dst_offset];
        dst_offset++;
        memcpy(dst->adr, &pdu[dst_offset], dst->len);

        return OK;
    }

    NETWORK_ERROR("%s: DST field is absent\r\n", __func__);

    return -EPERM;
}

/**
 * npdu_add_src_field - 添加报文源域
 *
 * @npdu: 待添加源域的网络层报文
 * @src: 待添加的源网络地址
 *
 * @return: 成功返回0，失败返回负数
 *
 */
int npdu_add_src_field(bacnet_buf_t *npdu, bacnet_addr_t *src)
{
    uint8_t *old_pdu;
    uint8_t *new_pdu;
    int offset;
    int rv;

    if ((src == NULL) || (src->len > MAX_MAC_LEN)) {
        NETWORK_ERROR("%s: invalid src_address\r\n", __func__);
        return -EINVAL;
    }
    
    if ((npdu == NULL) || (npdu->data == NULL) || (npdu->data_len <= MIN_NPCI_LEN)) {
        NETWORK_ERROR("%s: invalid npdu\r\n", __func__);
        return -EINVAL;
    }

    old_pdu = npdu->data;
    if (old_pdu[0] != NETWORK_PROTOCOL_VERSION) {
        NETWORK_ERROR("%s: invalid network version(%d)\r\n", __func__, old_pdu[0]);
        return -EINVAL;
    }

    if (old_pdu[1] & BIT3) {
        NETWORK_ERROR("%s: SRC Field is already present\r\n", __func__);
        return -EPERM;
    }

    offset = 2;
    if (old_pdu[1] & BIT5) {
        offset = 5 + old_pdu[4];
    }

    rv = bacnet_buf_push(npdu, 3 + src->len);
    if (rv < 0) {
        NETWORK_ERROR("%s: buf push failed(%d)\r\n", __func__, rv);
        return rv;
    }

    new_pdu = npdu->data;
    memcpy(new_pdu, old_pdu, offset);
    offset += encode_unsigned16(&new_pdu[offset], src->net);
    new_pdu[offset++] = src->len;
    memcpy(&new_pdu[offset], src->adr, src->len);

    new_pdu[1] |= BIT3;

    return OK;
}

/**
 * npdu_remove_dst_field - 去掉报文的目的域
 *
 * @npdu: 待去除目的域的网络报文
 *
 *  Note: 去掉hop_count域
 *
 * @return: 成功返回0，失败返回负数
 *
 */
int npdu_remove_dst_field(bacnet_buf_t *npdu)
{
    uint8_t *pdu;
    uint16_t dst_field_len;
    uint16_t src_field_len;
    int src_offset;
    int rv;
    
    if ((npdu == NULL) || (npdu->data == NULL) || (npdu->data_len < 6)) {
        NETWORK_ERROR("%s: invalid npdu\r\n", __func__);
        return -EINVAL;
    }

    pdu = npdu->data;
    if (pdu[0] != NETWORK_PROTOCOL_VERSION) {
        NETWORK_ERROR("%s: invalid network version(%d)\r\n", __func__, pdu[0]);
        return -EINVAL;
    }
    
    if (!(pdu[1] & BIT5)) {
        NETWORK_ERROR("%s: DST field is already absent\r\n", __func__);
        return -EPERM;
    }

    if (pdu[4] > MAX_MAC_LEN) {
        NETWORK_ERROR("%s: invalid DLEN(%d)\r\n", __func__, pdu[4]);
        return -EPERM;
    }

    dst_field_len = 3 + pdu[4];
    src_offset = 2 + dst_field_len;

    src_field_len = 0;
    if (pdu[1] & BIT3) {
        if ((pdu[src_offset + 2] == 0) || (pdu[src_offset + 2] > MAX_MAC_LEN)) {
            NETWORK_ERROR("%s: invalid SLEN(%d)\r\n", __func__, pdu[src_offset + 2]);
            return -EINVAL;
        }
        
        src_field_len = 3 + pdu[src_offset + 2];
    }

    if (npdu->data_len <= src_offset + src_field_len + HOP_COUNT_LEN) {
        NETWORK_ERROR("%s: invalid npdu len(%d)\r\n", __func__, npdu->data_len);
	    return -EINVAL;
    }

    memmove(pdu + src_offset + 1, pdu + src_offset, src_field_len);
    pdu[src_offset - 1] = pdu[0];
    pdu[src_offset] = pdu[1] & (~BIT5);

    rv = bacnet_buf_pull(npdu, dst_field_len + HOP_COUNT_LEN);
    if (rv < 0) {
        NETWORK_ERROR("%s: buf pull failed(%d)\r\n", __func__, rv);
    }

    return rv;
}

