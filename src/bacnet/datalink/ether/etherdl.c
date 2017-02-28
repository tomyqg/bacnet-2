/*
 * etherdl.c
 *
 *  Created on: Jun 30, 2015
 *      Author: lin
 */

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>

#include "etherdl_def.h"
#include "bacnet/etherdl.h"
#include "bacnet/bacint.h"
#include "bacnet/network.h"
#include "debug.h"

static const uint8_t broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static struct list_head all_ether_list;

bool ether_dbg_verbos = true;
bool ether_dbg_warn = true;
bool ether_dbg_err = true;

/**
 * ether_event_handler - ether收帧接口
 *
 * @handler: 事件句柄
 * @events: 发生的事件
 *
 * @return: void
 *
 */
static void ether_event_handler(el_watch_t *watch, int events)
{
    datalink_ether_t *ether;
    int fd;
    int rv;
    bacnet_addr_t src_mac;
    uint8_t *pdu;
    uint16_t pdu_len;
    DECLARE_BACNET_BUF(rx, MAX_ETH_802_3_LEN);

    if (!(events & EPOLLIN)) {
        ETH_ERROR("%s: invalid events\r\n", __func__);
        return;
    }

    ether = (datalink_ether_t *)watch->data;
    if (!ether) {
        ETH_ERROR("%s: null ether argument\r\n", __func__);
        return;
    }

    fd = el_watch_fd(watch);
    if (fd < 0) {
        ETH_ERROR("%s: invalid watch fd(%d)\r\n", __func__, fd);
        return;
    }

    if (ether->fd != fd) {
        ETH_ERROR("%s: port_id(%d) sock_fd(%d) wrong callback fd(%d)\r\n", __func__,
            ether->dl.port_id, ether->fd, fd);
        return;
    }

    bacnet_buf_init(&rx.buf, MAX_ETH_802_3_LEN);
    pdu = rx.buf.data;

    rv = recv(ether->fd, pdu, MAX_ETH_802_3_LEN, MSG_DONTWAIT);
    if (rv < 0) {
        ETH_ERROR("%s: recv failed cause %s\r\n", __func__, strerror(errno));
        return;
    }

    if (rv < ETH_802_3_HEADER) {
        ETH_ERROR("%s: not enough byte(%d) for header\r\n", __func__, rv);
        return;
    }

    (void)decode_unsigned16(&pdu[12], &pdu_len);
    if (pdu_len > rv - ETH_802_3_HEADER) {
        ETH_ERROR("%s: not enough byte(%d) for eth packet length(%d)\r\n", __func__, rv, pdu_len);
        return;
    }
    
    if (pdu_len < 3) {
        ETH_VERBOS("%s: too short mpdu length(%d), maybe not bacnet\r\n", __func__, pdu_len);
        return;
    }

    if (pdu[14] != 0x82 || pdu[15] != 0x82 || pdu[16] != 0x03) {    /* not bacnet */
        return;
    }

    ether->dl.rx_all++;
    
    if ((memcmp(&pdu[0], ether->mac, 6) != 0) && (memcmp(&pdu[0], broadcast_mac, 6) != 0)) {    /* not for me */
        return;
    }
    
    if (memcmp(&pdu[6], ether->mac, 6) == 0) {  /* from me */
        ETH_WARN("%s: send by myself\r\n", __func__);
        return;
    }

    src_mac.net = 0;
    src_mac.len = 6;
    memcpy(src_mac.adr, &pdu[6], 6);
    rx.buf.data += ETH_MPDU_ALL_HEADER;
    rx.buf.data_len = pdu_len - 3;

    ether->dl.rx_ok++;
    ETH_VERBOS("%s: received a pdu, length(%d)\r\n", __func__, pdu_len - 3);
    (void)network_receive_pdu(ether->dl.port_id, &rx.buf, &src_mac);   
}

/**
 * ether_send_pdu - ether发帧接口
 *
 * @ether: 发送端口
 * @dst_mac: 目的MAC地址
 * @buf: npdu发送缓冲区
 * @prio: 发送优先级，对于MSTP该参数被忽略
 * @der: 报文应答标志，true表示需要应答，false表示不需要应答
 *
 * 若dst_mac为NULL或者dst_mac->len为0，则表示在本地广播；
 *
 * @return: 成功返回0，失败返回负数
 */
