/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bip.c
 * Original Author:  linzhixian, 2014-10-10
 *
 * BACNET/IP
 *
 * History
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/timerfd.h>

#include "bacnet/config.h"
#include "bip_def.h"
#include "bacnet/bip.h"
#include "bacnet/bacint.h"
#include "bacnet/bactext.h"
#include "bacnet/network.h"
#include "debug.h"

static LIST_HEAD(all_bip_list);

bool bip_dbg_verbos = true;
bool bip_dbg_warn = true;
bool bip_dbg_err = true;

static int bip_internet_to_bacnet_address(struct sockaddr_in *sin, bacnet_addr_t *src)
{
    if ((sin == NULL) || (src == NULL)) {
        BIP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    src->net = 0;
    src->len = BIP_ADDRESS_LEN;
    memcpy(&(src->adr[0]), &(sin->sin_addr.s_addr), IP_ADDRESS_LEN);
    memcpy(&(src->adr[IP_ADDRESS_LEN]), &(sin->sin_port), UDP_PORT_LEN);

    return OK;
}

int bip_get_interface_info(const char *ifname, struct in_addr *ip, struct in_addr *mask)
{
    struct ifreq ifr;
    struct sockaddr_in *sock_addr;
    int rv;
    int sock_fd;

    if (ifname == NULL) {
        BIP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_fd < 0) {
        BIP_ERROR("%s: create uip socket failed cause %s\r\n", __func__, strerror(errno));
        return -EPERM;
    }

    (void)strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    
    if (ip) {
        rv = ioctl(sock_fd, SIOCGIFADDR, &ifr);
        if (rv < 0) {
            BIP_ERROR("%s: ioctl on %s failed(%s)\r\n", __func__, ifname, strerror(errno));
            close(sock_fd);
            return rv;
        }

        sock_addr = (struct sockaddr_in *)&(ifr.ifr_addr);
        memcpy(ip, &(sock_addr->sin_addr), sizeof(struct in_addr));
    }

    if (mask) {
        rv = ioctl(sock_fd, SIOCGIFNETMASK, &ifr);
        if (rv < 0) {
            BIP_ERROR("%s: ioctl on %s failed(%s)\r\n", __func__, ifname, strerror(errno));
            close(sock_fd);
            return rv;
        }

        sock_addr = (struct sockaddr_in *)&(ifr.ifr_addr);
        memcpy(mask, &(sock_addr->sin_addr), sizeof(struct in_addr));
    }

    close(sock_fd);

    return OK;
}

/**
 * bip_send_pdu - bip发帧接口
 *
 * @bip: 发送端口
 * @dst_mac: 目的MAC地址
 * @npdu: npdu发送缓冲区
 * @prio: 发送优先级
 * @der: 报文应答标志，true表示需要应答，false表示不需要应答
 *
 * 若dst_mac为NULL或者dst_mac->len为0，则表示在本地广播；
 *
 * @return: 成功返回0，失败返回负数
 *
 */
static int bip_send_pdu(datalink_bip_t *bip, bacnet_addr_t *dst_mac, bacnet_buf_t *npdu,
	        __attribute__ ((unused))bacnet_prio_t prio, __attribute__ ((unused))bool der)
{
    struct sockaddr_in bip_dst;
    struct sockaddr_in bip_src;
    struct in_addr address;
    uint8_t function;
    uint8_t *mpdu;
    uint16_t port;
    uint16_t mpdu_len;
    int rv;

    if (!bip) {
        BIP_ERROR("%s: null bip\r\n", __func__);
        return -EINVAL;
    }

    bip->dl.tx_all++;
    
    if ((npdu == NULL) || (npdu->data == NULL) || (npdu->data_len == 0) 
            || ((npdu->data_len> BIP_MAX_DATA_LEN))) {
        BIP_ERROR("%s: invalid npdu\r\n", __func__);
        return -EINVAL;
    }

    if ((dst_mac == NULL) || (dst_mac->len == 0)) {
        if (bip->fd_client) {
            function = BVLC_DISTRIBUTE_BROADCAST_TO_NETWORK;
            address.s_addr = bip->fd_client->remote.sin_addr.s_addr;
            port = bip->fd_client->remote.sin_port;
        } else {
            function = BVLC_ORIGINAL_BROADCAST_NPDU;
            address.s_addr = bip->bcast_sin.sin_addr.s_addr;
            port = bip->bcast_sin.sin_port;
        }
    } else if (dst_mac->len == BIP_ADDRESS_LEN) {
        if (memcmp(&(bip->bcast_sin.sin_addr.s_addr), &(dst_mac->adr[0]), IP_ADDRESS_LEN) == 0) {
            BIP_ERROR("%s: unicast to broadcast ip\r\n", __func__);
            return -EINVAL;
        }
        function = BVLC_ORIGINAL_UNICAST_NPDU;
        memcpy(&(address.s_addr), &(dst_mac->adr[0]), IP_ADDRESS_LEN);
        memcpy(&port, &(dst_mac->adr[IP_ADDRESS_LEN]), UDP_PORT_LEN);
    } else {
        BIP_ERROR("%s: invalid dst_mac len(%d)\r\n", __func__, dst_mac->len);
        return -EINVAL;
    }

    rv = bacnet_buf_push(npdu, BVLC_HDR_LEN);
    if (rv < 0) {
        BIP_ERROR("%s: buf push failed(%d)\r\n", __func__, rv);
        return rv;
    }

    mpdu_len = npdu->data_len;
    mpdu = npdu->data;
    mpdu[0] = BVLL_TYPE_BACNET_IP;
    mpdu[1] = function;
    (void)encode_unsigned16(&mpdu[2], mpdu_len);
    
    bip_dst.sin_family = AF_INET;
    bip_dst.sin_addr.s_addr = address.s_addr;
    bip_dst.sin_port = port;
    memset(&(bip_dst.sin_zero), '\0', sizeof(bip_dst.sin_zero));

    rv = sendto(bip->sock_uip, mpdu, mpdu_len, MSG_DONTWAIT, (struct sockaddr *)&bip_dst,
        sizeof(struct sockaddr));
    if (rv < 0) {
        BIP_ERROR("%s: sendto failed cause %s\r\n", __func__, strerror(errno));
    } else if (rv != mpdu_len) {
        BIP_ERROR("%s: sockfd(%d) send failed cause only send %d bytes in %d bytes\r\n", __func__, 
            bip->sock_uip, rv, mpdu_len);
        rv = -EPERM;
    } else {
        bip->dl.tx_ok++;
        rv = OK;
    }

    (void)bacnet_buf_pull(npdu, BVLC_HDR_LEN);

    if ((bip->bbmd) && (function == BVLC_ORIGINAL_BROADCAST_NPDU)) {
        bip_src.sin_addr.s_addr = bip->sin.sin_addr.s_addr;
        bip_src.sin_port = bip->sin.sin_port;
        
        (void)bvlc_bdt_forward_npdu(bip, &bip_src, npdu);
        (void)bvlc_fdt_forward_npdu(bip, &bip_src, npdu);
    }

    return rv;
}

/**
 * bip_event_handler - bip事件处理函数
 *
 * @handler: 指向datalink_bip_t.handler，用于找到datalink_bip_t对象
 * @events: epoll事件
 *
 */
static void bip_event_handler(el_watch_t *watch, int events)
{
    datalink_bip_t *bip;
    int fd;
    BACNET_BVLC_FUNCTION function;
    bacnet_addr_t src_mac;
    struct sockaddr_in sin;
    struct in_addr my_addr;
    socklen_t sin_len;
    uint8_t *mpdu;
    uint16_t mpdu_len;
    uint16_t my_port;
    uint16_t npdu_offset;
    DECLARE_BACNET_BUF(rx_pdu, BIP_RX_BUFF_LEN);
    int rx_bytes;
    int rv;

    if (!(events & EPOLLIN)) {
        BIP_ERROR("%s: invalid events\r\n", __func__);
        return;
    }

    bip = (datalink_bip_t *)watch->data;
    if (!bip) {
        BIP_ERROR("%s: null bip argument\r\n", __func__);
        return;
    }

    fd = el_watch_fd(watch);
    if (fd < 0) {
        BIP_ERROR("%s: invalid watch fd(%d)\r\n", __func__, fd);
        return;
    }
    
    if ((bip->sock_uip != fd) && (bip->sock_bip != fd)) {
        BIP_ERROR("%s: port_id(%d) sock_uip(%d) sock_bip(%d) wrong callback fd(%d)\r\n", __func__,
            bip->dl.port_id, bip->sock_uip, bip->sock_bip, fd);
        return;
    }

    bacnet_buf_init(&rx_pdu.buf, BIP_RX_BUFF_LEN);
    mpdu = rx_pdu.buf.data;
    sin_len = sizeof(sin);
    rx_bytes = recvfrom(fd, mpdu, BIP_RX_BUFF_LEN, MSG_DONTWAIT | MSG_TRUNC,
        (struct sockaddr *)&sin, &sin_len);
    if (rx_bytes < 0) {
        BIP_ERROR("%s: recvfrom failed cause %s\r\n", __func__, strerror(errno));
        return;
    }

    if (rx_bytes < BVLC_HDR_LEN || rx_bytes > BIP_RX_BUFF_LEN) {
        BIP_ERROR("%s: invalid mpdu_len(%d)\r\n", __func__, rx_bytes);
        return;
    }

    if (mpdu[0] != BVLL_TYPE_BACNET_IP) {
        BIP_ERROR("%s: unknown BVLC Type(0x%2x)\r\n", __func__, mpdu[0]);
        return;
    }

    (void)decode_unsigned16(&mpdu[2], &mpdu_len);
    if (mpdu_len != rx_bytes) {
        BIP_ERROR("%s: the bvlc length(%d) is not equal to rx_bytes(%d)\r\n", __func__, mpdu_len,
            rx_bytes);
        return;
    }
    rx_pdu.buf.data_len = mpdu_len;
    
    my_addr.s_addr = bip->sin.sin_addr.s_addr;
    my_port = bip->sin.sin_port;
    if ((sin.sin_addr.s_addr == my_addr.s_addr) && (sin.sin_port == my_port)) {
        return;
    }

    BIP_VERBOS("%s: from %s:%04X\r\n", __func__, inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
    
    function = mpdu[1];
    switch (function) {
    case BVLC_RESULT:
        (void)bvlc_receive_bvlc_result(bip, &rx_pdu.buf);
        return;
        
    case BVLC_WRITE_BROADCAST_DISTRIBUTION_TABLE:
        (void)bvlc_receive_write_bdt(bip, &sin, &rx_pdu.buf);
        return;
        
    case BVLC_READ_BROADCAST_DISTRIBUTION_TABLE:
        (void)bvlc_receive_read_bdt(bip, &sin, &rx_pdu.buf);
        return;
        
    case BVLC_READ_BROADCAST_DISTRIBUTION_TABLE_ACK:
        (void)bvlc_receive_read_bdt_ack(bip, &rx_pdu.buf);
        return;
        
    case BVLC_FORWARDED_NPDU:
        memcpy(&(sin.sin_addr.s_addr), &mpdu[4], IP_ADDRESS_LEN);
        memcpy(&(sin.sin_port), &mpdu[8], UDP_PORT_LEN);
        
        if (bip->bbmd) {
            (void)bvlc_receive_forwarded_npdu(bip, &sin, &rx_pdu.buf);
        }
        npdu_offset = FORWARDED_NPDU_HDR_LEN;
        break;
        
    case BVLC_REGISTER_FOREIGN_DEVICE:
        (void)bvlc_receive_register_foreign_device(bip, &sin, &rx_pdu.buf);
        return;
        
    case BVLC_READ_FOREIGN_DEVICE_TABLE:
        (void)bvlc_receive_read_fdt(bip, &sin, &rx_pdu.buf);
        return;
        
    case BVLC_READ_FOREIGN_DEVICE_TABLE_ACK:
        (void)bvlc_receive_read_fdt_ack(bip, &rx_pdu.buf);
        return;
        
    case BVLC_DELETE_FOREIGN_DEVICE_TABLE_ENTRY:
        (void)bvlc_receive_delete_fdt_entry(bip, &sin, &rx_pdu.buf);
        return;
    
    case BVLC_DISTRIBUTE_BROADCAST_TO_NETWORK:
        if (!(bip->bbmd)) {
            BIP_ERROR("%s: Unexpected Msg(%d) for Non-BBMD\r\n", __func__, function);
            return;
        }
        
        (void)bvlc_receive_distribute_bcast_to_network(bip, &sin, &rx_pdu.buf);
        npdu_offset = 4;
        break;
    
    case BVLC_ORIGINAL_UNICAST_NPDU:
        npdu_offset = 4;
        break;
        
    case BVLC_ORIGINAL_BROADCAST_NPDU:
        if (bip->fd_client) {
            BIP_ERROR("%s: Unexpected Msg(%d) for Foreign Device\r\n", __func__, function);
            return;
        }
        
        if (bip->bbmd) {
            (void)bvlc_receive_original_broadcast_npdu(bip, &sin, &rx_pdu.buf);
        }
        npdu_offset = 4;
        break;
        
    default:
        BIP_ERROR("%s: Unknown BVLC Function(%d)\r\n", __func__, function);
        return;
    }

    bip->dl.rx_all++;

    rv = bip_internet_to_bacnet_address(&sin, &src_mac);
    if (rv < 0) {
        BIP_ERROR("%s: sin to bacnet address failed(%d)\r\n", __func__, rv);
        return;
    }
    
    if (mpdu_len <= npdu_offset) {
        BIP_ERROR("%s: invalid mpdu_len(%d)\r\n", __func__, mpdu_len);
        return;
    }

    rv = bacnet_buf_pull(&rx_pdu.buf, npdu_offset);
    if (rv < 0) {
        BIP_ERROR("%s: buf pull failed(%d)\r\n", __func__, rv);
        return;
    }

    bip->dl.rx_ok++;
    (void)network_receive_pdu(bip->dl.port_id, &rx_pdu.buf, &src_mac);

    return;
}

/* FD设备注册 */
static void bip_fd_register_func(el_timer_t *timer)
{
    datalink_bip_t *bip = (datalink_bip_t*)timer->data;
    int rv;

    BIP_VERBOS("%s: remote_bbmd %s:%04X\r\n", __func__, inet_ntoa(bip->fd_client->remote.sin_addr),
        ntohs(bip->fd_client->remote.sin_port));
    
    rv = bvlc_register_with_bbmd(bip, &(bip->fd_client->remote), bip->fd_client->ttl);
    if (rv < 0) {
        BIP_ERROR("%s: register with bbmd failed(%d)\r\n", __func__, rv);
    }

    el_timer_mod(&el_default_loop, timer, bip->fd_client->interval * 1000);
}

static int bip_fd_register_config(datalink_bip_t *bip, cJSON *cfg)
{
    int rv;

    if (cfg->type != cJSON_Object) {
        BIP_ERROR("%s: cfg should be object\r\n", __func__);
        return -EPERM;
    }

    bip->fd_client = (fd_client_t*)malloc(sizeof(fd_client_t));
    if (!bip->fd_client) {
        BIP_ERROR("%s: not enough memory\r\n", __func__);
        return -EPERM;
    }
    memset(bip->fd_client, 0, sizeof(fd_client_t));

    cJSON *ipstr = cJSON_GetObjectItem(cfg, "dst_bbmd");
    if ((!ipstr) || (ipstr->type != cJSON_String)) {
        BIP_ERROR("%s: get dst_bbmd item in fd_register failed\r\n", __func__);
        goto err1;
    }

    rv = inet_aton(ipstr->valuestring, &bip->fd_client->remote.sin_addr);
    if (rv == 0) {
        BIP_ERROR("%s: inet_aton of dst_bbmd in fd_register "
                "failed cause %s\r\n", __func__, strerror(errno));
        goto err1;
    }

    cJSON *port = cJSON_GetObjectItem(cfg, "dst_port");
    if ((!port) || (port->type != cJSON_Number) || (port->valueint <= 0)
            || (port->valueint > 65535)) {
        BIP_ERROR("%s: get dst_port item in fd_register failed\r\n", __func__);
        goto err1;
    }

    bip->fd_client->remote.sin_family = AF_INET;
    bip->fd_client->remote.sin_port = htons((uint16_t)port->valueint);

    cJSON *ttl = cJSON_GetObjectItem(cfg, "ttl");
    if (!ttl) {
        BIP_WARN("%s: no ttl item for fd_register, use default %d\r\n", __func__, FD_DEFAULT_TTL);
        bip->fd_client->ttl = FD_DEFAULT_TTL;
    } else if (ttl->type != cJSON_Number) {
        BIP_ERROR("%s: ttl item in fd_register should be number\r\n", __func__);
        goto err1;
    } else if (ttl->valueint < FD_MIN_TTL) {
        BIP_WARN("%s: ttl in fd_register should>=%d\r\n", __func__, FD_MIN_TTL);
        bip->fd_client->ttl = FD_MIN_TTL;
    } else if (ttl->valueint >= FD_MAX_TTL) {
        BIP_WARN("%s: ttl in fd_register should<%d\r\n", __func__, FD_MAX_TTL);
        bip->fd_client->ttl = FD_MAX_TTL - 1;
    } else {
        bip->fd_client->ttl = ttl->valueint;
    }

    cJSON *reg = cJSON_GetObjectItem(cfg, "register_interval");
    if (!reg) {
        BIP_WARN("%s: no register_interval item for fd_register, use default %d\r\n", __func__,
            bip->fd_client->ttl/2);
        bip->fd_client->interval = bip->fd_client->ttl / 2;
    } else if (reg->type != cJSON_Number) {
        BIP_ERROR("%s: register_interval item in fd_register should be number\r\n", __func__);
        goto err1;
    } else if (reg->valueint < FD_MIN_REGISTER_INTERVAL) {
        BIP_WARN("%s: register_interval in fd_register should>=%d\r\n", __func__,
            FD_MIN_REGISTER_INTERVAL);
        bip->fd_client->interval = FD_MIN_REGISTER_INTERVAL;
    } else if (reg->valueint > bip->fd_client->ttl) {
        BIP_WARN("%s: register_interval in fd_register should <= ttl\r\n", __func__);
        bip->fd_client->interval = bip->fd_client->ttl;
    } else {
        bip->fd_client->interval = reg->valueint;
    }

    return OK;

err1:
    free(bip->fd_client);
    bip->fd_client = NULL;
    
    return -EPERM;
}

/* 配置BBMD口 */
static int bip_bbmd_config(datalink_bip_t *bip, cJSON *cfg)
{
    bdt_entry_t *bbmd_table;
    struct in_addr in_addr;
    cJSON *bdt, *entry, *tmp;
    uint16_t port;
    int bdt_size;
    int i;
    int rv;
    bool router_bst;

    if (cfg->type != cJSON_Object) {
        BIP_ERROR("%s: cfg should be object\r\n", __func__);
        return -EPERM;
    }

    bip->bbmd = bbmd_create();
    if (!bip->bbmd) {
        BIP_ERROR("%s: create bbmd data failed\r\n", __func__);
        return -EPERM;
    }
    
    tmp = cJSON_GetObjectItem(cfg, "router_broadcast");
    if (tmp) {
        if ((tmp->type != cJSON_True) && (tmp->type != cJSON_False)) {
            BIP_ERROR("%s: router_broadcast item should be boolean\r\n", __func__);
            goto err1;
        }
        router_bst = tmp->type == cJSON_True;
    } else {
        BIP_WARN("%s: no router_broadcast found, set to false\r\n", __func__);
        router_bst = false;
    }

    /* 根据配置文件创建BDT */
    bdt = cJSON_GetObjectItem(cfg, "BDT");
    if (bdt) {
        if (bdt->type != cJSON_Array) {
            BIP_ERROR("%s: bdt item should be array\r\n", __func__);
            goto err1;
        }
        bdt_size = cJSON_GetArraySize(bdt);
    } else {
        BIP_WARN("%s: no BDT found, use empty array\r\n", __func__);
        bdt_size = 0;
    }

    if (bdt_size > BDT_MAX_SIZE - 1) {
        BIP_ERROR("%s: BDT size should be <= %d\r\n", __func__, BDT_MAX_SIZE - 1);
        goto err1;
    }

    bbmd_table = (bdt_entry_t *)malloc(sizeof(bdt_entry_t) * (bdt_size + 1));
    if (bbmd_table == NULL) {
        BIP_ERROR("%s: malloc bbmd_table failed\r\n", __func__);
        goto err1;
    }
    memset(bbmd_table, 0, sizeof(bdt_entry_t) * (bdt_size + 1));

    for (i = 0; i < bdt_size; i++) {
        entry = cJSON_GetArrayItem(bdt, i);
        if ((!entry) || (entry->type != cJSON_Object)) {
            BIP_ERROR("%s: get BDT_Entry[%d] item failed\r\n", __func__, i);
            goto err2;
        }

        tmp = cJSON_GetObjectItem(entry, "dst_bbmd");
        if ((!tmp) || (tmp->type != cJSON_String)) {
            BIP_ERROR("%s: get BDT_Entry[%d] dst_bbmd item failed\r\n", __func__, i);
            goto err2;
        }

        rv = inet_aton(tmp->valuestring, &in_addr);
        if (rv == 0) {
            BIP_ERROR("%s: inet_aton failed cause %s\r\n", __func__, strerror(errno));
            goto err2;
        }
        bbmd_table[i].dst_addr.s_addr = in_addr.s_addr;

        tmp = cJSON_GetObjectItem(entry, "bcast_mask");
        if ((!tmp) || (tmp->type != cJSON_String)) {
            BIP_ERROR("%s: get BDT_Entry[%d] bcast_mask item failed\r\n", __func__, i);
            goto err2;
        }

        rv = inet_aton(tmp->valuestring, &in_addr);
        if (rv == 0) {
            BIP_ERROR("%s: inet_aton failed cause %s\r\n", __func__, strerror(errno));
            goto err2;
        } else if (!check_broadcast_mask(in_addr)) {
            BIP_ERROR("%s: invalid broadcast mask %s\r\n", __func__, tmp->valuestring);
            goto err2;
        }
        bbmd_table[i].bcast_mask.s_addr = in_addr.s_addr;

        tmp = cJSON_GetObjectItem(entry, "dst_port");
        if ((!tmp) || (tmp->type != cJSON_Number)) {
            BIP_ERROR("%s: get BDT_Entry[%d] dst_port item failed\r\n", __func__, i);
            goto err2;
        }
        port = (uint16_t)(tmp->valueint);
        bbmd_table[i].dst_port = htons(port);

        if (bbmd_table[i].dst_addr.s_addr == bip->sin.sin_addr.s_addr
                && bbmd_table[i].dst_port == bip->sin.sin_port) {
            BIP_WARN("%s: discard BDT_Entry[%d] conflict to local entry\r\n", __func__, i);
            cJSON_DeleteItemFromArray(bdt, i);
            bdt_size--;
            i--;
        }
    }

    bbmd_table[bdt_size].dst_addr = bip->sin.sin_addr;
    bbmd_table[bdt_size].dst_port = bip->sin.sin_port;
    if (router_bst) {
        bbmd_table[bdt_size].bcast_mask = bip->netmask;
    } else {
        bbmd_table[bdt_size].bcast_mask.s_addr = -1;
    }

    tmp = cJSON_GetObjectItem(cfg, "push_interval");
    if (tmp) {
        if (tmp->type != cJSON_Number) {
            BIP_ERROR("%s: push_interval should be number\r\n", __func__);
            goto err2;
        }
        if (tmp->valueint < 0) {
            BIP_ERROR("%s: push_interval should >= 0\r\n", __func__);
            goto err2;
        }
        if ((tmp->valueint > 0) && (tmp->valueint < BDT_PUSH_MIN_INTERVAL)) {
            BIP_WARN("%s: too small push_interval, set to default(%d)\r\n", __func__,
                BDT_PUSH_MIN_INTERVAL);
            bip->bbmd->push.interval = BDT_PUSH_MIN_INTERVAL;
        } else {
            bip->bbmd->push.interval = tmp->valueint;
        }
    } else {
        BIP_WARN("%s: no push_interval found, disable\r\n", __func__);
        bip->bbmd->push.interval = 0;
    }

    bip->bbmd->bdt = bbmd_table;
    bip->bbmd->bdt_size = bdt_size + 1;
    bip->bbmd->local_entry = bdt_size;

    BIP_VERBOS("%s: ok\r\n", __func__);
    
    return OK;

err2:
    free(bbmd_table);

err1:
    bbmd_destroy(bip->bbmd);
    bip->bbmd = NULL;
    
    return -EPERM;
}

static cJSON *bip_get_mib(datalink_base_t *dl_port);

/**
 * bip_port_create - 创建bip口的链路层对象
 *
 * @cfg: 端口配置信息
 *
 * @return: 成功返回端口的链路层对象指针，失败返回NULL
 *
 */
datalink_bip_t *bip_port_create(cJSON *cfg, cJSON *res)
{
    datalink_bip_t *bip;
    struct in_addr ip, mask;
    int sock_fd;
    int sockopt;
    const char *ifname, *res_type;
    uint16_t port;
    cJSON *tmp;
    int rv;

    if (cfg == NULL || res == NULL) {
        BIP_ERROR("%s: null argument\r\n", __func__);
        return NULL;
    }

    cfg = cJSON_Duplicate(cfg, true);
    if (cfg == NULL) {
        BIP_ERROR("%s: cjson duplicate failed\r\n", __func__);
        return NULL;
    }

    bip = (datalink_bip_t *)malloc(sizeof(datalink_bip_t));
    if (bip == NULL) {
        BIP_ERROR("%s: malloc datalink_bip_t failed\r\n", __func__);
        goto out0;
    }
    memset(bip, 0, sizeof(datalink_bip_t));

    tmp = cJSON_GetObjectItem(cfg, "resource_name");
    if ((!tmp) || (tmp->type != cJSON_String)) {
        BIP_ERROR("%s: get resource_name item failed\r\n", __func__);
        goto out1;
    }

    res_type = datalink_get_type_by_resource_name(res, tmp->valuestring);
    if (res_type == NULL) {
        BIP_ERROR("%s: get resource type failed by name: %s\r\n", __func__, tmp->valuestring);
        goto out1;
    }

    if (strcmp(res_type, "ETH")) {
        BIP_ERROR("%s: resource type is not ETH: %s\r\n", __func__, res_type);
        goto out1;
    }

    ifname = datalink_get_ifname_by_resource_name(res, tmp->valuestring);
    if (ifname == NULL) {
        BIP_ERROR("%s: get ifname by resource name:%s failed\r\n", __func__, tmp->valuestring);
        goto out1;
    }
    (void)strncpy(bip->ifname, ifname, sizeof(bip->ifname));

    cJSON_DeleteItemFromObject(cfg, "resource_name");

    tmp = cJSON_GetObjectItem(cfg, "udp_port");
    if (!tmp) {
        BIP_WARN("%s: udp_port not found, set to 47808\r\n", __func__);
        port = 47808;
    } else if (tmp->type != cJSON_Number) {
        BIP_ERROR("%s: get udp_port item failed\r\n", __func__);
        goto out1;
    } else {
        port = (uint16_t)(tmp->valueint);
        cJSON_DeleteItemFromObject(cfg, "udp_port");
    }
    port = htons(port);
    bip->sin.sin_port = port;

    /* setup sock_uip */
    sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_fd < 0) {
        BIP_ERROR("%s: create uip socket failed cause %s\r\n", __func__, strerror(errno));
        goto out1;
    }
    bip->sock_uip = sock_fd;

    /* allow us to use the same socket for sending and receiving */
    sockopt = 1;
    rv = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt));
    if (rv < 0) {
        BIP_ERROR("%s: uip setsockopt REUSEADDR failed cause %s\r\n", __func__, strerror(errno));
        goto out2;
    }

    /* allow us to send a broadcast */
    sockopt = 1;
    rv = setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, &sockopt, sizeof(sockopt));
    if (rv < 0) {
        BIP_ERROR("%s: uip setsockopt BROADCAST failed cause %s\r\n", __func__, strerror(errno));
        goto out2;
    }

    rv = bip_get_interface_info(ifname, &ip, &mask);
    if (rv < 0) {
        BIP_ERROR("%s: get interface info failed(%d)\r\n", __func__, rv);
        goto out2;
    }
    bip->sin.sin_family = AF_INET;
    bip->sin.sin_addr.s_addr = ip.s_addr;
    bip->sin.sin_port = port;
    memset(&(bip->sin.sin_zero), '\0', sizeof(bip->sin.sin_zero));

    bip->netmask = mask;
    bip->bcast_sin.sin_family = AF_INET;
    bip->bcast_sin.sin_addr.s_addr = ip.s_addr | (~mask.s_addr);
    bip->bcast_sin.sin_port = port;
    memset(&(bip->bcast_sin.sin_zero), '\0', sizeof(bip->bcast_sin.sin_zero));

    /* bind the socket to the local port number and IP address */
    rv = bind(sock_fd, (struct sockaddr *)&bip->sin, sizeof(struct sockaddr));
    if (rv < 0) {
        BIP_ERROR("%s: uip bind failed cause %s\r\n", __func__, strerror(errno));
        goto out2;
    }

    /* setup sock_bip */
    sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_fd < 0) {
        BIP_ERROR("%s: create bip socket failed cause %s\r\n", __func__, strerror(errno));
        goto out2;
    }
    bip->sock_bip = sock_fd;

    /* allow us to use the same socket for sending and receiving */
    sockopt = 1;
    rv = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt));
    if (rv < 0) {
        BIP_ERROR("%s: bip setsockopt REUSEADDR failed cause %s\r\n", __func__, strerror(errno));
        goto out3;
    }

    /* bind the socket to the local port number and broadcast address */
    rv = bind(sock_fd, (struct sockaddr *)&bip->bcast_sin, sizeof(struct sockaddr));
    if (rv < 0) {
        BIP_ERROR("%s: bip bind failed cause %s\r\n", __func__, strerror(errno));
        goto out3;
    }

    /* 配置BBMD口 */
    tmp = cJSON_GetObjectItem(cfg, "bbmd");
    if (!tmp) {
        BIP_WARN("%s: bbmd not found, set to off\r\n", __func__);
    } else {
        rv = bip_bbmd_config(bip, tmp);
        if (rv < 0) {
            BIP_ERROR("%s: config BBMD failed(%d)\r\n", __func__, rv);
            goto out3;
        }
        cJSON_DeleteItemFromObject(cfg, "bbmd");
    }

    /* 配置FD口 */
    tmp = cJSON_GetObjectItem(cfg, "fd_client");
    if (!tmp) {
        BIP_WARN("%s: fd_register not found, set to false\r\n", __func__);
    } else if (bip->bbmd) {
        BIP_ERROR("%s: bbmd and fd_client should not enable at same time\r\n", __func__);
        goto out4;
    } else {
        rv = bip_fd_register_config(bip, tmp);
        if (rv < 0) {
            BIP_ERROR("%s: config fd register failed(%d)\r\n", __func__, rv);
            goto out4;
        }
        cJSON_DeleteItemFromObject(cfg, "fd_client");
    }

    bip->dl.send_pdu = (int(*)(datalink_base_t *, bacnet_addr_t *, bacnet_buf_t *, bacnet_prio_t, 
        bool))bip_send_pdu;
    bip->dl.get_port_mib = datalink_get_mib;
    bip->watch_uip = NULL;
    bip->watch_bip = NULL;
    bip->dl.max_npdu_len = 1497;
    bip->dl.get_port_mib = bip_get_mib;
    
    list_add_tail(&bip->bip_list, &all_bip_list);

    cJSON *child = cfg->child;
    while (child) {
        BIP_WARN("%s: unknown cfg item: %s\r\n", __func__, child->string);
        child = child->next;
    }

    cJSON_Delete(cfg);

    BIP_VERBOS("%s: create sock_fd(%d) ok\r\n", __func__, sock_fd);
    
    return bip;

