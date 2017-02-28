/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * network.c
 * Original Author:  linzhixian, 2014-7-7
 *
 * BACnet网络层协议
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
#include "bacnet/network.h"
#include "bacnet/apdu.h"
#include "npdu.h"
#include "route.h"
#include "protocol.h"
#include "network_def.h"
#include "misc/eventloop.h"
#include "bacnet/mstp.h"
#include "misc/bits.h"
#include "module_mng.h"
#include "bacnet/bacnet.h"
#include "debug.h"

static bool network_init_status = false;

bool network_dbg_err = true;
bool network_dbg_warn = true;
bool network_dbg_verbos = true;

extern int route_port_nums;
extern bool is_bacnet_router;

static int _buf_push_pci(bacnet_buf_t *pdu, npci_info_t *pci)
{
    int rv;
    
    rv = bacnet_buf_push(pdu, pci->nud_offset);
    if (rv < 0) {
        NETWORK_ERROR("%s: buf push failed(%d)\r\n", __func__, rv);
        return rv;
    }

    rv = npdu_encode_pci(pdu, pci);
    if (rv < 0) {
        NETWORK_ERROR("%s: encode failed(%d)\r\n", __func__, rv);
    }

    return rv;
}

/* 本地应用层报文处理 */
static int network_apdu_handler(bacnet_port_t *in_port, bacnet_addr_t *src_mac, bacnet_buf_t *npdu, 
            npci_info_t *npci_info)
{
    bacnet_prio_t prio;
    DECLARE_BACNET_BUF(reply_apdu, MAX_APDU);
    npci_info_t pci;
    bool der;
    int rv;

    rv = bacnet_buf_pull(npdu, npci_info->nud_offset);
    if (rv < 0) {
        NETWORK_ERROR("%s: buf pull failed(%d)\r\n", __func__, rv);
        return rv;
    }

    (void)bacnet_buf_init(&reply_apdu.buf, MAX_APDU);
    der = (npci_info->control & BIT2)? true: false;
    if (npci_info->src.net != 0) {
        apdu_handler(npdu, der, &reply_apdu.buf, &(npci_info->src));
    } else {
        apdu_handler(npdu, der, &reply_apdu.buf, src_mac);
    }
    
    (void)bacnet_buf_push(npdu, npci_info->nud_offset);

    if (reply_apdu.buf.data_len != 0) {
        prio = (npci_info->control) & 0x03;
        if ((npci_info->src.net == 0) || (npci_info->src.net == in_port->net)) {
            rv = npdu_get_npci_info(&pci, NULL, NULL, prio, false, INVALID_NETWORK_MESSAGE_TYPE);
        } else {
            rv = npdu_get_npci_info(&pci, &(npci_info->src), NULL, prio, false, 
                INVALID_NETWORK_MESSAGE_TYPE);
        }

        if (rv < 0) {
            NETWORK_ERROR("%s: get npci info failed(%d)\r\n", __func__, rv);
            return rv;
        }

        rv = _buf_push_pci(&reply_apdu.buf, &pci);
        if (rv < 0) {
            NETWORK_ERROR("%s: reply_apdu push pci failed(%d)\r\n", __func__, rv);
            return rv;
        }

        /* 单播应答 */
        rv = in_port->dl->send_pdu(in_port->dl, src_mac, &reply_apdu.buf, prio, false);
        if (rv < 0) {
            NETWORK_ERROR("%s: dl send failed(%d)\r\n", __func__, rv);
        }
    }

    return rv;
}

/**
 * network_relay_handler - 报文转发处理
 *
 * @in_port: 报文入口
 * @src_addr: 接收报文的源地址
 * @npdu: 待转发的报文
 * @npci_info: 待转发的报文信息
 *
 * @return: 成功返回0，失败返回负数
 *
 */
