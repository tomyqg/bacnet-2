/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * my_test.h
 * Original Author:  linzhixian, 2016-4-7
 *
 * 
 *
 * History
 */

#ifndef _MY_TEST_H_
#define _MY_TEST_H_

#include <stdint.h>

#include "bacnet/bacenum.h"
#include "bacnet/bacapp.h"
#include "bacnet/bacstr.h"
#include "misc/eventloop.h"
#include "misc/hashtable.h"
#include "bacnet/object/av.h"

#define MY_HASH_BITS                (8)
#define MY_MIN_REQUEST_INTERVAL     (60)
#define MY_MAX_ACK_INTERVAL         (120)

typedef struct watch_object_s {
    uint32_t device_id;
    BACNET_OBJECT_TYPE type;
    uint32_t instance;
    bool status_flag_fault;
    float present_value;
    uint32_t update_time;
    struct hlist_node hlist;
    struct list_head list;
} watch_object_t;

typedef struct my_object_s {
    struct list_head list;
    object_av_t *av;
    el_timer_t *timer;
    watch_object_t *last_tx_watch;
    pthread_rwlock_t rwlock;
    int watch_object_count;
    struct list_head watch_head;
    DECLARE_HASHTABLE(watch_object_table, MY_HASH_BITS);
} my_object_t;

typedef struct my_object_list_s {
    pthread_rwlock_t rwlock;
    int count;
    struct list_head head;
} my_object_list_t;

#endif /* _MY_TEST_H_ */

