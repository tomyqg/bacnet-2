/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * connect_mng_def.h
 * Original Author:  linzhixian, 2015-11-4
 *
 * Connect Manager
 *
 * History
 */

#ifndef _CONNECT_MNG_DEF_H_
#define _CONNECT_MNG_DEF_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "misc/eventloop.h"
#include "misc/list.h"
#include "connect_mng.h"

extern bool connet_mng_dbg_err;
extern bool connet_mng_dbg_warn;
extern bool connet_mng_dbg_verbos;

#define CONNECT_MNG_ERROR(fmt, args...)             \
do {                                                \
    if (connet_mng_dbg_err) {                       \
        printf(fmt, ##args);                        \
    }                                               \
} while (0)
 
#define CONNECT_MNG_WARN(fmt, args...)              \
do {                                                \
    if (connet_mng_dbg_warn) {                      \
        printf(fmt, ##args);                        \
    }                                               \
} while (0)
 
#define CONNECT_MNG_VERBOS(fmt, args...)            \
do {                                                \
    if (connet_mng_dbg_verbos) {                    \
        printf(fmt, ##args);                        \
    }                                               \
} while (0)

#ifndef OK
#define OK                                          (0)
#endif

#define MAX_SOCKET_LISTEN_BACKLOG                   (30)
#define MAX_READ_WRITE_TIMEOUT                      (30000)
#define MAX_CONNECT_MNG_DATA_LEN                    (64000)

typedef struct sockfd_list_s {
    int count;
    struct list_head head;
} sockfd_list_t;

typedef struct connect_mng_pkt_s {
    uint32_t type;
    uint32_t data_len;
} connect_mng_pkt_t;

typedef struct connect_info_impl_s {
    connect_info_t base;
    uint32_t bytes_done;
    uint32_t fd;
    el_watch_t *watcher;
    el_timer_t *timer;
    struct list_head node;
    bool in_handler;
} connect_info_impl_t;

typedef struct connect_client_async_impl_s {
    connect_client_async_t base;
    uint32_t sent_bytes;
    uint32_t recv_bytes;
    bool in_callback;       // 当前是否在调用callback中
    struct el_loop_s *el;
    el_watch_t *watch;
    el_timer_t *timer;
} connect_client_async_impl_t;

#endif /* _CONNECT_MNG_DEF_H_ */