int network_relay_handler(bacnet_port_t *in_port, bacnet_addr_t *src_addr, bacnet_buf_t *npdu, 
        npci_info_t *npci_info)
{
    route_entry_t entry;
    bacnet_port_t *out_port;
    bacnet_addr_t next_mac;
    bacnet_prio_t prio;
    bool der;
    bool find;
    uint8_t hop_count;
    uint8_t reason;
    int rv;
    
    if (is_bacnet_router == false) {
        NETWORK_WARN("%s: discarded by Non-Router\r\n", __func__);
        return -EPERM;
    }

    /* DNET域不存在的直接丢弃 */
    if (npci_info->dst.net == 0) {
        NETWORK_ERROR("%s: discard npdu cause dnet field is not present\r\n", __func__);
        return -EPERM;
    }

    /* hop count-- */
    hop_count = npdu->data[npci_info->hop_count_offset];
    (hop_count)--;
    npdu->data[npci_info->hop_count_offset] = hop_count;
    if (hop_count == 0) {
        NETWORK_ERROR("%s: discard msg because hop count reaches to 0\r\n", __func__);
        return -EPERM;
    }

    /* 添加源域 */
    if (npci_info->src.net == 0) {
        rv = npdu_add_src_field(npdu, src_addr);
        if (rv < 0) {
            NETWORK_ERROR("%s: add SRC field failed(%d)\r\n", __func__, rv);
            return -EPERM;
        }
    }

    /* 全局广播报文 */
    if (npci_info->dst.net == BACNET_BROADCAST_NETWORK) {
        rv = route_port_broadcast_pdu(in_port, npdu, npci_info);
        if (rv < 0) {
            NETWORK_ERROR("%s: bcast npdu exclude in_port(%d) failed(%d)\r\n", __func__,
                in_port->id, rv);
        }
        
        return rv;
    }

    /* 远程报文 */
    find = route_find_entry(npci_info->dst.net, &entry);
    if (!find || entry.busy) {
        if (npci_info->msg_type == REJECT_MESSAGE_TO_NETWORK) {
            NETWORK_WARN("%s: relay reject_msg_to_network failed\r\n", __func__);
            return -EPERM;
        }

        if (!find) {
            reason = NETWORK_REJECT_NO_ROUTE;
        } else {
            reason = NETWORK_REJECT_ROUTER_BUSY;
        }
        
        /* send a reject_message_to_network to original device */
        (void)send_Reject_Message_To_Network(in_port->id, &(npci_info->src), npci_info->dst.net, 
            reason);

        return -EPERM;
    }
    
    out_port = entry.port;
    if (out_port == in_port) {
        NETWORK_ERROR("%s: out_port of dnet(%d) cannot be in_port(%d)\r\n", __func__, 
            npci_info->dst.net, in_port->id);
        return -EPERM;
    }

    prio = (npci_info->control) & 0x03;
    der = (npci_info->control & BIT2)? true: false;
    
    if (entry.direct_net == true) {
        rv = npdu_remove_dst_field(npdu);
        if (rv < 0) {
            NETWORK_ERROR("%s: remove DST field failed(%d)\r\n", __func__, rv);
            return rv;
        }
        rv = out_port->dl->send_pdu(out_port->dl, &(npci_info->dst), npdu, prio, der);
    } else {
        next_mac.net = 0;
        next_mac.len = entry.mac_len;
        memcpy(next_mac.adr, entry.mac, MAX_MAC_LEN);
        rv = out_port->dl->send_pdu(out_port->dl, &next_mac, npdu, prio, der);
    }

    if (rv < 0) {
        NETWORK_ERROR("%s: out_port(%d) relay failed(%d)\r\n", __func__, out_port->id, rv);
    }

    NETWORK_VERBOS("%s: out_port(%d) relay(%d)\r\n", __func__, out_port->id, rv);

    return rv;
}