out4:
    if (bip->bbmd) {
        bbmd_destroy(bip->bbmd);
    }

out3:
    close(bip->sock_bip);

out2:
    close(bip->sock_uip);

out1:
    free(bip);

out0:
    cJSON_Delete(cfg);

    return NULL;
}

int bip_port_delete(datalink_bip_t *bip_port)
{
    if (bip_port == NULL) {
        BIP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    list_del(&bip_port->bip_list);

    if (bip_port->watch_uip) {
        (void)el_watch_destroy(&el_default_loop, bip_port->watch_uip);
    }

    if (bip_port->watch_bip) {
        (void)el_watch_destroy(&el_default_loop, bip_port->watch_bip);
    }

    close(bip_port->sock_bip);
    close(bip_port->sock_uip);

    if (bip_port->fd_client) {
        if (bip_port->fd_client->timer) {
            el_timer_destroy(&el_default_loop, bip_port->fd_client->timer);
        }
        free(bip_port->fd_client);
    }

    if (bip_port->bbmd) {
        bdt_push_stop(bip_port);
        bbmd_destroy(bip_port->bbmd);
    }
    
    free(bip_port);
    
    return OK;
}

datalink_bip_t *bip_next_port(datalink_bip_t *prev)
{
    if (!prev) {
        if (list_empty(&all_bip_list)) {
            return NULL;
        }
        
        return list_first_entry(&all_bip_list, datalink_bip_t, bip_list);
    }

    if (prev->bip_list.next != &all_bip_list) {
	    return list_next_entry(prev, bip_list);
    }

    return NULL;
}

int bip_init(void)
{   
    return OK;
}

int bip_startup(void)
{
    datalink_bip_t *bip, *bip_todel;

    list_for_each_entry(bip, &all_bip_list, bip_list) {
        bip->dl.tx_all = 0;
        bip->dl.tx_ok = 0;
        bip->dl.rx_all = 0;
        bip->dl.rx_ok = 0;
        
        bip->watch_uip = el_watch_create(&el_default_loop, bip->sock_uip, EPOLLIN);
        bip->watch_bip = el_watch_create(&el_default_loop, bip->sock_bip, EPOLLIN);
        if (bip->fd_client) {
            bip->fd_client->timer = el_timer_create(&el_default_loop, 0);
        }
        
	    if ((bip->watch_uip == NULL) || (bip->watch_bip == NULL)
	            || (bip->fd_client && bip->fd_client->timer == NULL)) {
	        BIP_ERROR("%s: watch or timer create failed\r\n", __func__);

	        list_for_each_entry(bip_todel, &all_bip_list, bip_list) {
	            if (bip_todel->watch_uip) {
	                (void)el_watch_destroy(&el_default_loop, bip_todel->watch_uip);
	                bip_todel->watch_uip = NULL;
	            }
	            if (bip_todel->watch_bip) {
	                (void)el_watch_destroy(&el_default_loop, bip_todel->watch_bip);
	                bip_todel->watch_bip = NULL;
	            }
                if (bip_todel->fd_client && bip_todel->fd_client->timer) {
                    (void)el_timer_destroy(&el_default_loop, bip_todel->fd_client->timer);
                    bip_todel->fd_client->timer = NULL;
                }
                bdt_push_stop(bip_todel);

	            if (bip_todel == bip) {
	                break;
	            }
	        }

	        goto out;
	    }

	    bip->watch_uip->handler = bip_event_handler;
	    bip->watch_uip->data = bip;
        bip->watch_bip->handler = bip_event_handler;
        bip->watch_bip->data = bip;
        if (bip->fd_client) {
            bip->fd_client->timer->handler = bip_fd_register_func;
            bip->fd_client->timer->data = bip;
        }
        bdt_push_start(bip);
    }

    BIP_VERBOS("%s: ok\r\n", __func__);
    bip_set_dbg_level(0);
    
    return OK;

out:

    return -EPERM;
}

void bip_stop(void)
{
    datalink_bip_t *bip;
    int rv;

    el_sync(&el_default_loop);
    
    list_for_each_entry(bip, &all_bip_list, bip_list) {
        if (bip->watch_uip) {
            rv = el_watch_destroy(&el_default_loop, bip->watch_uip);
            if (rv < 0) {
                BIP_ERROR("%s: el_watch_destroy sock_uip failed(%d)\r\n", __func__, rv);
            }
            bip->watch_uip = NULL;
        }

        if (bip->watch_bip) {
            rv = el_watch_destroy(&el_default_loop, bip->watch_bip);
            if (rv < 0) {
                BIP_ERROR("%s: el_watch_destroy sock_bip failed(%d)\r\n", __func__, rv);
            }
            bip->watch_bip = NULL;
        }

        if (bip->fd_client) {
            rv = el_timer_destroy(&el_default_loop, bip->fd_client->timer);
            if (rv < 0) {
                BIP_ERROR("%s: el_timer_destroy fd_timer failed(%d)\r\n", __func__, rv);
            }
            bip->fd_client->timer = NULL;
        }

        bdt_push_stop(bip);
    }

    el_unsync(&el_default_loop);
}

void bip_clean(void)
{   
    datalink_bip_t *each;
    
    while((each = list_first_entry_or_null(&all_bip_list, datalink_bip_t, bip_list))) {
	    list_del(&each->bip_list);
	    
	    close(each->sock_uip);
	    close(each->sock_bip);

        if (each->bbmd) {
            bbmd_destroy(each->bbmd);
        }
	    
	    free(each);
    }

    INIT_LIST_HEAD(&all_bip_list);

    return;
}

void bip_exit(void)
{
    bip_stop();
    bip_clean();
}

static cJSON *bip_port_get_bdt(datalink_bip_t *bip_port)
{
    bdt_entry_t *bdt;
    cJSON *BDT, *tmp;
    uint32_t bdt_size;

    BDT = cJSON_CreateArray();
    if (BDT == NULL) {
        BIP_ERROR("%s: create BDT array failed\r\n", __func__);
        return NULL;
    }

    RWLOCK_RDLOCK(&(bip_port->bbmd->bdt_lock));

    bdt = bip_port->bbmd->bdt;
    bdt_size = bip_port->bbmd->bdt_size;

    for (unsigned i = 0; i < bdt_size; i++) {
        tmp = cJSON_CreateObject();
        if (tmp == NULL) {
            BIP_ERROR("%s: create BDT entry item failed\r\n", __func__);
            RWLOCK_UNLOCK(&(bip_port->bbmd->bdt_lock));
            cJSON_Delete(BDT);
            return NULL;
        }

        cJSON_AddItemToArray(BDT, tmp);
        cJSON_AddStringToObject(tmp, "dst_bbmd", inet_ntoa(bdt[i].dst_addr));
        cJSON_AddNumberToObject(tmp, "dst_port", ntohs(bdt[i].dst_port));
        cJSON_AddStringToObject(tmp, "bcast_mask", inet_ntoa(bdt[i].bcast_mask));
    }

    RWLOCK_UNLOCK(&(bip_port->bbmd->bdt_lock));
    return BDT;
}

static cJSON *bip_get_bdt(cJSON *request)
{
    cJSON *reply;
    datalink_bip_t *bip_port;
    cJSON *BDT, *tmp;
    uint32_t port_id;
    bool exist;
    int error_code;
    const char *reason;
    
    tmp = cJSON_GetObjectItem(request, "port_id");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        BIP_ERROR("%s: get port_id item failed\r\n", __func__);
        error_code = -1;
        reason = "get port_id item failed";
        goto err;
    }

    if (tmp->valueint < 0) {
        BIP_ERROR("%s: invalid port_id(%d)\r\n", __func__, tmp->valueint);
        error_code = -1;
        reason = "invalid port_id";
        goto err;
    }

    port_id = (uint32_t)tmp->valueint;

    exist = false;
    list_for_each_entry(bip_port, &all_bip_list, bip_list) {
        if (bip_port->dl.port_id == port_id) {
            exist = true;
            break;
        }
    }

    if (exist == false) {
        BIP_ERROR("%s: port_id(%d) is not present\r\n", __func__, port_id);
        error_code = -1;
        reason = "bip port_id is not present";
        goto err;
    }

    if (bip_port->bbmd == NULL) {
        BIP_ERROR("%s: port_id(%d) BBMD is disabled\r\n", __func__, port_id);
        error_code = -1;
        reason = "bip port BBMD is disabled";
        goto err;
    }

    BDT = bip_port_get_bdt(bip_port);
    if (BDT == NULL) {
        BIP_ERROR("%s: bip_port_get_bdt failed\r\n", __func__);
        return NULL;
    }
    
    reply = cJSON_CreateObject();
    if (!reply) {
        BIP_ERROR("%s: create reply object failed\r\n", __func__);
        cJSON_Delete(BDT);
        return NULL;
    }

    cJSON_AddItemToObject(reply, "result", BDT);

    return reply;

err:
    reply = cJSON_CreateObject();
    if (reply == NULL) {
        BIP_ERROR("%s: create reply object failed\r\n", __func__);
        return NULL;
    }

    cJSON_AddNumberToObject(reply, "error_code", error_code);
    cJSON_AddStringToObject(reply, "reason", reason);
    
    return reply;
}

