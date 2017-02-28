/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bip.h
 * Original Author:  linzhixian, 2014-10-10
 *
 * BACNET_IP对外头文件
 *
 * History
 */

#ifndef _BIP_H_
#define _BIP_H_

#include <stdbool.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "bacnet/bacdef.h"
#include "bacnet/datalink.h"
#include "misc/list.h"
#include "misc/cJSON.h"
#include "misc/eventloop.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum {
    BIP_GET_BDT = 0,
    BIP_GET_FDT = 1
} BIP_SERVICE_STATUS;

#define FD_MIN_TTL                                  (30)
#define FD_MAX_TTL                                  (0x10000)
#define FD_MIN_REGISTER_INTERVAL                    (15)
#define FD_DEFAULT_TTL                              (100)

#define BDT_PUSH_MIN_INTERVAL                       (30)

typedef struct bbmd_data_s bbmd_data_t;
typedef struct fd_client_s fd_client_t;

typedef struct datalink_bip_s {
    datalink_base_t dl;
    el_watch_t *watch_uip;
    el_watch_t *watch_bip;
    struct list_head bip_list;
    char ifname[IFNAMSIZ];
    int sock_uip;
    int sock_bip;
    struct sockaddr_in sin;                 /* in network format */
    struct sockaddr_in bcast_sin;           /* in network format */
    struct in_addr netmask;                 /* in network format */
    bbmd_data_t *bbmd;                      /* not null if bbmd enable */
    fd_client_t *fd_client;                 /* not null if fd client enable */
} datalink_bip_t;

extern int bip_init(void);

extern void bip_exit(void);

extern int bip_startup(void);

extern void bip_stop(void);

extern void bip_clean(void);

/**
 * bip_port_create - 创建bip口的链路层对象
 *
 * @cfg: 端口配置信息
 *
 * @return: 成功返回端口的链路层对象指针，失败返回NULL
 *
 */
extern datalink_bip_t *bip_port_create(cJSON *cfg, cJSON *res);

extern int bip_port_delete(datalink_bip_t *bip_port);

/**
 * 取出下一个bip端口对象
 *
 * @prev: 上一个bip端口对象，如为NULL，取出第一个对象
 *
 * @return: 成功返回对象指针，失败返回NULL
 */
extern datalink_bip_t *bip_next_port(datalink_bip_t *prev);

extern void bip_set_dbg_level(uint32_t level);

extern cJSON *bip_set_bdt(cJSON *request);

extern cJSON *bip_get_status(cJSON *request);

extern int bip_get_interface_info(const char *ifname, struct in_addr *ip, struct in_addr *mask);

static inline int check_broadcast_mask(struct in_addr mask)
{
    uint32_t tripped;
#ifdef __ORDER_LITTLE_ENDIAN__
    tripped = mask.s_addr | (mask.s_addr >> 1);
    tripped |= tripped >> 2;
    tripped |= tripped >> 4;
    tripped |= tripped >> 8;
    tripped |= tripped >> 16;
#elif defined(__ORDER_BIG_ENDIAN__)
    tripped = mask.s_addr | (mask.s_addr << 1);
    tripped |= tripped << 2;
    tripped |= tripped << 4;
    tripped |= tripped << 8;
    tripped |= tripped << 16;
#else
#error "unknown byte order"
#endif
    return mask.s_addr == tripped;
}

#ifdef __cplusplus
}
#endif

#endif  /* _BIP_H_ */

