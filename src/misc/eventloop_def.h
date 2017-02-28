/*
 * Copyright(C) 2014 SWG. All rights reserved.
 *
 * eventloop_def.h
 * Original Author:  lincheng, 2015-5-20
 *
 * Event loop
 *
 * History
 */

#ifndef _EVENTLOOP_DEF_H_
#define _EVENTLOOP_DEF_H_

#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>

#include "misc/list.h"
#include "misc/eventloop.h"

extern bool el_dbg_verbos;
extern bool el_dbg_warn;
extern bool el_dbg_err;

#define EL_ERROR(fmt, args...)                      \
do {                                                \
    if (el_dbg_err) {                               \
       printf(fmt, ##args);                         \
    }                                               \
} while (0)

#define EL_WARN(fmt, args...)                       \
do {                                                \
    if (el_dbg_warn) {                              \
        printf(fmt, ##args);                        \
    }                                               \
} while (0)

#define EL_VERBOS(fmt, args...)                     \
do {                                                \
    if (el_dbg_verbos) {                            \
       printf(fmt, ##args);                         \
    }                                               \
} while (0)

#define TVN_NUMS                            (4)
#define TVN_BITS                            (6)
#define TVR_BITS                            (8)
#define TVN_SIZE                            (1 << TVN_BITS)
#define TVR_SIZE                            (1 << TVR_BITS)
#define TVN_MASK                            (TVN_SIZE - 1)
#define TVR_MASK                            (TVR_SIZE - 1)

#define MAX_EVENTS                          (8)
#define RESERVE_WATCH                       (16)
#define RESERVE_TIMER                       (16)
/* 需保证能被1000整除 */
#define TIMER_GRANULARITY                   (100)

typedef struct el_watch_impl_s {
    union {
        el_watch_t base;
        struct list_head list;
    };
    int fd;
} el_watch_impl_t;

typedef struct el_timer_impl_s {
    union {
        el_timer_t base;
        struct list_head free_list;
    };
    struct list_head list;
    unsigned expires;
} el_timer_impl_t;

struct el_loop_s {
    int epoll_fd;
    pthread_t epoll_thread;
    pthread_mutex_t sync_lock;
    pthread_cond_t sync_cond;
    int busy;                       /* set busy flag */
    int started;
    bool inited;

    struct list_head free_watch_head;
    struct list_head recy_watch_head;
    struct list_head free_timer_head;

    unsigned free_watch_count;
    unsigned recy_watch_count;
    unsigned free_timer_count;

    unsigned curr_tick;
    struct list_head wheel_list[TVR_SIZE + TVN_SIZE * TVN_NUMS];
    struct list_head to_timer;          /* timer already timeout */
};

#endif /* _EVENTLOOP_DEF_H_ */