cJSON *bip_set_bdt(cJSON *request)
{
    bdt_entry_t *bbmd_table;
    cJSON *entry, *tmp, *reply, *bdt;
    datalink_bip_t *bip_port;
    struct in_addr in_addr;
    uint32_t port_id;
    bool exist;
    int bdt_size;
    int i;
    int error_code;
    const char *reason;

    tmp = cJSON_GetObjectItem(request, "port_id");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        BIP_ERROR("%s: get port_id item failed\r\n", __func__);
        error_code = -1;
        reason = "get port_id item failed";
        goto err;
    }

    if (tmp->valueint < 0) {
        BIP_ERROR("%s: invalid port_id(%d)\r\n", __func__, tmp->valueint);
        error_code = -1;
        reason = "invalid port_id";
        goto err;
    }
    
    port_id = (uint32_t)tmp->valueint;

    bdt = cJSON_GetObjectItem(request, "BDT");
    if ((bdt == NULL) || (bdt->type != cJSON_Array)) {
        BIP_ERROR("%s: get BDT array failed\r\n", __func__);
        error_code = -1;
        reason = "get BDT array failed";
        goto err;
    }

    exist = false;
    list_for_each_entry(bip_port, &all_bip_list, bip_list) {
        if (bip_port->dl.port_id == port_id) {
            exist = true;
            break;
        }
    }
    
    if (exist == false) {
        BIP_ERROR("%s: port_id(%d) is not present\r\n", __func__, port_id);
        error_code = -1;
        reason = "bip port_id is not present";
        goto err;
    }

    if (bip_port->bbmd == NULL) {
        BIP_ERROR("%s: port_id(%d) BBMD is disabled\r\n", __func__, port_id);
        error_code = -1;
        reason = "bip port BBMD is disabled";
        goto err;
    }
    
    bdt_size = cJSON_GetArraySize(bdt);
    if (bdt_size <= 0) {
        BIP_ERROR("%s: get BDT array size failed(%d)\r\n", __func__, bdt_size);
        error_code = -1;
        reason = "bip get BDT array size failed";
        goto err;
    }

    bbmd_table = (bdt_entry_t *)malloc(sizeof(bdt_entry_t) * bdt_size);
    if (bbmd_table == NULL) {
        BIP_ERROR("%s: malloc bbmd_table failed\r\n", __func__);
        error_code = -1;
        reason = "bip malloc bbmd_table failed";
        goto err;
    }
    memset(bbmd_table, 0, sizeof(bdt_entry_t) * bdt_size);

    for (i = 0; i < bdt_size; i++) {
        entry = cJSON_GetArrayItem(bdt, i);
        if ((entry == NULL) || (entry->type != cJSON_Object)) {
            BIP_ERROR("%s: get BDT[%d] item failed\r\n", __func__, i);
            free(bbmd_table);
            error_code = -1;
            reason = "bip get BDT entry item failed";
            goto err;
        }

        tmp = cJSON_GetObjectItem(entry, "dst_bbmd");
        if ((tmp == NULL) || (tmp->type != cJSON_String)) {
            BIP_ERROR("%s: get BDT[%d] dst_bbmd item failed\r\n", __func__, i);
            free(bbmd_table);
            error_code = -1;
            reason = "bip get BDT dst_bbmd item failed";
            goto err;
        }

        if (inet_aton(tmp->valuestring, &in_addr) == 0) {
            BIP_ERROR("%s: BDT[%d] dst_bbmd inet_aton failed cause %s\r\n", __func__, i,
                strerror(errno));
            free(bbmd_table);
            error_code = -1;
            reason = "bip inet_aton BDT dst_bbmd address failed";
            goto err;
        }
        bbmd_table[i].dst_addr.s_addr = in_addr.s_addr;

        tmp = cJSON_GetObjectItem(entry, "bcast_mask");
        if ((tmp == NULL) || (tmp->type != cJSON_String)) {
            BIP_ERROR("%s: get BDT[%d] bcast_mask item failed\r\n", __func__, i);
            free(bbmd_table);
            error_code = -1;
            reason = "bip get BDT bcast_mask item failed";
            goto err;
        }
        
        if (inet_aton(tmp->valuestring, &in_addr) == 0) {
            BIP_ERROR("%s: BDT[%d] bcast_mask inet_aton failed cause %s\r\n", __func__, i,
                strerror(errno));
            free(bbmd_table);
            error_code = -1;
            reason = "bip inet_aton BDT bcast_mask failed";
            goto err;
        }
        bbmd_table[i].bcast_mask.s_addr = in_addr.s_addr;

        tmp = cJSON_GetObjectItem(entry, "dst_port");
        if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
            BIP_ERROR("%s: get BDT[%d] dst_port item failed\r\n", __func__, i);
            free(bbmd_table);
            error_code = -1;
            reason = "bip get BDT dst_port item failed";
            goto err;
        }
        bbmd_table[i].dst_port = htons((uint16_t)(tmp->valueint));
    }

    reply = cJSON_CreateObject();
    if (reply == NULL) {
        BIP_ERROR("%s: create reply object failed\r\n", __func__);
        return NULL;
    }
    cJSON_AddNullToObject(reply, "result");
    
    RWLOCK_WRLOCK(&(bip_port->bbmd->bdt_lock));

    if (bip_port->bbmd->bdt) {
        free(bip_port->bbmd->bdt);
    }

    bip_port->bbmd->bdt = bbmd_table;
    bip_port->bbmd->bdt_size = bdt_size;

    RWLOCK_UNLOCK(&(bip_port->bbmd->bdt_lock));

    return reply;