/* 网络层协议报文处理 */
static void network_control_handler(bacnet_port_t *in_port, bacnet_addr_t *src_addr, 
                bacnet_buf_t *npdu, npci_info_t *npci_info)
{
    int rv;
    
    NETWORK_VERBOS("%s: msg_type(%d)\r\n", __func__, npci_info->msg_type);
    
    switch (npci_info->msg_type) {
    case WHO_IS_ROUTER_TO_NETWORK:
        (void)receive_Who_Is_Router_To_Network(in_port, src_addr, npdu, npci_info);
        break;

    case I_AM_ROUTER_TO_NETWORK:
        (void)receive_I_Am_Router_To_Network(in_port, src_addr, npdu, npci_info);
        break;

    case REJECT_MESSAGE_TO_NETWORK:
        (void)receive_Reject_Message_To_Network(in_port, src_addr, npdu, npci_info);
        break;

    case INITIALIZE_ROUTING_TABLE:
    case INITIALIZE_ROUTING_TABLE_ACK:
        goto not_supported;

    case ROUTER_BUSY_TO_NETWORK:
    case ROUTER_AVAILABLE_TO_NETWORK:
        (void)receive_Router_Busy_or_Available_To_Network(in_port,
            npci_info->msg_type == ROUTER_BUSY_TO_NETWORK, src_addr, npdu, npci_info);
        break;
        
    case I_COULD_BE_ROUTER_TO_NETWORK:
    case ESTABLISH_CONNECTION_TO_NETWORK:
    case DISCONNECT_CONNECTION_TO_NETWORK:
        /* Do nothing cause we don't support PTP half-router control */
        goto not_supported;
    
    default:
        /* send a reject_message_to_network */
        (void)send_Reject_Message_To_Network(in_port->id, &(npci_info->src), 0, 
            NETWORK_REJECT_UNKNOWN_MESSAGE_TYPE);
        break;
    }

    return;

not_supported:
    if ((is_bacnet_router == false) || (npci_info->dst.net == 0)) {
        return;
    }

    /* 转发 */
    rv = network_relay_handler(in_port, src_addr, npdu, npci_info);
    if (rv < 0) {
        NETWORK_ERROR("%s: relay network control message failed(%d)\r\n", __func__, rv);
    }
}

/**
 * network_receive_pdu - 网络层收包处理
 *
 * @in_port: 报文入口
 * @npdu: 收到的网络层报文
 * @src_mac: 接收报文的源mac
 *
 * @return: 成功返回0，失败返回负数
 *
 */
int network_receive_pdu(uint32_t port_id, bacnet_buf_t *npdu, bacnet_addr_t *src_mac)
{
    bacnet_port_t *in_port;
    npci_info_t npci_info;
    int rv;

    if ((port_id >= route_port_nums) || (src_mac == NULL) || (npdu == NULL) || (npdu->data == NULL) 
            || (npdu->data_len <= MIN_NPCI_LEN)) {
        NETWORK_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }
    
    rv = npdu_decode_pci(npdu, &npci_info);
    if (rv < 0) {
        NETWORK_ERROR("%s: decode npci failed(%d)\r\n", __func__, rv);
        return rv;
    }

    in_port = route_port_find_by_id(port_id);
    if (in_port == NULL) {
        NETWORK_ERROR("%s: find port(%d) failed\r\n", __func__, port_id);
        return -EPERM;
    }
    src_mac->net = in_port->net;

    if (network_dbg_verbos) {
        printf("%s: port(%d) receive npdu(%d) ", __func__, port_id, npdu->data_len);
        if (npci_info.dst.net != 0) {
            printf("To net(%d) ", npci_info.dst.net);
        }
        printf("from Source: ");
        PRINT_BACNET_ADDRESS(src_mac);
    }

    /* 源域学习 */
    if (npci_info.src.net != 0) {
        (void)route_update_dynamic_entry(npci_info.src.net, NETWORK_REACHABLE_REVERSE, in_port,
            src_mac);
    }

    /* 网络层协议报文处理 */
    if (npci_info.control & BIT7) {
        network_control_handler(in_port, src_mac, npdu, &npci_info);
        return OK;
    }

    /* 本地APDU处理 */
    if ((npci_info.dst.net == 0) || (npci_info.dst.net == BACNET_BROADCAST_NETWORK)) {
        rv = network_apdu_handler(in_port, src_mac, npdu, &npci_info);
        if (rv < 0) {
            NETWORK_ERROR("%s: apdu handler failed(%d)\r\n", __func__, rv);
        }

        if ((is_bacnet_router == false) || (npci_info.dst.net == 0)) {
            return rv;
        }
    }

    /* 转发 */
    rv = network_relay_handler(in_port, src_mac, npdu, &npci_info);
    if (rv < 0) {
        NETWORK_ERROR("%s: relay failed(%d)\r\n", __func__, rv);
    }

    return rv;
}