static int ether_send_pdu(datalink_ether_t *ether, bacnet_addr_t *dst_mac, bacnet_buf_t *npdu,
            __attribute__ ((unused))bacnet_prio_t prio, bool der)
{
    int rv;
    uint8_t *pdu;
    uint8_t *dst_mac_addr;

    if (!ether) {
        ETH_ERROR("%s: null port\r\n", __func__);
        return -EINVAL;
    }

    ether->dl.tx_all++;
    
    if ((npdu == NULL) || (npdu->data == NULL) || (npdu->data_len == 0)
            || (npdu->data_len > MAX_ETH_NPDU)) {
        ETH_ERROR("%s: invalid npdu\r\n", __func__);
        return -EINVAL;
    }

    if (dst_mac) {
        if (dst_mac->len == 0) {
            dst_mac_addr = (uint8_t*)broadcast_mac;
        } else if (dst_mac->len == 6) {
            if (memcmp(dst_mac->adr, broadcast_mac, 6) == 0) {
                ETH_ERROR("%s: unicast to broadcast address\r\n", __func__);
                return -EINVAL;
            }
            dst_mac_addr = dst_mac->adr;
        } else {
            ETH_ERROR("%s: invalid dst mac len(%d)\r\n", __func__, dst_mac->len);
            return -EINVAL;
        }
    } else {
        dst_mac_addr = (uint8_t*)broadcast_mac;
    }
    
    rv = bacnet_buf_push(npdu, ETH_MPDU_ALL_HEADER);
    if (rv < 0) {
        ETH_ERROR("%s: buf push failed(%d)\r\n", __func__, rv);
        return rv;
    }

    pdu = npdu->data;
    memcpy(pdu, dst_mac_addr, 6);
    memcpy(pdu + 6, ether->mac, 6);
    encode_unsigned16(pdu + 12, npdu->data_len - 14);
    pdu[14] = 0x82;
    pdu[15] = 0x82;
    pdu[16] = 0x03;

    rv = send(ether->fd, pdu, npdu->data_len, MSG_DONTWAIT);
    if (rv < 0) {
        if (errno == EAGAIN) {
            ETH_ERROR("%s: write fd(%d) %d bytes overflow\r\n", __func__, ether->fd, npdu->data_len);
            rv = -EPERM;
        } else {
            ETH_ERROR("%s: write failed cause %s\r\n", __func__, strerror(errno));
            rv = -EPERM;
        }
    } else if (rv != npdu->data_len) {
        ETH_ERROR("%s: failed cause only write %d bytes in %d bytes to fd(%d)\r\n", __func__, rv,
            npdu->data_len, ether->fd);
        rv = -EPERM;
    } else {
        ETH_VERBOS("%s: write fd(%d) %d bytes ok\r\n", __func__, ether->fd, npdu->data_len);
        ether->dl.tx_ok++;
        rv = OK;
    }

    bacnet_buf_pull(npdu, ETH_MPDU_ALL_HEADER);
    
    return rv;
}

/**
 * ether_port_create - 创建bip口的链路层对象
 *
 * @cfg: 端口配置信息
 *
 * @return: 成功返回端口的链路层对象指针，失败返回NULL
 *
 */
datalink_ether_t *ether_port_create(cJSON *cfg, cJSON *res)
{
    datalink_ether_t *ether;
    struct sockaddr_ll addr_ll = {};
    struct ifreq ifr = {};
    cJSON *tmp;
    const char *ifname, *res_type;
    int len;

    if (cfg == NULL || res == NULL) {
        ETH_ERROR("%s: null argument\r\n", __func__);
        return NULL;
    }

    cfg = cJSON_Duplicate(cfg, true);
    if (cfg == NULL) {
        ETH_ERROR("%s: cjson duplicate failed\r\n", __func__);
        return NULL;
    }

    if (getuid() != 0) {
        ETH_ERROR("%s: ether port should only run under root priveleges", __func__);
        goto out0;
    }

    ether = (datalink_ether_t *)malloc(sizeof(datalink_ether_t));
    if (!ether) {
        ETH_ERROR("%s: malloc datalink_ether_t failed\r\n", __func__);
        goto out0;
    }
    memset(ether, 0, sizeof(datalink_ether_t));
    
    ether->dl.send_pdu = (int(*)(datalink_base_t *, bacnet_addr_t *, bacnet_buf_t *, bacnet_prio_t,
        bool))ether_send_pdu;
    ether->dl.get_port_mib = datalink_get_mib;
    ether->dl.max_npdu_len = MAX_ETH_NPDU;

    tmp = cJSON_GetObjectItem(cfg, "resource_name");
    if ((!tmp) || (tmp->type != cJSON_String)) {
        ETH_ERROR("%s: get resource_name item failed\r\n", __func__);
        goto out1;
    }

    res_type = datalink_get_type_by_resource_name(res, tmp->valuestring);
    if (res_type == NULL) {
        ETH_ERROR("%s: get resource type failed by name: %s\r\n", __func__, tmp->valuestring);
        goto out1;
    }

    if (strcmp(res_type, "ETH")) {
        ETH_ERROR("%s: resource type is not ETH: %s\r\n", __func__, res_type);
        goto out1;
    }

    ifname = datalink_get_ifname_by_resource_name(res, tmp->valuestring);
    if (ifname == NULL) {
        ETH_ERROR("%s: get ifname by resource name:%s failed\r\n", __func__, tmp->valuestring);
        goto out1;
    }

    cJSON_DeleteItemFromObject(cfg, "resource_name");
    
    len = strlen(ifname);
    if (len > sizeof(ifr.ifr_name)) {
        ETH_ERROR("%s: ifname overflow(%s)\r\n", __func__, ifname);
        goto out1;
    }
    memcpy(ifr.ifr_name, ifname, len);

    if ((ether->fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_802_2))) < 0) {
        ETH_ERROR("%s: create socket failed cause %s\r\n", __func__, strerror(errno));
        goto out1;
    }
    addr_ll.sll_protocol = htons(ETH_P_802_2);
    addr_ll.sll_family = PF_PACKET;

    if (ioctl(ether->fd, SIOCGIFINDEX, &ifr) < 0) {
        ETH_ERROR("%s: ioctl SIOCGIFINDEX failed cause %s\r\n", __func__, strerror(errno));
        goto out2;
    }
    addr_ll.sll_ifindex = ifr.ifr_ifindex;

    if (ioctl(ether->fd, SIOCGIFHWADDR, &ifr) < 0) {
        ETH_ERROR("%s: ioctl SIOCGIFHWADDR failed cause %s\r\n", __func__, strerror(errno));
        goto out2;
    }
    memcpy(ether->mac, ifr.ifr_hwaddr.sa_data, IFHWADDRLEN);

    if (bind(ether->fd, (struct sockaddr*)&addr_ll, sizeof(addr_ll)) != 0) {
        ETH_ERROR("%s: bind socket failed cause %s\r\n", __func__, strerror(errno));
        goto out2;
    }

    list_add_tail(&(ether->ether_list), &all_ether_list);

    cJSON *child = cfg->child;
    while (child) {
        ETH_WARN("%s: unknown cfg item: %s\r\n", __func__, child->string);
        child = child->next;
    }

    cJSON_Delete(cfg);
    
    return ether;