err:
    reply = cJSON_CreateObject();
    if (reply == NULL) {
        BIP_ERROR("%s: create reply object failed\r\n", __func__);
        return NULL;
    }

    cJSON_AddNumberToObject(reply, "error_code", error_code);
    cJSON_AddStringToObject(reply, "reason", reason);
    
    return reply;
}

static cJSON *bip_port_get_fdt(datalink_bip_t *bip_port)
{
    fdt_entry_t *entry;
    cJSON *FDT, *tmp;
    int i;

    FDT = cJSON_CreateArray();
    if (FDT == NULL) {
        BIP_ERROR("%s: create FDT item failed\r\n", __func__);
        return NULL;
    }

    RWLOCK_RDLOCK(&(bip_port->bbmd->fdt_lock));

    unsigned now = el_current_millisecond();

    hash_for_each(bip_port->bbmd->fdt, i, entry, hnode) {
        tmp = cJSON_CreateObject();
        if (tmp == NULL) {
            BIP_ERROR("%s: create fdt entry object failed\r\n", __func__);
            RWLOCK_UNLOCK(&(bip_port->bbmd->fdt_lock));
            cJSON_Delete(FDT);
            return NULL;
        }

        unsigned remaining = el_timer_expire(entry->timer) - now;
        if (remaining > 65535*1000)
            continue;

        cJSON_AddItemToArray(FDT, tmp);
        cJSON_AddStringToObject(tmp, "dst_addr", inet_ntoa(entry->dst_addr));
        cJSON_AddNumberToObject(tmp, "dst_port", ntohs(entry->dst_port));
        cJSON_AddNumberToObject(tmp, "time-to-live", entry->time_to_live);
        cJSON_AddNumberToObject(tmp, "seconds_remaining", remaining/1000);
    }

    RWLOCK_UNLOCK(&(bip_port->bbmd->fdt_lock));
    return FDT;
}

