/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * route.h
 * Original Author:  linzhixian, 2014-7-14
 *
 * ·��ģ��ͷ�ļ�
 *
 * History
 */

#ifndef _ROUTE_H_
#define _ROUTE_H_

#include <time.h>

#include "misc/list.h"
#include "bacnet/bacdef.h"
#include "npdu.h"
#include "bacnet/datalink.h"

#define MAX_ROUTE_ENTRY                         (1000)
#define ROUTE_TABLE_HASH_BITS                   (8)

#define MAX_WHOIS_LOG                           (250)
#define WHOIS_LOG_HASH_BITS                     (6)

#define WHOIS_INTERVAL_MIN                      (10)
#define WHOIS_INTERVAL_MAX                      (300)

/* ����timeout���Զ�ȡ��busy */
#define ROUTE_BUSY_TIMEOUT                      (30)

/* ���ֿɴ�״̬ */
typedef enum network_reachable_status_e {
    NETWORK_REACHABLE = 0,                  /* �ɴ� */
    NETWORK_UNREACHABLE_TEMPORARILY,        /* ��ʱ���ɴ� */
    NETWORK_UNREACHABLE_PERMANENTLY,        /* ���ò��ɴ� */
    NETWORK_REACHABLE_REVERSE,              /* ����ɴ� */
} network_reachable_status_t;

/* �˿���Ϣ */
typedef struct bacnet_port_s {
    uint32_t id;                            /* �˿�ID */
    bool valid;                             /* true��ʾ�ö˿���Ч��false��ʾ�˿���Ч */
    uint16_t net;                           /* ֱ������� */
    datalink_base_t *dl;                    /* ��·����� */
} bacnet_port_t;

/* information of routing entry */
typedef struct route_entry_s {
    uint16_t dnet;                          /* Ŀ������� */
    uint8_t mac_len;                        /* ��һ��·����MAC��ַ���� */
    uint8_t mac[MAX_MAC_LEN];               /* ��һ��MAC��ַ */
    bool direct_net;                        /* ֱ�������ʶ��true��ʶֱ�����磬false��ʶ��ֱ������ */
    bool busy;                              /* �����Ƿ�busy */
    bacnet_port_t *port;                    /* ���������Ķ˿�(������) */
} route_entry_t;

typedef struct route_entry_impl_s {
    route_entry_t info;
    uint32_t timestamp;                     /* ��¼��һ�η���Who_Is_Router���յ�busy��ʱ�� */
    struct hlist_node hash_node;            /* ��ϣ������ */
    struct list_head reachable_node;        /* �˿ڿɴ������ */
    struct list_head route_node;            /* ȫ��·�ɱ�������ʱ������ */
} route_entry_impl_t;

extern bacnet_port_t *route_port_get_list_head(void);

extern bacnet_port_t *route_port_find_by_id(uint32_t port_id);

extern void route_ports_show(void);

extern void route_port_show_reachable_list(uint32_t port_id);

extern int route_port_get_reachable_net_exclude_port(uint32_t port_id, uint16_t dnet_list[],
            int list_num);

extern int route_port_get_reachable_net_on_mac_set_busy(uint32_t port_id, bool busy,
            bacnet_addr_t *mac, uint16_t dnet_list[], int list_num);

extern int route_port_unicast_pdu(bacnet_port_t *port, bacnet_addr_t *dst_mac, bacnet_buf_t *npdu, 
            npci_info_t *npci_info);

extern int route_port_broadcast_pdu(bacnet_port_t *ex_port, bacnet_buf_t *npdu, 
            npci_info_t *npci_info);

extern bool route_find_entry(uint16_t dnet, route_entry_t *entry);

/**
 * find route and register whois try, caller have to do sending work
 * @need_whois: bool*, if null, force register whois, if not null, check by
 *              internal logic, return whether need to send whois
 * @return: true, if find entry, false not found
 */
extern bool route_try_find_entry(uint16_t dnet, route_entry_t *entry, bool *need_whois);

extern int route_update_dynamic_entry(uint16_t dnet, network_reachable_status_t state,
            bacnet_port_t *out_port, bacnet_addr_t *next_mac);

extern int route_startup(void);

extern int route_port_init(cJSON *cfg);

extern void route_port_destroy(void);

extern void route_table_show(void);

extern int route_table_init(void);

extern void route_table_destroy(void);

extern int route_table_config(cJSON *cfg);

extern cJSON *route_get_port_mib(cJSON *request);

#endif  /* _ROUTE_H_ */