/**
 * network_send_pdu - 网络层发包接口
 *
 * @dst: 目的地址
 * @buf: 发送缓冲区
 * @prio: 报文优先级
 * @der: 是否应答标志，true表示需要应答，false表示不需要应答
 *
 * @return: 成功返回0，失败返回负数
 *
 */
int network_send_pdu(bacnet_addr_t *dst, bacnet_buf_t *buf, bacnet_prio_t prio, bool der)
{
    npci_info_t npci_info;
    bacnet_port_t *port;
    route_entry_t entry;
    bacnet_addr_t next_mac;
    int cond;
    int rv;
    
    if ((dst == NULL) || (buf == NULL) || (buf->data == NULL) || (buf->data_len == 0)) {
        NETWORK_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    /* 全局广播，DLEN须为0 */
    if ((dst->net == BACNET_BROADCAST_NETWORK) && (dst->len != 0)) {
        NETWORK_ERROR("%s: dst_len of global bcast cannot be (%d)\r\n", __func__, dst->len);
        return -EINVAL;
    }

    if (!NETWORK_PRIO_IS_VALID(prio)) {
        NETWORK_ERROR("%s: invalid prio(%d)\r\n", __func__, prio);
        return -EINVAL;
    }

    if (network_init_status == false) {
        NETWORK_ERROR("%s: network init failed\r\n", __func__);
        return -EPERM;
    }

    port = NULL;
    if (dst->net == 0) {
        if (is_bacnet_router == true) {
            NETWORK_ERROR("%s: For Router sender, the dst_net cannot be %d\r\n", __func__, dst->net);
            return -EPERM;
        }
        
        cond = 0;
        rv = npdu_get_npci_info(&npci_info, NULL, NULL, prio, der, INVALID_NETWORK_MESSAGE_TYPE);
    } else if (dst->net == BACNET_BROADCAST_NETWORK) {
        cond = 1;
        rv = npdu_get_npci_info(&npci_info, dst, NULL, prio, der, INVALID_NETWORK_MESSAGE_TYPE);
    } else {
        bool need_whois;
        if (!route_try_find_entry(dst->net, &entry, &need_whois)) {
            NETWORK_ERROR("%s: dnet(%d) entry is not found\r\n", __func__, dst->net);
            if (need_whois) {
                bcast_Who_Is_Router_To_Network(NULL, dst->net);
            }
            return -EPERM;
        }

        if (entry.busy) {
            NETWORK_ERROR("%s: dnet(%d) is busy\r\n", __func__, dst->net);
            return -EPERM;
        }

        port = entry.port;
        if (entry.direct_net) {
            cond = 2;
            rv = npdu_get_npci_info(&npci_info, NULL, NULL, prio, der, INVALID_NETWORK_MESSAGE_TYPE);
        } else {
            cond = 3;
            next_mac.net = 0;
            next_mac.len = entry.mac_len;
            memcpy(next_mac.adr, entry.mac, MAX_MAC_LEN);
            rv = npdu_get_npci_info(&npci_info, dst, NULL, prio, der, INVALID_NETWORK_MESSAGE_TYPE);
        }
    }

    if (rv < 0) {
        NETWORK_ERROR("%s: encode npci_info failed(%d)\r\n", __func__, rv);
        return rv;
    }

    rv = _buf_push_pci(buf, &npci_info);
    if (rv < 0) {
        NETWORK_ERROR("%s: push buf npci failed(%d)\r\n", __func__, rv);
        return rv;
    }

    switch (cond) {
    case 0:
        port = route_port_get_list_head();
        rv = route_port_unicast_pdu(port, dst, buf, &npci_info);
        break;

    case 1:
        rv = route_port_broadcast_pdu(NULL, buf, &npci_info);
        break;

    case 2:
        rv = route_port_unicast_pdu(port, dst, buf, &npci_info);
        break;

    case 3:
        rv = route_port_unicast_pdu(port, &next_mac, buf, &npci_info);
        break;
    
    default:
        NETWORK_ERROR("%s: invalid cond(%d)\r\n", __func__, cond);
        rv = -EPERM;
        break;
    }

    bacnet_buf_pull(buf, npci_info.nud_offset);
    
    NETWORK_VERBOS("%s: rv = %d\r\n", __func__, rv);
    
    return rv;
}

/**
 * network_set_dbg_level - 设置调试状态
 *
 * @level: 调试开关
 *
 * @return: void
 *
 */
void network_set_dbg_level(uint32_t level)
{
    network_dbg_verbos = level & DEBUG_LEVEL_VERBOS;
    network_dbg_warn = level & DEBUG_LEVEL_WARN;
    network_dbg_err = level & DEBUG_LEVEL_ERROR;
}

/**
 * network_show_dbg_status - 查看调试状态信息
 *
 * @return: void
 *
 */
void network_show_dbg_status(void)
{
    printf("Device: %s\r\n", is_bacnet_router? "Router": "Non-Router");
    printf("network_init_status: %d\r\n", network_init_status);
    printf("network_dbg_verbos: %d\r\n", network_dbg_verbos);
    printf("network_dbg_warn: %d\r\n", network_dbg_warn);
    printf("network_dbg_err: %d\r\n", network_dbg_err);
}

void network_show_route_table(void)
{
    route_table_show();
}

/**
 * network_receive_mstp_proxy_pdu - mstp proxy功能专用的包处理。当mstp层收到广播包时，
 * 除调用network_receive_pdu, 还需调用本接口。当mstp层的send被上层调用时，如目标地址为广播，
 * 则也调用本接口
 *
 * @in_port: 报文入口
 * @npdu: 接收到的网络层报文
 * @src_mac: 接收报文的源mac
 *
 * @return: 成功返回0，失败返回负数
 *
 */
int network_receive_mstp_proxy_pdu(uint32_t port_id, bacnet_buf_t *npdu)
{
    bacnet_port_t *in_port;
    npci_info_t npci_info;
    int rv;

    if ((port_id >= route_port_nums) || (npdu == NULL) || (npdu->data == NULL)
            || (npdu->data_len <= MIN_NPCI_LEN)) {
        NETWORK_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    rv = npdu_decode_pci(npdu, &npci_info);
    if (rv < 0) {
        NETWORK_ERROR("%s: decode npci failed(%d)\r\n", __func__, rv);
        return rv;
    }

    in_port = route_port_find_by_id(port_id);
    if (in_port == NULL) {
        NETWORK_ERROR("%s: find port(%d) failed\r\n", __func__, port_id);
        return -EPERM;
    }
    if (in_port->dl->type != DL_MSTP) {
        NETWORK_ERROR("%s: port(%d) not mstp\r\n", __func__, port_id);
        return -EPERM;
    }

    /* 源域学习在network_receive_pdu中完成 */
    /* 网络层协议报文不处理 */
    if (npci_info.control & BIT7) {
        return OK;
    }

    /* 本地APDU处理 */
    if ((npci_info.dst.net == 0) || (npci_info.dst.net == BACNET_BROADCAST_NETWORK)) {
        rv = bacnet_buf_pull(npdu, npci_info.nud_offset);
        if (rv < 0) {
            NETWORK_ERROR("%s: buf pull failed(%d)\r\n", __func__, rv);
            return rv;
        }

        if (npci_info.src.net)
            apdu_mstp_proxy_handler(npdu, npci_info.src.net, in_port->net);
        else
            apdu_mstp_proxy_handler(npdu, in_port->net, in_port->net);

        (void)bacnet_buf_push(npdu, npci_info.nud_offset);
    }

    /* 不转发 */
    return rv;
}

/**
 * MSTP层的伪装报文
 * @param port_id
 * @param mac
 * @param dst
 * @param buf
 * @param prio
 * @return 失败返回负数，0表示转发出口，>0表示MSTP伪装出口
 */
int network_mstp_fake(uint32_t port_id, uint8_t mac, bacnet_addr_t *dst, bacnet_buf_t *buf,
        bacnet_prio_t prio)
{
    npci_info_t npci_info;
    bacnet_port_t *port;
    route_entry_t entry;
    bacnet_addr_t next_mac;
    bacnet_port_t *in_port;
    bool need_relay;
    int rv;

    if ((dst == NULL) || (buf == NULL) || (buf->data == NULL) || (buf->data_len == 0)) {
        NETWORK_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    /* 全局广播，DLEN须为0 */
    if ((dst->net == BACNET_BROADCAST_NETWORK) && (dst->len != 0)) {
        NETWORK_ERROR("%s: dst_len of global bcast cannot be (%d)\r\n", __func__, dst->len);
        return -EINVAL;
    }

    if (!NETWORK_PRIO_IS_VALID(prio)) {
        NETWORK_ERROR("%s: invalid prio(%d)\r\n", __func__, prio);
        return -EINVAL;
    }

    if (network_init_status == false) {
        NETWORK_ERROR("%s: network init failed\r\n", __func__);
        return -EPERM;
    }

    if (port_id >= route_port_nums) {
        NETWORK_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    in_port = route_port_find_by_id(port_id);
    if (in_port == NULL) {
        NETWORK_ERROR("%s: find port(%d) failed\r\n", __func__, port_id);
        return -EPERM;
    }

    if (in_port->dl->type != DL_MSTP) {
        NETWORK_ERROR("%s: port(%d) not mstp\r\n", __func__, port_id);
        return -EPERM;
    }

    port = NULL;
    need_relay = false;
    if (dst->net == 0) {
        if (is_bacnet_router == true) {
            NETWORK_ERROR("%s: For Router sender, the dst_net cannot be %d\r\n", __func__, dst->net);
            return -EPERM;
        }
    } else if (dst->net == BACNET_BROADCAST_NETWORK) {
        need_relay = true;
    } else {
        if (!route_find_entry(dst->net, &entry)) {
            NETWORK_ERROR("%s: dnet(%d) entry is not found\r\n", __func__, dst->net);
            return -EPERM;
        }
        if (entry.busy) {
            NETWORK_ERROR("%s: dnet(%d) is busy\r\n", __func__, dst->net);
            return -EPERM;
        }

        port = entry.port;
        if (port != in_port) {              /* relay on other port */
            need_relay = true;
        } else if (!entry.direct_net) {     /* to in_port, but other net */
            next_mac.net = 0;
            next_mac.len = entry.mac_len;
            memcpy(next_mac.adr, entry.mac, MAX_MAC_LEN);
        } else {
            port = NULL;                    /* local */
        }
    }

    if (need_relay) {
        bacnet_addr_t src_mac;
        src_mac.net = in_port->net;
        src_mac.len = 1;
        src_mac.adr[0] = mac;

        rv = npdu_get_npci_info(&npci_info, dst, NULL, prio, false, INVALID_NETWORK_MESSAGE_TYPE);
        if (rv < 0) {
            NETWORK_ERROR("%s: encode relay npci_info failed(%d)\r\n", __func__, rv);
            return rv;
        }

        rv = _buf_push_pci(buf, &npci_info);
        if (rv < 0) {
            NETWORK_ERROR("%s: push relay buf npci failed(%d)\r\n", __func__, rv);
            return rv;
        }

        rv = network_relay_handler(in_port, &src_mac, buf, &npci_info);
        if (rv < 0) {
            NETWORK_ERROR("%s: relay failed(%d)\r\n", __func__, rv);
        }

        if (port != NULL) {     /* only need relay, not global broadcast */
            return rv;
        }

        bacnet_buf_pull(buf, npci_info.nud_offset);
    }

    if (port == NULL) { /* local unicast/broadcast or global broadcast */
        rv = npdu_get_npci_info(&npci_info, NULL, NULL, prio, false, INVALID_NETWORK_MESSAGE_TYPE);
    } else {            /* send to another jump */
        rv = npdu_get_npci_info(&npci_info, dst, NULL, prio, false, INVALID_NETWORK_MESSAGE_TYPE);
    }

    if (rv < 0) {
        NETWORK_ERROR("%s: encode npci_info failed(%d)\r\n", __func__, rv);
        return rv;
    }

    rv = _buf_push_pci(buf, &npci_info);
    if (rv < 0) {
        NETWORK_ERROR("%s: push buf npci failed(%d)\r\n", __func__, rv);
        return rv;
    }

    rv = mstp_fake_pdu((datalink_mstp_t*)in_port->dl, port ? &next_mac : dst, mac, buf, prio);
    NETWORK_VERBOS("%s: rv = %d\r\n", __func__, rv);

    bacnet_buf_pull(buf, npci_info.nud_offset);
    if (rv < 0)
        return rv;

    return 1;
}

int network_number_get(uint32_t port_id)
{
    bacnet_port_t *in_port;
    
    in_port = route_port_find_by_id(port_id);
    if (in_port == NULL) {
        NETWORK_ERROR("%s: find port(%d) failed\r\n", __func__, port_id);
        return -EPERM;
    }

    return in_port->net;
}

static module_handler_t module = {
    .name = "network",
    .startup = network_startup,
    .stop = network_stop,
};

int network_init(void)
{
    int rv;

    rv = datalink_init();
    if (rv < 0) {
        NETWORK_ERROR("%s: datalink init failed(%d)\r\n", __func__, rv);
        goto out0;
    }
    
    rv = module_mng_register(&module);
    if (rv < 0) {
        NETWORK_ERROR("%s: module register failed(%d)\r\n", __func__, rv);
        goto out1;
    }

    NETWORK_VERBOS("%s: OK\r\n", __func__);
    return OK;

out1:
    datalink_exit();

out0:

    return rv;
}

int network_startup(void)
{
    cJSON *network_cfg;
    int rv;

    rv = route_table_init();
    if (rv < 0) {
        NETWORK_ERROR("%s: route table init failed(%d)\r\n", __func__, rv);
        goto out0;
    }

    network_cfg = bacnet_get_network_cfg();
    if (network_cfg == NULL) {
        NETWORK_ERROR("%s: get network cfg failed\r\n", __func__);
        rv = -EPERM;
        goto out1;
    }

    rv = route_port_init(network_cfg);
    if (rv < 0) {
        NETWORK_ERROR("%s: route port init failed(%d)\r\n", __func__, rv);
        goto out2;
    }

    rv = route_table_config(network_cfg);
    if (rv < 0) {
        NETWORK_ERROR("%s: route config failed(%d)\r\n", __func__, rv);
        goto out3;
    }

    rv = datalink_startup();
    if (rv < 0) {
        NETWORK_ERROR("%s: datalink startup failed(%d)\r\n", __func__, rv);
        goto out3;
    }

    rv = route_startup();
    if (rv < 0) {
        NETWORK_ERROR("%s: route startup failed(%d)\r\n", __func__, rv);
        goto out4;
    }

    network_init_status = true;
    network_set_dbg_level(0);

    return OK;

out4:
    datalink_stop();

out3:
    route_port_destroy();

out2:
    cJSON_Delete(network_cfg);

out1:
    route_table_destroy();

out0:
    return rv;
}

void network_stop(void)
{
    datalink_stop();
    
    network_init_status = false;

    return;
}

/**
 * network_exit - 网络层反初始化
 *
 * @return: void
 *
 */
void network_exit(void)
{
    datalink_exit();
    
    network_stop();
}

cJSON *network_get_status(connect_info_t *conn, cJSON *request)
{
    /* TODO */
    return cJSON_CreateObject();
}

cJSON *network_get_port_mib(connect_info_t *conn, cJSON *request)
{
    cJSON *reply, *tmp;

    reply = cJSON_CreateObject();
    if (reply == NULL) {
        NETWORK_ERROR("%s: create result object failed\r\n", __func__);
        connect_mng_drop(conn);
        return NULL;
    }
    
    tmp = route_get_port_mib(request);
    if (tmp == NULL) {
        NETWORK_ERROR("%s: get port mib failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get port mib failed");
    } else {
        cJSON_AddItemToObject(reply, "result", tmp);
    }

    return reply;    
}

