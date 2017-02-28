/*
 * threadpool_def.h
 *
 *  Created on: Apr 22, 2016
 *      Author: lin
 */

#ifndef SRC_MISC_THREADPOOL_DEF_H_
#define SRC_MISC_THREADPOOL_DEF_H_

#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>

#include <misc/list.h>
#include <misc/threadpool.h>

extern bool tp_dbg_verbos;
extern bool tp_dbg_warn;
extern bool tp_dbg_err;

#define TP_ERROR(fmt, args...)                      \
do {                                                \
    if (tp_dbg_err) {                               \
       printf(fmt, ##args);                         \
    }                                               \
} while (0)

#define TP_WARN(fmt, args...)                       \
do {                                                \
    if (tp_dbg_warn) {                              \
        printf(fmt, ##args);                        \
    }                                               \
} while (0)

#define TP_VERBOS(fmt, args...)                     \
do {                                                \
    if (tp_dbg_verbos) {                            \
       printf(fmt, ##args);                         \
    }                                               \
} while (0)

typedef struct tp_work_s {
    tp_work_func func;
    void *context;
    unsigned data;
} tp_work_t;

#define TP_WORKER_NUMBER                            (10)
#define TP_WORK_STORAGE_SIZE                        (29)

typedef struct tp_work_storage_s {
    tp_work_t   works[TP_WORK_STORAGE_SIZE];
    uint16_t next;
    uint16_t size;
    struct list_head node;
} tp_work_storage_t;

struct tp_pool_s {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    bool started;

    unsigned worker_count;
    struct list_head head;
};

#endif /* SRC_MISC_THREADPOOL_DEF_H_ */