static cJSON *bip_get_fdt(cJSON *request)
{
    cJSON *reply;
    datalink_bip_t *bip_port;
    cJSON *FDT, *tmp;
    bool exist;
    uint32_t port_id;
    int error_code;
    const char *reason;

    tmp = cJSON_GetObjectItem(request, "port_id");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        BIP_ERROR("%s: get port_id item failed\r\n", __func__);
        error_code = -1;
        reason = "get port_id item failed";
        goto err;
    }

    if (tmp->valueint < 0) {
        BIP_ERROR("%s: invalid port_id(%d)\r\n", __func__, tmp->valueint);
        error_code = -1;
        reason = "invalid port_id";
        goto err;
    }
    
    port_id = (uint32_t)tmp->valueint;

    exist = false;
    list_for_each_entry(bip_port, &all_bip_list, bip_list) {
        if (bip_port->dl.port_id == port_id) {
            exist = true;
            break;
        }
    }

    if (exist == false) {
        BIP_ERROR("%s: port_id(%d) is not present\r\n", __func__, port_id);
        error_code = -1;
        reason = "bip port_id is not present";
        goto err;
    }

    if (bip_port->bbmd == NULL) {
        BIP_ERROR("%s: port_id(%d) BBMD is disabled\r\n", __func__, port_id);
        error_code = -1;
        reason = "bip port BBMD is diabled";
        goto err;
    }

    FDT = bip_port_get_fdt(bip_port);
    if (FDT == NULL) {
        BIP_ERROR("%s: bip_port_get_fdt failed\r\n", __func__);
        return NULL;
    }
    
    reply = cJSON_CreateObject();
    if (reply == NULL) {
        BIP_ERROR("%s: create reply object failed\r\n", __func__);
        cJSON_Delete(FDT);
        return NULL;
    }
    cJSON_AddItemToObject(reply, "result", FDT);
    
    return reply;

