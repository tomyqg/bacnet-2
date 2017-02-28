/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * protocol.c
 * Original Author:  linzhixian, 2014-8-20
 *
 * ·��Э�鱨�Ĵ���ģ��
 *
 * History
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "bacnet/config.h"
#include "network_def.h"
#include "protocol.h"
#include "misc/bits.h"

extern bool is_bacnet_router;
extern int route_port_nums;
extern bacnet_port_t *route_ports;

/**
 * send_I_Am_Router_To_Network - ��ָ���˿ڷ���i am router to network����
 *
 * @port_id: ���Ͷ˿�
 * @dnet_list: �ɴ�������б�
 * @list_num: �ɴ�������б��С
 *
 * ���ع㲥����
 *
 * @return: �ɹ�����0��ʧ�ܷ��ظ���
 *
 */
int send_I_Am_Router_To_Network(uint32_t port_id, const uint16_t dnet_list[], uint32_t list_num)
{
    DECLARE_BACNET_BUF(npdu, MAX_APDU);
    npci_info_t npci_info;
    bacnet_port_t *port;
    uint32_t npdu_len;
    int len;
    int i;
    int rv;
 
    if ((port_id >= route_port_nums) || (dnet_list == NULL) || (list_num == 0)) {
        NETWORK_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    rv = npdu_get_npci_info(&npci_info, NULL, NULL, PRIORITY_NORMAL, false, I_AM_ROUTER_TO_NETWORK);
    if (rv < 0) {
        NETWORK_ERROR("%s: encode npci info failed(%d)\r\n", __func__, rv);
        return rv;
    }
    
    npdu_len = npci_info.nud_offset + (list_num * sizeof(uint16_t));
    bacnet_buf_init(&npdu.buf, MAX_APDU);
    npdu.buf.data_len = npdu_len;
 
    rv = npdu_encode_pci(&npdu.buf, &npci_info);
    if (rv < 0) {
        NETWORK_ERROR("%s: encode pci failed(%d)\r\n", __func__, rv);
        return rv;
    }

    len = npci_info.nud_offset;
    for (i = 0; i < list_num; i++) {
        len += encode_unsigned16(&(npdu.buf.data[len]), dnet_list[i]);
    }
 
    if (len > npdu_len) {
        NETWORK_ERROR("%s: invalid npdu len(%d)\r\n", __func__, len);
        return -EPERM;
    }

    port = &route_ports[port_id];
    rv = port->dl->send_pdu(port->dl, NULL, &npdu.buf, PRIORITY_NORMAL, false);
    NETWORK_VERBOS("%s: port(%d), rv = %d\r\n", __func__, port_id, rv);
    
    return rv;
}

/**
 * send_Who_Is_Router_To_Network - ��ָ���˿ڷ���who is router to network����
 *
 * @dst: ����ѯ��·������ַ
 * @port: ���Ͷ˿�
 * @dnet: Ŀ�������
 *
 * �ñ��Ŀ�����·�����ͷ�·�����������Բ�ѯ·�ɱ���Ϣ��
 * ��dstΪNULLʱ��ʾ���ع㲥�ñ��ģ������ʾ��ѯָ��·������·�ɱ�(ֻ�и�·�����Ա���Ӧ��)��
 * ��dnetΪ0ʱ��ʾ��ѯ���е�·�ɱ�������ʾ��ѯ����ָ��dnet��·�ɱ��
 *
 * @return: �ɹ�����0��ʧ�ܷ��ظ���
 *
 */
int send_Who_Is_Router_To_Network(uint32_t port_id, uint16_t dnet)
{
    DECLARE_BACNET_BUF(npdu, MIN_APDU);
    npci_info_t npci_info;
    bacnet_port_t *port;
    uint16_t nud_len;
    uint16_t nud_offset;
    int rv;

    if (port_id >= route_port_nums) {
        NETWORK_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }
    
    rv = npdu_get_npci_info(&npci_info, NULL, NULL, PRIORITY_NORMAL, false, 
        WHO_IS_ROUTER_TO_NETWORK);
    if (rv < 0) {
        NETWORK_ERROR("%s: encode npci info failed(%d)\r\n", __func__, rv);
        return rv;
    }

    nud_len = (dnet == 0)? 0: 2;
    nud_offset = npci_info.nud_offset;
    bacnet_buf_init(&npdu.buf, MIN_APDU);
    npdu.buf.data_len = nud_offset + nud_len;
    
    rv = npdu_encode_pci(&npdu.buf, &npci_info);
    if (rv < 0) {
        NETWORK_ERROR("%s: encode pci failed(%d)\r\n", __func__, rv);
        return rv;
    }

    if (dnet > 0) {
        (void)encode_unsigned16(&(npdu.buf.data[nud_offset]), dnet);
    }

    port = &route_ports[port_id];
    rv = port->dl->send_pdu(port->dl, NULL, &npdu.buf, PRIORITY_NORMAL, false);
    NETWORK_VERBOS("%s: port(%d), dnet(%d), rv = %d\r\n", __func__, port_id, dnet, rv);
    
    return rv;
}

/**
 * bcast_Who_Is_Router_To_Network - �㲥who is router to network����
 *
 * @ex_port: �㲥�ų��Ķ˿�
 * @dnet: ����ѯ��Ŀ�������
 *
 * @return: �ɹ�����0��ʧ�ܷ��ظ���
 *
 */
int bcast_Who_Is_Router_To_Network(bacnet_port_t *ex_port, uint16_t dnet)
{
    bacnet_port_t *port;
    int i;
    int rv;
    
    if ((dnet == 0) || (dnet == BACNET_BROADCAST_NETWORK)) {
        NETWORK_ERROR("%s: invalid dnet(%d)\r\n", __func__, dnet);
        return -EINVAL;
    }
 
    for (i = 0; i < route_port_nums; i++) {
        port = &(route_ports[i]);
        if ((port->valid) && (port != ex_port)) {
            rv = send_Who_Is_Router_To_Network(i, dnet);
            if (rv < 0) {
                NETWORK_ERROR("%s: port(%d) send faild(%d)\r\n", __func__, i, rv);
            }
        }
    }
 
    return OK;
}

/**
 * bcast_Who_Is_Router_To_Network - �㲥who is router to network����
 *
 * @ex_port: �㲥�ų��Ķ˿�, ����ΪNULL
 * @busy: Busy_To_Network or Available_To_Network
 * @dnet_list: ������б�
 * @list_num: ������б��С
 *
 * @return: �ɹ�����0��ʧ�ܷ��ظ���
 *
 */
extern int bcast_Router_Busy_or_Available_To_Network(bacnet_port_t *ex_port, bool busy,
            const uint16_t dnet_list[], uint32_t list_num)
{
    DECLARE_BACNET_BUF(npdu, MAX_APDU);
    npci_info_t npci_info;
    bacnet_port_t *port;
    uint32_t npdu_len;
    int len;
    int i;
    int rv;

    if ((ex_port == NULL) || (dnet_list == NULL) || (list_num == 0)) {
        NETWORK_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    rv = npdu_get_npci_info(&npci_info, NULL, NULL, PRIORITY_NORMAL, false,
        busy ? ROUTER_BUSY_TO_NETWORK : ROUTER_AVAILABLE_TO_NETWORK);
    if (rv < 0) {
        NETWORK_ERROR("%s: encode npci info failed(%d)\r\n", __func__, rv);
        return rv;
    }

    npdu_len = npci_info.nud_offset + (list_num * sizeof(uint16_t));
    bacnet_buf_init(&npdu.buf, MAX_APDU);
    npdu.buf.data_len = npdu_len;

    rv = npdu_encode_pci(&npdu.buf, &npci_info);
    if (rv < 0) {
        NETWORK_ERROR("%s: encode pci failed(%d)\r\n", __func__, rv);
        return rv;
    }

    len = npci_info.nud_offset;
    for (i = 0; i < list_num; i++) {
        len += encode_unsigned16(&(npdu.buf.data[len]), dnet_list[i]);
    }

    if (len > npdu_len) {
        NETWORK_ERROR("%s: invalid npdu len(%d)\r\n", __func__, len);
        return -EPERM;
    }

    for (i = 0; i < route_port_nums; i++) {
        port = &(route_ports[i]);
        if ((port->valid) && (port != ex_port)) {
            rv = port->dl->send_pdu(port->dl, NULL, &npdu.buf, PRIORITY_NORMAL, false);
            if (rv < 0) {
                NETWORK_ERROR("%s: port(%d) send faild(%d)\r\n", __func__, i, rv);
            }
        }
    }

    return OK;
}

/**
 * send_Reject_Message_To_Network - ��ָ���˿ڷ���reject message to network����
 *
 * @in_port: ���ܾ����ĵ����
 * @dst: Ŀ��������ַ��Ҳ���Ǳ��ܾ����ĵ�Դ��ַ
 * @dnet: ��ͨ��Ĳ��ɴ������
 * @reject_reason: �ܾ����ĵ�ԭ��
 *
 * dstΪ���ܾ����ĵ�Դ�豸�����ַ�����dstΪNULL����dst->net = 0���ʾ���ܾ����ĵ�Դ�豸�뱾·��
 * �豸��ͬһ���磬��ֻ���ڱ������port���б��ع㲥������Ҫ��ָ��Ŀ�������ַ��·�������ͣ�
 * ������ܾ��ı�����δ֪�����Э�鱨�ģ���dnetΪ������ֵ0��
 *
 * @return: �ɹ�����0��ʧ�ܷ��ظ���
 *
 */
int send_Reject_Message_To_Network(uint32_t in_port, bacnet_addr_t *dst, uint16_t dnet, 
        uint8_t reject_reason)
{
    DECLARE_BACNET_BUF(npdu, MIN_APDU);
    bacnet_addr_t next_mac;
    bacnet_port_t *port;
    route_entry_t entry;
    npci_info_t npci_info;
    bool direct_net;
    uint32_t npdu_len;
    uint32_t out_port;
    int len;
    int rv;
    
    if (is_bacnet_router == false) {
        return OK;
    }
 
    direct_net = false;
    if ((dst == NULL) || (dst->net == 0)) {
        direct_net = true;
        out_port = in_port;
    } else {
        if ((!route_find_entry(dst->net, &entry)) || (entry.port == NULL)) {
            direct_net = true;
            out_port = in_port;
        } else {
            direct_net = entry.direct_net;
            out_port = entry.port->id;
        }
    }

    if (out_port >= route_port_nums) {
        NETWORK_ERROR("%s: invalid out_port_id(%d)\r\n", __func__, out_port);
        return -EPERM;
    }
 
    if (direct_net) {
        next_mac.net = 0;
        next_mac.len = 0;
        rv = npdu_get_npci_info(&npci_info, NULL, NULL, PRIORITY_NORMAL, false,
            REJECT_MESSAGE_TO_NETWORK);
    } else {
        next_mac.net = 0;
        next_mac.len = entry.mac_len;
        memcpy(next_mac.adr, entry.mac, MAX_MAC_LEN);
        rv = npdu_get_npci_info(&npci_info, dst, NULL, PRIORITY_NORMAL, false, 
            REJECT_MESSAGE_TO_NETWORK);
    }
    
    if (rv < 0) {
        NETWORK_ERROR("%s: encode npci info failed(%d)\r\n", __func__, rv);
        return rv;
    }

    npdu_len = npci_info.nud_offset + 3;
    bacnet_buf_init(&npdu.buf, MIN_APDU);
    npdu.buf.data_len = npdu_len;
     
    rv = npdu_encode_pci(&npdu.buf, &npci_info);
    if (rv < 0) {
        NETWORK_ERROR("%s: encode pci failed(%d)\r\n", __func__, rv);
        return rv;
    }

    len = npci_info.nud_offset;
    npdu.buf.data[len++] = reject_reason;
    len += encode_unsigned16(&(npdu.buf.data[len]), dnet);
    if (len > npdu_len) {
        NETWORK_ERROR("%s: invalid npdu len(%d)\r\n", __func__, len);
        return -EPERM;
    }

    port = &route_ports[out_port];
    rv = port->dl->send_pdu(port->dl, &next_mac, &npdu.buf, PRIORITY_NORMAL, false);
    NETWORK_VERBOS("%s: port(%d), rv = %d\r\n", __func__, out_port, rv);
     
    return rv;
}

/**
 * receive_I_Am_Router_To_Network - ����I_Am_Router_To_Network����
 *
 * @in_port: �������
 * @src_mac: �����ڱ��������������·��Դmac
 * @npdu: ���յ��ı���
 * @npci_info: ���ĵ�Э�������Ϣ
 *
 * @return: �ɹ�����0��ʧ�ܷ��ظ���
 *
 */
int receive_I_Am_Router_To_Network(bacnet_port_t *in_port, bacnet_addr_t *src_mac,
        bacnet_buf_t *npdu, npci_info_t *npci_info)
{
    uint8_t *pdu;
    uint16_t dnet;
    int nud_len;
    uint16_t nud_offset;
    int len;
    int rv;

    nud_offset = npci_info->nud_offset;
    nud_len = npdu->data_len - nud_offset;
    pdu = npdu->data;

    NETWORK_VERBOS("%s: in_port(%d) npdu_len(%d) npci_len(%d)\r\n", __func__, in_port->id,
        npdu->data_len, npci_info->nud_offset);

    if (nud_len == 0 || (nud_len % 2) != 0) {
        NETWORK_ERROR("%s: invalid nud_len(%d)\r\n", __func__, nud_len);
        return -EPERM;
    }
    
    rv = OK;
    while (nud_len > 0) {
        len = decode_unsigned16(&pdu[nud_offset], &dnet);
        nud_len -= len;
        nud_offset += len;

        if (route_update_dynamic_entry(dnet, NETWORK_REACHABLE, in_port, src_mac) < 0) {
            NETWORK_ERROR("%s: update dynamic entry failed(%d)\r\n", __func__, dnet);
            rv = -EPERM;
        }
    }

    if (is_bacnet_router == false) {
        NETWORK_VERBOS("%s: ok\r\n", __func__);
        return OK;
    }
    
    if (rv < 0) {
        return rv;
    }
    
    /* �㲥ת�� */
    if (npci_info->src.net != 0 || npci_info->dst.net != 0) {
        NETWORK_ERROR("%s: not local npdu, src net(%d), dst net(%d)\r\n", __func__,
            npci_info->src.net, npci_info->dst.net);
        return -EPERM;
    }

    rv = route_port_broadcast_pdu(in_port, npdu, npci_info);
    NETWORK_VERBOS("%s: bcast to ports exclude in_port(%d), rv = %d\r\n", __func__, in_port->id, rv);

    return rv;
}

/**
 * receive_Who_Is_Router_To_Network - ����Who_Is_Router_To_Network����
 *
 * @in_port: �������
 * @src_addr: �յ����ĵ�Դ��ַ
 * @npdu: ���յ��ı���
 * @npci_info: ���ĵ�Э�������Ϣ
 *
 * @return: �ɹ�����0��ʧ�ܷ��ظ���
 *
 */
int receive_Who_Is_Router_To_Network(bacnet_port_t *in_port, bacnet_addr_t *src_addr, 
        bacnet_buf_t *npdu, npci_info_t *npci_info)
{
    route_entry_t entry;
    uint16_t dnet;
    uint16_t dnet_list[(MIN_NPDU - 3) / 2];
    int list_num;
    uint8_t *pdu;
    uint32_t pdu_len;
    uint32_t net_num;
    int rv;
 
    if (is_bacnet_router == false) {
        NETWORK_VERBOS("%s: discarded by Non-Router\r\n", __func__);
        return OK;
    }

    pdu = npdu->data;
    pdu_len = npdu->data_len;
    
    NETWORK_VERBOS("%s: in_port(%d) npdu_len(%d) npci_len(%d)\r\n", __func__, in_port->id, pdu_len,
        npci_info->nud_offset);
 
    /* ����δָ��dnet */
    if (pdu_len == npci_info->nud_offset) {
        list_num = sizeof(dnet_list)/sizeof(dnet_list[0]);
        net_num = route_port_get_reachable_net_exclude_port(in_port->id, dnet_list, list_num);
        if (net_num <= 0) {
            NETWORK_ERROR("%s: get reachable net failed(%d)\r\n", __func__, net_num);
            return -EPERM;
        }
        
        rv = send_I_Am_Router_To_Network(in_port->id, dnet_list, net_num);
        if (rv < 0) {
            NETWORK_ERROR("%s: send I_Am_Router_To_Network failed(%d)\r\n", __func__, rv);
        }
        
        return rv;
    }

    if (pdu_len - npci_info->nud_offset != sizeof(dnet)) {
        NETWORK_ERROR("%s: invalid nud_len, npdu_len(%d), npci_len(%d)\r\n", __func__, pdu_len,
            npci_info->nud_offset);
        return -EPERM;
    }
 
    (void)decode_unsigned16(&(pdu[npci_info->nud_offset]), &dnet);
    if ((dnet == 0) || (dnet == BACNET_BROADCAST_NETWORK)) {
        NETWORK_ERROR("%s: invalid dnet(%d)\r\n", __func__, dnet);
        return -EPERM;
    }
    
    if (!route_try_find_entry(dnet, &entry, NULL)) {
        if (npci_info->src.net == 0) {
            rv = npdu_add_src_field(npdu, src_addr);
            if (rv < 0) {
                NETWORK_ERROR("%s: add SRC field failed(%d)\r\n", __func__, rv);
                return rv;
            }
        }

        rv = route_port_broadcast_pdu(in_port, npdu, npci_info);
        NETWORK_VERBOS("%s: relay to ports exclude in_port(%d), rv = %d\r\n", __func__, in_port->id,
            rv);

        return rv;
    }

    if (entry.port == in_port) {
        NETWORK_ERROR("%s: out_port of dnet(%d) cannot be in_port(%d)\r\n", __func__, dnet,
            in_port->id);
        return -EPERM;
    }

    rv = send_I_Am_Router_To_Network(in_port->id, &dnet, 1);
    if (rv < 0) {
        NETWORK_ERROR("%s: send I_Am_Router_to_Network failed(%d)\r\n", __func__, rv);    
    }

    NETWORK_VERBOS("%s: send I_Am_Router_to_Network(%d), rv = %d\r\n", __func__, dnet, rv);

    return rv;
}

/**
 * receive_Reject_Message_To_Network - ����Reject_Message_To_Network����
 *
 * @in_port: �������
 * @src_addr: ����Դ��ַ
 * @npdu: ���յ��ı���
 * @npci_info: ���ĵ�Э�������Ϣ
 *
 * @return: �ɹ�����0��ʧ�ܷ��ظ���
 *
 */
int receive_Reject_Message_To_Network(bacnet_port_t *in_port, bacnet_addr_t *src_addr, 
        bacnet_buf_t *npdu, npci_info_t *npci_info)
{
    uint8_t *pdu;
    uint8_t reject_reason;
    uint16_t dnet;
    int len;
    int rv;

    pdu = npdu->data;
    len = npci_info->nud_offset;
    if (npdu->data_len - len != 3) {
        NETWORK_ERROR("%s: invalid nud_len, npdu_len(%d), npci_len(%d)\r\n", __func__,
            npdu->data_len, len);
        return -EPERM;
    }

    reject_reason = pdu[len++];
    if ((reject_reason != 1) && (reject_reason != 2)) {
        NETWORK_VERBOS("%s: reject_reason(%d)\r\n", __func__, reject_reason);
        return OK;
    }
 
    (void)decode_unsigned16(&pdu[len], &dnet);

    rv = route_update_dynamic_entry(dnet,
        (reject_reason == 1)? NETWORK_UNREACHABLE_PERMANENTLY : NETWORK_UNREACHABLE_TEMPORARILY,
        in_port, src_addr);
    
    if (rv < 0) {
        NETWORK_ERROR("%s: update route fail(%d)\r\n", __func__, rv);
        return -EPERM;
    }
 
    if ((is_bacnet_router == false) || (npci_info->dst.net == 0)) {
        NETWORK_VERBOS("%s: update dnet(%d) entry state ok\r\n", __func__, dnet);
        return OK;
    }
 
    rv = network_relay_handler(in_port, src_addr, npdu, npci_info);
    NETWORK_VERBOS("%s: relay msg, rv = %d\r\n", __func__, rv);
 
    return rv;
}

int receive_Router_Busy_or_Available_To_Network(bacnet_port_t *in_port,
        bool busy, bacnet_addr_t *src_addr, bacnet_buf_t *npdu,
        npci_info_t *npci_info)
{
    uint16_t dnet;
    uint16_t dnet_list[(MIN_NPDU - 3) / 2];
    int list_num;
    uint8_t *pdu;
    int nud_len;
    uint32_t net_num;
    int rv;

    nud_len = npdu->data_len - npci_info->nud_offset;

    NETWORK_VERBOS("%s: in_port(%d) %s npdu_len(%d) npci_len(%d)\r\n", __func__, in_port->id,
        (busy ? "Busy" : "Available"), npdu->data_len, npci_info->nud_offset);

    if (nud_len % 2 != 0) {
        NETWORK_ERROR("%s: invalid nud_len(%d)\r\n", __func__, nud_len);
        return -EPERM;
    }

    /* ����δָ��dnet */
    if (nud_len == 0) {
        list_num = sizeof(dnet_list)/sizeof(dnet_list[0]);
        net_num = route_port_get_reachable_net_on_mac_set_busy(in_port->id, busy, src_addr,
            dnet_list, list_num);
        if (net_num <= 0) {
            NETWORK_ERROR("%s: get reachable net failed(%d)\r\n", __func__, net_num);
            return -EPERM;
        }

        if (is_bacnet_router == false) {
            NETWORK_VERBOS("%s: ok\r\n", __func__);
            return OK;
        }

        rv = bcast_Router_Busy_or_Available_To_Network(in_port, busy, dnet_list, net_num);
        if (rv < 0) {
            NETWORK_ERROR("%s: bcast Route %s failed(%d)\r\n", __func__,
                (busy ? "Busy" : "Available"), rv);
        }

        return rv;
    }

    pdu = npdu->data + npci_info->nud_offset;
    rv = OK;
    while (nud_len > 0) {
        (void)decode_unsigned16(pdu, &dnet);
        pdu += 2;
        nud_len -= 2;

        if ((dnet == 0) || (dnet == BACNET_BROADCAST_NETWORK)) {
            NETWORK_ERROR("%s: invalid dnet(%d)\r\n", __func__, dnet);
            return -EPERM;
        }

        if (route_update_dynamic_entry(dnet,
            (busy ? NETWORK_UNREACHABLE_TEMPORARILY : NETWORK_REACHABLE), in_port, src_addr) < 0) {
            NETWORK_ERROR("%s: dynamic update route %s failed(%d)\r\n", __func__,
                (busy ? "Busy" : "Available"), dnet);
            rv = -EPERM;
        }
    }

    if (rv < 0)
        return rv;

    if (is_bacnet_router == false) {
        NETWORK_VERBOS("%s: ok\r\n", __func__);
        return OK;
    }

    /* �㲥ת�� */
    if (npci_info->src.net != 0 || npci_info->dst.net != 0) {
        NETWORK_ERROR("%s: not local npdu, src net(%d), dst net(%d)\r\n", __func__,
            npci_info->src.net, npci_info->dst.net);
        return -EPERM;
    }

    rv = route_port_broadcast_pdu(in_port, npdu, npci_info);
    NETWORK_VERBOS("%s: bcast to ports exclude in_port(%d), rv = %d\r\n", __func__, in_port->id, rv);

    return rv;
}