out2:
    close(ether->fd);

out1:
    free(ether);

out0:
    cJSON_Delete(cfg);

    return NULL;
}

int ether_port_delete(datalink_ether_t *ether_port)
{
    if (ether_port == NULL) {
        ETH_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    list_del(&ether_port->ether_list);

    if (ether_port->watch) {
        (void)el_watch_destroy(&el_default_loop, ether_port->watch);
    }
    
    close(ether_port->fd);

    free(ether_port);

    return OK;
}

/**
 * 取出下一个ether端口对象
 *
 * @prev: 上一个ether端口对象，如为NULL，取出第一个对象
 *
 * @return: 成功返回对象指针，失败返回NULL
 */
datalink_ether_t *ether_next_port(datalink_ether_t *prev)
{
    if (!prev) {
        if (list_empty(&all_ether_list)) {
            return NULL;
        }
        return list_first_entry(&all_ether_list, datalink_ether_t, ether_list);
    }

    if (prev->ether_list.next != &all_ether_list) {
        return list_next_entry(prev, ether_list);
    } else {
        return NULL;
    }
}

int ether_init(void)
{
    INIT_LIST_HEAD(&all_ether_list);
    return OK;
}

int ether_startup(void)
{
    datalink_ether_t *ether, *ether_todel;

    list_for_each_entry(ether, &all_ether_list, ether_list) {
        ether->dl.tx_all = 0;
        ether->dl.tx_ok = 0;
        ether->dl.rx_all = 0;
        ether->dl.rx_ok = 0;
        
        ether->watch = el_watch_create(&el_default_loop, ether->fd, EPOLLIN);
        if (ether->watch == NULL) {
            ETH_ERROR("%s: event watch create failed\r\n", __func__);
            list_for_each_entry(ether_todel, &all_ether_list, ether_list) {
                if (ether_todel == ether) {
                    break;
                }
                (void)el_watch_destroy(&el_default_loop, ether_todel->watch);
                ether_todel->watch = NULL;
            }
            return -EPERM;
        }
        ether->watch->handler = ether_event_handler;
        ether->watch->data = ether;
    }

    ETH_VERBOS("%s: ok\r\n", __func__);
    ether_set_dbg_level(0);

    return OK;
}

void ether_stop(void)
{
    datalink_ether_t *ether;
    int rv;

    list_for_each_entry(ether, &all_ether_list, ether_list) {
        if (ether->watch != NULL) {
            rv = el_watch_destroy(&el_default_loop, ether->watch);
            if (rv < 0) {
                ETH_ERROR("%s: event_loop_del failed(%d)\r\n", __func__, rv);
            }
            ether->watch = NULL;
        }
    }
}

void ether_clean(void)
{
    datalink_ether_t *each;

    while((each = list_first_entry_or_null(&all_ether_list, datalink_ether_t, ether_list))) {
        list_del(&each->ether_list);

        close(each->fd);

        free(each);
    }

    INIT_LIST_HEAD(&all_ether_list);
    
    return;
}

void ether_exit(void)
{
    ether_stop();
    ether_clean();
}

void ether_set_dbg_level(uint32_t level)
{
    ether_dbg_verbos = level & DEBUG_LEVEL_VERBOS;
    ether_dbg_warn = level & DEBUG_LEVEL_WARN;
    ether_dbg_err = level & DEBUG_LEVEL_ERROR;
}

cJSON *ether_get_status(cJSON *request)
{
    /* TODO */

    return NULL;
}