err:
    reply = cJSON_CreateObject();
    if (reply == NULL) {
        BIP_ERROR("%s: create reply object failed\r\n", __func__);
        return NULL;
    }

    cJSON_AddNumberToObject(reply, "error_code", error_code);
    cJSON_AddStringToObject(reply, "reason", reason);
    
    return reply;
}

void bip_set_dbg_level(uint32_t level)
{
    bip_dbg_verbos = level & DEBUG_LEVEL_VERBOS;
    bip_dbg_warn = level & DEBUG_LEVEL_WARN;
    bip_dbg_err = level & DEBUG_LEVEL_ERROR;
}

cJSON *bip_get_status(cJSON *request)
{
    cJSON *reply, *tmp;
    char str[64];
    int error_code;
    const char *reason;
    
    if (request == NULL) {
        BIP_ERROR("%s: invalid request\r\n", __func__);
        return NULL;
    }

    tmp = cJSON_GetObjectItem(request, "service");
    if ((tmp == NULL) || (tmp->type != cJSON_String)) {
        BIP_ERROR("%s: get service item failed\r\n", __func__);
        error_code = -1;
        reason = "get service item failed";
        goto err;
    }

    (void)bactext_tolower(tmp->valuestring, str, sizeof(str));
    if (strcmp(str, "get bdt") == 0) {
        reply = bip_get_bdt(request);
    } else if (strcmp(str, "get fdt") == 0) {
        reply = bip_get_fdt(request);
    } else {
        BIP_ERROR("%s: invalid service(%s)\r\n", __func__, tmp->valuestring);
        error_code = -1;
        reason = "invalid service, service should be :\r\n"
            "get bdt\r\n"
            "get fdt\r\n";
        goto err;
    }

    return reply;

err:
    reply = cJSON_CreateObject();
    if (reply == NULL) {
        BIP_ERROR("%s: create reply object failed\r\n", __func__);
        return NULL;
    }

    cJSON_AddNumberToObject(reply, "error_code", error_code);
    cJSON_AddStringToObject(reply, "reason", reason);
    
    return reply;
}

static cJSON *bip_get_mib(datalink_base_t *dl_port)
{
    cJSON *result, *tmp;
    datalink_bip_t *bip;

    if (dl_port == NULL) {
        BIP_ERROR("%s: invalid argument\r\n", __func__);
        return NULL;
    }

    result = datalink_get_mib(dl_port);
    if (result == NULL) {
        BIP_ERROR("%s: get datalink mib failed\r\n", __func__);
        return NULL;
    }

    bip = (datalink_bip_t *)dl_port;
    if (bip->bbmd == NULL) {
        return result;
    }

    tmp = bip_port_get_bdt(bip);
    if (tmp == NULL) {
        BIP_ERROR("%s: get bdt failed\r\n", __func__);
        cJSON_Delete(result);
        return NULL;
    }
    cJSON_AddItemToObject(result, "BDT", tmp);

    tmp = bip_port_get_fdt(bip);
    if (tmp == NULL) {
        BIP_ERROR("%s: get fdt failed\r\n", __func__);
        cJSON_Delete(result);
        return NULL;
    }
    cJSON_AddItemToObject(result, "FDT", tmp);

    return result;
}

