/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * route.c
 * Original Author:  linzhixian, 2014-7-14
 *
 * 网络层路由模块
 *
 * History
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/prctl.h>

#include "misc/cJSON.h"
#include "bacnet/bacint.h"
#include "bacnet/config.h"
#include "route.h"
#include "protocol.h"
#include "network_def.h"
#include "misc/bits.h"
#include "misc/eventloop.h"
#include "bacnet/bacnet.h"
#include "misc/hashtable.h"
#include "misc/utils.h"

typedef struct whois_try_s {
    uint16_t dnet;
    uint16_t timeout;
    uint32_t timestamp;
    struct hlist_node hash_node;
    struct list_head route_node;
} whois_try_t;

/* 路由表 */
static struct {
    pthread_rwlock_t rwlock;
    uint32_t entry_num;                                     /* 路由表项数目 */
    uint32_t whois_num;                                     /* try的数目 */
    struct list_head entry_head;                            /* 可用路由链表头 */
    struct list_head whois_head;                            /* 正尝试的路由 */
    DECLARE_HASHTABLE(entry_table, ROUTE_TABLE_HASH_BITS);  /* 有效哈希表 */
    DECLARE_HASHTABLE(whois_table, ROUTE_TABLE_HASH_BITS);  /* whois哈希表 */
} route_table;

struct list_head *port_reachable_lists;         /* 各端口可达网络信息 */
bacnet_port_t *route_ports;                     /* 路由口 */
bool is_bacnet_router = false;
int route_port_nums = 0;

static route_entry_impl_t *__route_find_entry(uint16_t dnet)
{
    route_entry_impl_t *entry;

    hash_for_each_possible(route_table.entry_table, entry, hash_node, dnet) {
        if (entry->info.dnet == dnet) {
            return entry;
        }
    }

    return NULL;
}

static void __route_delete_entry(route_entry_impl_t *entry)
{
    hash_del(&entry->hash_node);
    __list_del_entry(&entry->route_node);
    __list_del_entry(&entry->reachable_node);
    (route_table.entry_num)--;
    free(entry);
}

static route_entry_impl_t *__route_add_entry(uint16_t dnet, bool busy, bacnet_port_t *out_port,
        bacnet_addr_t *next_mac)
{
    route_entry_impl_t *entry, *last;
    
    if (out_port == NULL) {
        NETWORK_ERROR("%s: null argument\r\n", __func__);
        return NULL;
    }

    if ((dnet == 0 && (route_port_nums > 1 || next_mac)) || (dnet == BACNET_BROADCAST_NETWORK)) {
        NETWORK_ERROR("%s: invalid dnet(%d)\r\n", __func__, dnet);
        return NULL;
    }

    if ((next_mac != NULL) && (next_mac->len == 0)) {
        NETWORK_ERROR("%s: next hop should not be broadcast(%d)\r\n", __func__, dnet);
        return NULL;
    }

    entry = (route_entry_impl_t *)malloc(sizeof(route_entry_impl_t));
    if (entry == NULL) {
        NETWORK_ERROR("%s: no enough memory\r\n", __func__);
        return NULL;
    }
    
    memset(entry, 0, sizeof(route_entry_impl_t));
    entry->info.dnet = dnet;
    entry->info.port = out_port;
    entry->info.busy = busy;
    entry->timestamp = el_current_second();
    if (next_mac != NULL) {
        entry->info.direct_net = false;
        entry->info.mac_len = next_mac->len;
        memcpy(entry->info.mac, next_mac->adr, next_mac->len);
    } else {
        entry->info.direct_net = true;
    }
    
    hash_add(route_table.entry_table, &entry->hash_node, dnet);
    list_add_tail(&(entry->reachable_node), &(port_reachable_lists[out_port->id]));
    list_add_tail(&(entry->route_node), &(route_table.entry_head));
    (route_table.entry_num)++;

    if (route_table.entry_num > MAX_ROUTE_ENTRY) {
        list_for_each_entry(last, &(route_table.entry_head), route_node){
            if (last->info.direct_net) {
                continue;
            }
            
            __route_delete_entry(last);
            break;
        }
    }

    return entry;
}

