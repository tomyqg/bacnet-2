/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * web_service_def.h
 * Original Author:  linzhixian, 2015-9-14
 *
 * Web Service
 *
 * History
 */

#ifndef _WEB_DEF_H_
#define _WEB_DEF_H_

#include <stdio.h>
#include <stdbool.h>

#include "web_service.h"
#include "misc/hashtable.h"

extern bool web_dbg_err;
extern bool web_dbg_warn;
extern bool web_dbg_verbos;

#define WEB_ERROR(fmt, args...)                     \
do {                                                \
    if (web_dbg_err) {                              \
        printf(fmt, ##args);                        \
    }                                               \
} while (0)
 
#define WEB_WARN(fmt, args...)                      \
do {                                                \
    if (web_dbg_warn) {                             \
        printf(fmt, ##args);                        \
    }                                               \
} while (0)
 
#define WEB_VERBOS(fmt, args...)                    \
do {                                                \
    if (web_dbg_verbos) {                           \
        printf(fmt, ##args);                        \
    }                                               \
} while (0)

#ifndef OK
#define OK                                          (0)
#endif

#define WEB_SERVICE_HASHTABLE_BITS                  (8)

typedef struct web_service_node_s {
    struct hlist_node h_node;
    const char *choice;
    web_service_handler handler;
} web_service_node_t;

typedef struct web_service_table_s {
    pthread_rwlock_t rwlock;
    int count;
    DECLARE_HASHTABLE(name, WEB_SERVICE_HASHTABLE_BITS);
} web_service_table_t;

#endif /* _WEB_DEF_H_ */