static int __route_update_entry(route_entry_impl_t *entry, bool busy, bacnet_port_t *out_port,
            bacnet_addr_t *next_mac)
{
    if ((entry == NULL) || (next_mac == NULL) || (out_port == NULL)) {
        NETWORK_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    if (next_mac->len == 0) {
        NETWORK_ERROR("%s: next hop should not be broadcast(%d)\r\n", __func__, entry->info.dnet);
        return -EPERM;
    }

    list_del_init(&(entry->route_node));
    list_del_init(&(entry->reachable_node));

    entry->info.busy = busy;
    entry->info.port = out_port;
    entry->info.mac_len = next_mac->len;
    memcpy(entry->info.mac, next_mac->adr, next_mac->len);
    entry->timestamp = el_current_second();

    list_add_tail(&(entry->route_node), &(route_table.entry_head));
    list_add_tail(&(entry->reachable_node), &(port_reachable_lists[out_port->id]));

    return OK;
}

static void __route_touch_entry(route_entry_impl_t *entry, bool update_ts)
{
    __list_del_entry(&(entry->route_node));
    list_add_tail(&(entry->route_node), &(route_table.entry_head));
    if (update_ts) {
        entry->timestamp = el_current_second();
    }
}

static whois_try_t *__route_find_whois(uint16_t dnet)
{
    whois_try_t *whois;

    hash_for_each_possible(route_table.whois_table, whois, hash_node, dnet) {
        if (whois->dnet == dnet) {
            return whois;
        }
    }

    return NULL;
}

static whois_try_t *__route_add_whois(uint16_t dnet)
{
    whois_try_t *whois, *last;

    whois = (whois_try_t *)malloc(sizeof(whois_try_t));
    if (whois == NULL) {
        NETWORK_ERROR("%s: no enough memory\r\n", __func__);
        return NULL;
    }

    whois->dnet = dnet;
    whois->timeout = WHOIS_INTERVAL_MIN;
    whois->timestamp = el_current_second();

    hash_add(route_table.whois_table, &(whois->hash_node), dnet);
    list_add_tail(&(whois->route_node), &(route_table.whois_head));
    (route_table.whois_num)++;

    if (route_table.whois_num > MAX_WHOIS_LOG) {
        last = list_first_entry(&(route_table.whois_head), whois_try_t, route_node);
        hash_del(&(last->hash_node));
        __list_del_entry(&(last->route_node));
        (route_table.whois_num)--;
        free(last);
    }

    return whois;
}

static void __route_delete_whois(uint16_t dnet)
{
    whois_try_t *whois;

    whois = __route_find_whois(dnet);
    if (whois == NULL) {
        return;
    }

    hash_del(&whois->hash_node);
    __list_del_entry(&whois->route_node);
    (route_table.whois_num)--;
    free(whois);
}

static void __route_update_whois(whois_try_t *whois, uint32_t retry)
{
    whois->timeout = retry;
    whois->timestamp = el_current_second();
    __list_del_entry(&(whois->route_node));
    list_add_tail(&(whois->route_node), &(route_table.whois_head));
}

bool route_find_entry(uint16_t dnet, route_entry_t *entry)
{
    route_entry_impl_t *tmp;

    RWLOCK_RDLOCK(&route_table.rwlock);

    tmp = __route_find_entry(dnet);
    if (tmp == NULL) {
        RWLOCK_UNLOCK(&route_table.rwlock);
        return false;
    }

    if (entry) {
        if ((tmp->info.busy)
                && ((uint32_t)(el_current_second() - tmp->timestamp) >= ROUTE_BUSY_TIMEOUT)) {
            NETWORK_WARN("%s: busy timeout(%d)\r\n", __func__, dnet);
            tmp->info.busy = false;
        }

        memcpy(entry, &tmp->info, sizeof(route_entry_t));
    }

    RWLOCK_UNLOCK(&route_table.rwlock);

    return true;
}

bool route_try_find_entry(uint16_t dnet, route_entry_t *entry, bool *need_whois)
{
    route_entry_impl_t *tmp;
    whois_try_t *whois;
    uint32_t past;
    bool find;
    
    find = false;

    RWLOCK_WRLOCK(&route_table.rwlock);

    tmp = __route_find_entry(dnet);
    if (tmp == NULL) {
        whois = __route_find_whois(dnet);
        if (whois == NULL) {
            __route_add_whois(dnet);
            if (need_whois) {
                *need_whois = true;
            }
            goto out;
        }

        past = el_current_second() - whois->timestamp;
        if (need_whois) {
            if (past < whois->timeout) {
                *need_whois = false;
                goto out;
            }
            *need_whois = true;
        }
        __route_update_whois(whois, cal_retry_timeout(whois->timeout, past, WHOIS_INTERVAL_MIN,
            WHOIS_INTERVAL_MAX, whois->timeout * 3 / 4));
    } else {
        find = true;
        past = el_current_second() - tmp->timestamp;
        if ((tmp->info.busy) && (past >= ROUTE_BUSY_TIMEOUT)) {
            NETWORK_WARN("%s: busy timeout(%d)\r\n", __func__, dnet);
            tmp->info.busy = false;
        }

        if (entry) {
            memcpy(entry, &tmp->info, sizeof(route_entry_t));
        }
    }

out:
    RWLOCK_UNLOCK(&route_table.rwlock);

    return find;
}

static int route_add_direct_entry(uint16_t dnet, bacnet_port_t *out_port)
{
    route_entry_impl_t *entry;
    int rv;

    if (out_port == NULL) {
        NETWORK_ERROR("%s: null argument\r\n", __func__);
        return -EINVAL;
    }
    
    if ((dnet == 0 && route_port_nums > 1) || (dnet == BACNET_BROADCAST_NETWORK)) {
        NETWORK_ERROR("%s: invalid dnet(%d)\r\n", __func__, dnet);
        return -EINVAL;
    }

    rv = OK;
    
    RWLOCK_WRLOCK(&route_table.rwlock);

    entry = __route_find_entry(dnet);
    if (entry == NULL) {
        entry = __route_add_entry(dnet, false, out_port, NULL);
        if (entry == NULL) {
            NETWORK_ERROR("%s: add dnet(%d) entry failed\r\n", __func__, dnet);
            rv = -EPERM;
        }

        NETWORK_VERBOS("%s: add dnet(%d)\r\n", __func__, dnet);
        __route_delete_whois(dnet);
    } else {
        NETWORK_ERROR("%s: add failed cause dnet(%d) entry is already present\r\n", __func__, dnet);
        rv = -EPERM;
    }

    RWLOCK_UNLOCK(&route_table.rwlock);

    return rv;
}

int route_update_dynamic_entry(uint16_t dnet, network_reachable_status_t state,
        bacnet_port_t *out_port, bacnet_addr_t *next_mac)
{
    route_entry_impl_t *entry;
    int rv;
    
    if ((out_port == NULL) || (next_mac == NULL)) {
        NETWORK_ERROR("%s: null argument\r\n", __func__);
        return -EINVAL;
    }

    if ((dnet == 0) || (dnet == BACNET_BROADCAST_NETWORK)) {
        NETWORK_ERROR("%s: invalid dnet(%d)\r\n", __func__, dnet);
        return -EINVAL;
    }

    if ((state < NETWORK_REACHABLE) || (state > NETWORK_REACHABLE_REVERSE)) {
        NETWORK_ERROR("%s: invalid state(%d)\r\n", __func__, state);
        return -EINVAL;
    }

    if (next_mac->len == 0) {
        NETWORK_ERROR("%s: next hop should not be broadcast(%d)\r\n", __func__, dnet);
        return -EINVAL;
    }

    rv = OK;

    RWLOCK_WRLOCK(&route_table.rwlock);
    
    entry = __route_find_entry(dnet);
    if (entry == NULL) {
        switch (state) {
        case NETWORK_REACHABLE:
        case NETWORK_REACHABLE_REVERSE:
        case NETWORK_UNREACHABLE_TEMPORARILY:
            entry = __route_add_entry(dnet, state == NETWORK_UNREACHABLE_TEMPORARILY, out_port,
                next_mac);
            if (entry == NULL) {
                NETWORK_ERROR("%s: add dnet(%d) entry failed\r\n", __func__, dnet);
                rv = -EPERM;
            }

            NETWORK_VERBOS("%s: add dnet(%d) entry by %d\r\n", __func__, dnet, state);
            __route_delete_whois(dnet);
            break;
            
        case NETWORK_UNREACHABLE_PERMANENTLY:
            NETWORK_WARN("%s: dnet(%d) duplicated delete\r\n", __func__, dnet);
            break;
        }
    } else if (entry->info.direct_net) {
        NETWORK_ERROR("%s: try dynamic update to direct dnet(%d)\r\n", __func__, dnet);
        rv = -EPERM;
    } else if ((out_port == entry->info.port)
            && (memcmp(&next_mac->len, &entry->info.mac_len, next_mac->len + 1) == 0)) {
        switch (state) {
        case NETWORK_UNREACHABLE_PERMANENTLY:
            NETWORK_WARN("%s: dnet(%d) unreachable permanently\r\n", __func__, dnet);
            __route_delete_entry(entry);
            break;
        
        case NETWORK_UNREACHABLE_TEMPORARILY:
            if (!entry->info.busy) {
                NETWORK_WARN("%s: dnet(%d) busy\r\n", __func__, dnet);
                entry->info.busy = true;
            }
            __route_touch_entry(entry, true);
            break;
        
        case NETWORK_REACHABLE:
            if (entry->info.busy) {
                NETWORK_WARN("%s: dnet(%d) assume leave busy\r\n", __func__, dnet);
                entry->info.busy = false;
            }
            __route_touch_entry(entry, true);
            break;
        
        case NETWORK_REACHABLE_REVERSE:
            /* 有可能busy的网络有反向包到达，此时如果未timeout，不更新busy及timestamp */
            if ((entry->info.busy)
                    && ((uint32_t)(el_current_second() - entry->timestamp) >= ROUTE_BUSY_TIMEOUT)) {
                NETWORK_WARN("%s: busy timeout(%d)\r\n", __func__, dnet);
                entry->info.busy = false;
            }
            __route_touch_entry(entry, !entry->info.busy);
            break;
        }
    } else if (state == NETWORK_UNREACHABLE_PERMANENTLY) {
        NETWORK_ERROR("%s: dnet(%d) try delete from wrong source\r\n", __func__, dnet);
        rv = -EPERM;
    } else {
        NETWORK_WARN("%s: dnet(%d) route changed by %d\r\n", __func__, dnet, state);
        rv = __route_update_entry(entry, state == NETWORK_UNREACHABLE_TEMPORARILY, out_port,
            next_mac);
        if (rv < 0) {
            NETWORK_ERROR("%s: update dnet(%d) entry failed(%d)\r\n", __func__, dnet, rv);
        }
    }

    RWLOCK_UNLOCK(&route_table.rwlock);

    return rv;
}

/**
 * route_port_show_routing_table - 查看路由表
 *
 * @return: void
 *
 */
void route_table_show(void)
{
    route_entry_impl_t *entry;
    int i;

    printf("\r\n[Dnet]  [Port]  [States]  [Next_Mac]\r\n");

    RWLOCK_RDLOCK(&route_table.rwlock);

    list_for_each_entry(entry, &(route_table.entry_head), route_node) {
        printf(" %-5d   %-5d   ", entry->info.dnet, entry->info.port->id);

        printf("%s", (entry->info.direct_net)? "D|": "  ");

        if (entry->info.busy) {
            printf("T     ");
        } else {
            printf("R     ");
        }

        if (entry->info.mac_len == 0) {
            printf("%-20s", "None");
        } else {
            for (i = 0; i < entry->info.mac_len; i++) {
                printf("%02x ", entry->info.mac[i]);
            }
        }
        printf("\r\n");
    }

    RWLOCK_UNLOCK(&route_table.rwlock);
}

/* 路由表初始化 */
int route_table_init(void)
{
    int rv;
    
    route_table.entry_num = 0;
    route_table.whois_num = 0;
    
    INIT_LIST_HEAD(&(route_table.entry_head));
    INIT_LIST_HEAD(&(route_table.whois_head));
    hash_init(route_table.entry_table);
    hash_init(route_table.whois_table);
    
    rv = pthread_rwlock_init(&route_table.rwlock, NULL);
    if (rv) {
        NETWORK_ERROR("%s: route_table_rwlock init failed(%d)\r\n", __func__, rv);
        return -EPERM;
    }
    
    return rv;
}

/* 路由表反初始化 */
void route_table_destroy(void)
{
    route_entry_impl_t *entry, *tmp_e;
    whois_try_t *whois, *tmp_w;

    RWLOCK_WRLOCK(&route_table.rwlock);

    list_for_each_entry_safe(entry, tmp_e, &(route_table.entry_head), route_node) {
        free(entry);
    }

    list_for_each_entry_safe(whois, tmp_w, &(route_table.whois_head), route_node) {
        free(whois);
    }

    route_table.entry_num = 0;
    route_table.whois_num = 0;

    INIT_LIST_HEAD(&(route_table.entry_head));
    INIT_LIST_HEAD(&(route_table.whois_head));
    hash_init(route_table.entry_table);
    hash_init(route_table.whois_table);

    RWLOCK_UNLOCK(&route_table.rwlock);

    (void)pthread_rwlock_destroy(&route_table.rwlock);
}

/**
 * route_port_get_list_head - 获取路由口头节点
 *
 * @return: 返回路由口头节点
 *
 */
bacnet_port_t *route_port_get_list_head(void)
{
    bacnet_port_t *head;
    
    if (route_port_nums < 1) {
        head = NULL;
    } else {
        head = route_ports;
        if (!(head->valid)) {
            head = NULL;
        }
    }

    return head;
}

/**
 * route_port_list_show - 查看所有路由口信息
 *
 * @return: void
 *
 */
void route_ports_show(void)
{
    bacnet_port_t *port;
    int i;

    printf("[PORT_ID]\t\t[STATE]\t\t[NET_NUM]\t\t[MAC_LEN]\t\t[MAC]\r\n");

    for (i = 0; i < route_port_nums; i++) {
        port = &(route_ports[i]);
        if (!(port->valid)) {
            continue;
        }
        
        printf("%d\r\n", port->id);
    }
}

bacnet_port_t *route_port_find_by_id(uint32_t port_id)
{
    bacnet_port_t *port;

    if (port_id >= route_port_nums) {
        NETWORK_ERROR("%s: invalid port_id(%d)\r\n", __func__, port_id);
        return NULL;
    }

    port = &(route_ports[port_id]);
    if (!(port->valid)) {
        port = NULL;
    }

    return port;
}

static bool route_port_list_find_by_net(uint16_t net)
{
    bacnet_port_t *port;
    int i;

    for (i = 0; i < route_port_nums; i++) {
        port = &(route_ports[i]);
        if ((port->valid) && (port->net == net)) {
            return true;
        }
    }

    return false;
}

/**
 * route_port_show_reachable_list - 查看指定路由口的可达网络列表
 *
 * @port_id: 指定的端口号
 *
 * @return: void
 *
 */
void route_port_show_reachable_list(uint32_t port_id)
{
    bacnet_port_t *port;
    route_entry_impl_t *entry;
    int i;

    if (port_id >= route_port_nums) {
        printf("route_port_show_reachable_list: invalid port_id(%d)\r\n", port_id);
        return;
    }

    printf("[DNET]\t[Direct_Net]\t[MAC_LEN]\t[MAC]\t\t[STATE]\r\n");
    
    port = &(route_ports[port_id]);
    if (!(port->valid)) {
        printf("route_port_show_reachable_list: invalid port_id(%d)\r\n", port_id);
        return;
    }

    RWLOCK_RDLOCK(&route_table.rwlock);
    
    list_for_each_entry(entry, &(port_reachable_lists[port->id]), reachable_node) {
        printf("%5d\t%s\t%d\t", entry->info.dnet, (entry->info.direct_net)? "TRUE": "FALSE",
            entry->info.mac_len);
    
        for (i = 0; i < entry->info.mac_len; i++) {
            printf("%02x ", entry->info.mac[i]);
        }
        printf("\t%d\r\n", entry->info.busy);
    }
    
    RWLOCK_UNLOCK(&route_table.rwlock);
}

/* 获取指定路由口的可达网络号列表 */
static int route_port_get_reachable_net(bacnet_port_t *port, uint16_t dnet_list[], int list_num)
{
    route_entry_impl_t *entry;
    int i;
    
    if ((port == NULL) || (dnet_list == NULL) || (list_num <= 0)) {
        NETWORK_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    i = 0;

    RWLOCK_RDLOCK(&route_table.rwlock);

    list_for_each_entry(entry, &(port_reachable_lists[port->id]), reachable_node) {
        dnet_list[i++] = entry->info.dnet;
        if (i == list_num) {
            NETWORK_WARN("%s: dnet buffer is already full\r\n", __func__);
            break;
        }
    }

    RWLOCK_UNLOCK(&route_table.rwlock);

    return i;
}

static int route_port_get_reachable_net_exclude_port_unlock(uint32_t port_id, uint16_t dnet_list[],
            int list_num)
{
    bacnet_port_t *port;
    int rest;
    int nets;
    int i;
    
    if ((port_id >= route_port_nums) || (dnet_list == NULL) || (list_num <= 0)) {
        NETWORK_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    nets = 0;
    for (i = 0; i < route_port_nums; i++) {
        port = &(route_ports[i]);
        if ((port->valid) && (i != port_id)) {
            rest = list_num - nets;
            nets += route_port_get_reachable_net(port, &dnet_list[nets], rest);
            if (nets == rest) {
                goto exit;
            }
        }
    }

exit:

    return nets;
}

/**
 * route_get_reachable_net_exclude_port - 获取指定端口以外的其他所有端口的可达网络号列表
 *
 * @port_id: 指定端口
 * @dnet_list: 可达网络号列表
 * @list_num: 可达网络列表大小
 *
 * @return: 成功返回实际可达网络号大小，失败返回负数
 *
 */
int route_port_get_reachable_net_exclude_port(uint32_t port_id, uint16_t dnet_list[], int list_num)
{
    bacnet_port_t *port;
    int rest;
    int nets;
    int i;
    
    if ((port_id >= route_port_nums) || (dnet_list == NULL) || (list_num <= 0)) {
        NETWORK_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }
    
    RWLOCK_RDLOCK(&(route_table.rwlock));

    nets = 0;
    for (i = 0; i < route_port_nums; i++) {
        port = &(route_ports[i]);
        if ((port->valid) && (i != port_id)) {
            rest = list_num - nets;
            nets += route_port_get_reachable_net(port, &dnet_list[nets], rest);
            if (nets == rest) {
                goto exit;
            }
        }
    }

exit:

    RWLOCK_UNLOCK(&(route_table.rwlock));

    return nets;
}

/**
 * route_get_reachable_net_on_mac - 获取指定端口指定节点的可达网络号列表
 *
 * @port_id: 指定端口
 * @mac: 指定对方节点
 * @dnet_list: 可达网络号列表
 * @list_num: 可达网络列表大小
 * @return: 成功返回实际可达网络号大小，失败返回负数
 */
int route_port_get_reachable_net_on_mac_set_busy(uint32_t port_id, bool busy, bacnet_addr_t *mac,
        uint16_t dnet_list[], int list_num)
{
    route_entry_impl_t *entry;
    int nets;

    if ((port_id >= route_port_nums) || (mac == NULL) || (dnet_list == NULL) || (list_num <= 0)) {
        NETWORK_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    RWLOCK_RDLOCK(&(route_table.rwlock));

    nets = 0;
    list_for_each_entry(entry, &port_reachable_lists[port_id], reachable_node) {
        if (memcmp(&mac->len, &entry->info.mac_len, mac->len + 1) == 0) {
            entry->info.busy = busy;
            __route_touch_entry(entry, true);

            dnet_list[nets++] = entry->info.dnet;
            if (nets == list_num) {
                NETWORK_WARN("%s: dnet buffer is already full\r\n", __func__);
                break;
            }
        }
    }

    RWLOCK_UNLOCK(&(route_table.rwlock));

    return nets;
}

/**
 * route_port_unicast_pdu - 指定端口发送报文
 *
 * @port: 发送出口
 * @dst_mac: 数据链路层的目的mac
 * @npdu: 待发送的网络层报文
 * @npci_info: 报文信息
 *
 * 当dst_mac为NULL或者dst_mac->len为0时，表示在指定端口广播报文
 *
 * @return: 成功返回0，失败返回负数
 *
 */
int route_port_unicast_pdu(bacnet_port_t *port, bacnet_addr_t *dst_mac, bacnet_buf_t *npdu, 
        npci_info_t *npci_info)
{
    bacnet_prio_t prio;
    bool der;
    int rv;
    
    if ((npdu == NULL) || (npdu->data == NULL) || (npdu->data_len == 0) || (port == NULL) 
            || (npci_info == NULL)) {
        NETWORK_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    prio = (npci_info->control) & 0x03;
    der = (npci_info->control & BIT2)? true: false;

    rv = port->dl->send_pdu(port->dl, dst_mac, npdu, prio, der);
    if (rv < 0) {
        NETWORK_ERROR("%s: port(%d) send pdu failed(%d)\r\n", __func__, port->id, rv);
    }

    NETWORK_VERBOS("%s: port(%d), rv = %d\r\n", __func__, port->id, rv);
    
    return rv;
}

/**
 * route_port_broadcast_pdu - 广播报文
 *
 * @ex_port: 广播排除的端口
 * @npdu: 待发送的网络层报文
 * @npci_info: 报文信息
 *
 * 从ex_port以外的其它所有端口广播指定报文
 *
 * @return: 成功返回0，失败返回负数
 *
 */
int route_port_broadcast_pdu(bacnet_port_t *ex_port, bacnet_buf_t *npdu, npci_info_t *npci_info)
{
    bacnet_port_t *port;
    bacnet_prio_t prio;
    bool der;
    int i;
    int rv;

    if ((npdu == NULL) || (npdu->data == NULL) || (npdu->data_len == 0) || (npci_info == NULL)) {
        NETWORK_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    prio = (npci_info->control) & 0x03;
    der = (npci_info->control & BIT2)? true: false;

    for (i = 0; i < route_port_nums; i++) {
        port = &(route_ports[i]);
        if (port->valid && (port != ex_port)) {
            rv = port->dl->send_pdu(port->dl, NULL, npdu, prio, der);
            if (rv < 0) {
                NETWORK_ERROR("%s: port(%d) send pdu failed(%d)\r\n", __func__, i, rv);
            }
        }
    }

    return OK;
}

/* 路由口数据链路层初始化 */
int route_port_init(cJSON *cfg)
{
    cJSON *port_array, *port, *tmp, *res;
    bacnet_port_t *route_port;
    int i;
    int size;
    int rv;

    if (cfg == NULL) {
        NETWORK_ERROR("%s: NULL config info\r\n", __func__);
	    return -EINVAL;
    }

    port_array = cJSON_GetObjectItem(cfg, "port");
    if ((port_array == NULL) || (port_array->type != cJSON_Array)) {
        NETWORK_ERROR("%s: get port item failed\r\n", __func__);
        return -EPERM;
    }

    size = cJSON_GetArraySize(port_array);
    if (size <= 0) {
        NETWORK_ERROR("%s: no route_port is present\r\n", __func__);
        return -EPERM;
    }
    
    res = bacnet_get_resource_cfg();
    if (res == NULL) {
        NETWORK_ERROR("%s: load resource_cfg failed\r\n", __func__);
        return -EPERM;
    }

    route_ports = (bacnet_port_t *)malloc(sizeof(bacnet_port_t) * size);
    if (route_ports == NULL) {
        NETWORK_ERROR("%s: malloc route ports failed\r\n", __func__);
        cJSON_Delete(res);
        return -ENOMEM;
    }
    memset(route_ports, 0 , sizeof(bacnet_port_t) * size);

    port_reachable_lists = (struct list_head *)malloc(sizeof(struct list_head) * size);
    if (port_reachable_lists == NULL) {
        NETWORK_ERROR("%s: malloc reachable list failed\r\n", __func__);
        rv = -ENOMEM;
        goto out0;
    }

    for (i = 0; i < size; ++i) {
        INIT_LIST_HEAD(&port_reachable_lists[i]);
    }

    RWLOCK_WRLOCK(&(route_table.rwlock));

    route_port_nums = 0;
    
    for (i = 0; i < size; i++) {
        rv = -EPERM;
        
        port = cJSON_GetArrayItem(port_array, i);
        if ((port == NULL) || (port->type != cJSON_Object)) {
            NETWORK_ERROR("%s: get port_array[%d] item failed\r\n", __func__, i);
            goto out1;
        }
        
        tmp = cJSON_GetObjectItem(port, "enable");
        if ((tmp) && (tmp->type == cJSON_False)) {
            continue;
        }
        cJSON_DeleteItemFromObject(port, "enable");
        
        route_port = &(route_ports[route_port_nums]);
        route_port->id = route_port_nums;
        
        tmp = cJSON_GetObjectItem(port, "net_num");
        if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
            NETWORK_ERROR("%s: get port_array[%d] net_num failed\r\n", __func__, i);
            goto out1;
        }

        if ((tmp->valueint < 0) || (tmp->valueint >= BACNET_BROADCAST_NETWORK)
                || ((tmp->valueint == 0) && (size > 1))) {
            NETWORK_ERROR("%s: invalid port_array[%d] net_num(%d)\r\n", __func__, i, tmp->valueint);
            goto out1;
        }
        
        if (route_port_list_find_by_net(tmp->valueint)) {
            NETWORK_ERROR("%s: port_array[%d] net_num(%d) is already present\r\n", __func__, i, 
                tmp->valueint);
            goto out1;
        }
        route_port->net = (uint16_t)tmp->valueint;
        cJSON_DeleteItemFromObject(port, "net_num");

        route_port->dl = datalink_port_create(port, res);
        if (!route_port->dl) {
            NETWORK_ERROR("%s: port_array[%d] datalink_create failed\r\n", __func__, i);
            goto out1;
        }
        route_port->dl->port_id = route_port_nums;
        
        route_port->valid = true;
        route_port_nums++;
    }

    if (route_port_nums == 0) {
        NETWORK_ERROR("%s: no port is initialized\r\n", __func__);
        rv = -EPERM;
        goto out1;
    }

    if ((route_port_nums == 1) && (route_ports[0].net != 0)) {
        NETWORK_WARN("%s: net_num(%d) is ignored while only one port is present\r\n", __func__, 
            route_ports[0].net);
        route_ports[0].net = 0;
    } else if (route_port_nums > 1) {
        is_bacnet_router = true;
    }

    rv = OK;

out1:
    RWLOCK_UNLOCK(&(route_table.rwlock));

    if (rv < 0) {
        free(port_reachable_lists);
        port_reachable_lists = NULL;
    }

out0:
    if (rv < 0) {
        free(route_ports);
        route_ports = NULL;
    }
    
    cJSON_Delete(res);

    return rv;
}

void route_port_destroy(void)
{
    datalink_clean();

    free(port_reachable_lists);
    port_reachable_lists = NULL;

    free(route_ports);
    route_ports = NULL;
    route_port_nums = 0;
}

int route_table_config(cJSON *network_cfg)
{
    cJSON *route_table, *entry, *tmp;
    uint16_t dnet;
    uint32_t port_id;
    bacnet_addr_t next_hop;
    int i;
    int rv;

    if (network_cfg == NULL) {
        NETWORK_ERROR("%s: NULL network_cfg\r\n", __func__);
        return -EINVAL;
    }
    
    route_table = cJSON_GetObjectItem(network_cfg, "route_table");
    if (route_table == NULL) {
        return OK;
    }

    if (route_table->type != cJSON_Array) {
        NETWORK_ERROR("%s: invalid route_table item type\r\n", __func__);
        return -EPERM;
    }

    i = 0;
    cJSON_ArrayForEach(entry, route_table) {
        rv = -EPERM;
        if (entry->type != cJSON_Object) {
            NETWORK_ERROR("%s: get entry[%d] item failed\r\n", __func__, i);
            goto out;
        }

        tmp = cJSON_GetObjectItem(entry, "dnet");
        if (!tmp || tmp->type != cJSON_Number) {
            NETWORK_ERROR("%s: get entry[%d] dnet item failed\r\n", __func__, i);
            goto out;
        }
        dnet = (uint16_t)(tmp->valueint);

        tmp = cJSON_GetObjectItem(entry, "out_port");
        if (!tmp || tmp->type != cJSON_Number) {
            NETWORK_ERROR("%s: get entry[%d] out_port item failed\r\n", __func__, i);
            goto out;
        }
        port_id = tmp->valueint;

        if ((port_id >= route_port_nums) || (!(route_ports[port_id].valid))) {
            NETWORK_ERROR("%s: invalid out_port(%d)\r\n", __func__, port_id);
            goto out;
        }
        
        tmp = cJSON_GetObjectItem(entry, "next_hop");
        if (!tmp || tmp->type != cJSON_String) {
            NETWORK_ERROR("%s: get entry[%d] next_hop item failed\r\n", __func__, i);
            goto out;
        }

        memset(&next_hop, 0, sizeof(next_hop));
        rv = bacnet_macstr_to_array(tmp->valuestring, next_hop.adr, MAX_MAC_LEN);
        if (rv < 0) {
            NETWORK_ERROR("%s: macstr_to_array failed(%d)\r\n", __func__, rv);
            goto out;
        }
        next_hop.len = rv;

        rv = route_update_dynamic_entry(dnet, NETWORK_REACHABLE, &(route_ports[port_id]), &next_hop);
        if (rv < 0) {
            NETWORK_ERROR("%s: add dnet(%d) entry failed\r\n", __func__, dnet);
            goto out;
        }
        i++;
    }

    rv = OK;

out:

    return rv;
}

/**
 * route_startup - 路由启动
 *
 * @return: 成功返回0，否则返回负数
 *
 */
int route_startup(void)
{
    bacnet_port_t *port;
    uint16_t dnet_list[(MIN_NPDU - 3)/2];
    int list_num;
    uint32_t net_num;
    int i;
    int rv;

    /* 添加每个路由口的直连网络表项 */
    for (i = 0; i < route_port_nums; i++) {
        port = &(route_ports[i]);
        if (port->valid) {
            rv = route_add_direct_entry(port->net, port);
            if (rv < 0) {
                NETWORK_ERROR("%s: port(%d) add Dnet(%d) Entry failed\r\n", __func__, i, port->net);
            }
        }
    }
    
    /* 广播I_Am_Router_To_Network报文 */
    list_num = sizeof(dnet_list)/sizeof(dnet_list[0]);
    for (i = 0; i < route_port_nums; i++) {
        port = &(route_ports[i]);
        if (port->valid) {
            net_num = route_port_get_reachable_net_exclude_port_unlock(i, dnet_list, list_num);
            if (net_num > 0) {
                rv = send_I_Am_Router_To_Network(i, dnet_list, net_num);
                if (rv < 0) {
                    NETWORK_ERROR("%s: port(%d) send i am router failed(%d)\r\n", __func__, i, rv);
                }
            }
        }
    }
    
    NETWORK_VERBOS("%s: ok\r\n", __func__);

    return OK;
}

static cJSON *route_get_port_reachable_net(bacnet_port_t *port)
{
    route_entry_impl_t *entry;
    cJSON *route, *tmp;
    char mac[MAX_MAC_STR_LEN];
    int rv;

    route = cJSON_CreateArray();
    if (route == NULL) {
        NETWORK_ERROR("%s: create route array failed\r\n", __func__);
        return NULL;
    }

    RWLOCK_RDLOCK(&route_table.rwlock);

    list_for_each_entry(entry, &(port_reachable_lists[port->id]), reachable_node) {
        if ((entry->info.direct_net == true) || (entry->info.mac_len == 0)) {
            continue;
        }
        
        rv = bacnet_array_to_macstr(entry->info.mac, entry->info.mac_len, mac, sizeof(mac));
        if (rv < 0) {
            NETWORK_ERROR("%s: array to macstr failed(%d)\r\n", __func__, rv);
            RWLOCK_UNLOCK(&route_table.rwlock);
            cJSON_Delete(route);
            return NULL;
        }
    
        tmp = cJSON_CreateObject();
        if (tmp == NULL) {
            NETWORK_ERROR("%s: create entry item failed\r\n", __func__);
            RWLOCK_UNLOCK(&route_table.rwlock);
            cJSON_Delete(route);
            return NULL;
        }

        cJSON_AddNumberToObject(tmp, "net_num", entry->info.dnet);
        cJSON_AddStringToObject(tmp, "mac", mac);
        cJSON_AddItemToArray(route, tmp);
    }

    RWLOCK_UNLOCK(&route_table.rwlock);

    return route;
}

cJSON *route_get_port_mib(cJSON *request)
{
    bacnet_port_t *port;
    cJSON *result;
    cJSON *tmp;
    const char *reason;
    int error_code;

    if (request == NULL) {
        NETWORK_ERROR("%s: invalid argument\r\n", __func__);
        return NULL;
    }

    tmp = cJSON_GetObjectItem(request, "port_id");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        NETWORK_ERROR("%s: get port_id item failed\r\n", __func__);
        error_code = -1;
        reason = "get port_id item failed";
        goto err;
    }
    
    if ((tmp->valueint < 0) || (tmp->valueint >= route_port_nums)) {
        NETWORK_ERROR("%s: invalid port_id(%d)\r\n", __func__, tmp->valueint);
        error_code = -1;
        reason = "invalid port_id";
        goto err;
    }

    port = &(route_ports[tmp->valueint]);
    if (!(port->valid)) {
        NETWORK_ERROR("%s: invalid port_id(%d)\r\n", __func__, tmp->valueint);
        error_code = -1;
        reason = "invalid port_id";
        goto err;
    }

    if ((port->dl == NULL) || (port->dl->get_port_mib == NULL)) {
        NETWORK_ERROR("%s: null get_port_mib handler\r\n", __func__);
        error_code = -1;
        reason = "null argument";
        goto err;
    }

    result = port->dl->get_port_mib(port->dl);
    if (result) {
        tmp = route_get_port_reachable_net(port);
        if (tmp == NULL) {
            NETWORK_ERROR("%s: get port reachable net failed\r\n", __func__);
            error_code = -1;
            reason = "get port reachable net failed";
            goto err;
        }
        cJSON_AddItemToObject(result, "route", tmp);
    }
    
    return result;

err:
    result = cJSON_CreateObject();
    if (result == NULL) {
        NETWORK_ERROR("%s: create result object failed\r\n", __func__);
        return NULL;
    }

    cJSON_AddNumberToObject(result, "error_code", error_code);
    cJSON_AddStringToObject(result, "reason", reason);
    
    return result;
}

