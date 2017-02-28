/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * tsm_def.h
 * Original Author:  linzhixian, 2016-7-29
 *
 * BACnet Transaction State Machine
 *
 * History
 */

#ifndef _TSM_DEF_H_
#define _TSM_DEF_H_

#include <stdint.h>

#include "bacnet/tsm.h"
#include "misc/hashtable.h"
#include "misc/eventloop.h"

#define MIN_MAX_PEER                        (100)
#define MIN_MAX_INVOKER                     (100)

#define MIN_APDU_TIMEOUT                    (5000)
#define MAX_APDU_RETRIES                    (5)

#define PEER_TSM_TABLE_HASH_BITS            (8)
#define INVOKER_TABLE_HASH_BITS             (9)

typedef struct tsm_table_s {
    pthread_rwlock_t rwlock;
    int peer_count;
    int invoker_count;
    DECLARE_HASHTABLE(peer_table, PEER_TSM_TABLE_HASH_BITS);
    DECLARE_HASHTABLE(invoker_table, INVOKER_TABLE_HASH_BITS);
} tsm_table_t;

typedef struct tsm_peer_s {
    bacnet_addr_t addr;
    uint8_t invokeID_bitmap[32];
    uint32_t free_bits;
    uint8_t next_bit;
    struct hlist_node node;
} tsm_peer_t;

typedef struct tsm_invoker_impl_s {
    tsm_invoker_t base;
    uint8_t not_acked_count;
    bool canceled;
    tsm_peer_t *peer_tsm;
    uint32_t last_tx_timestamp;
    struct hlist_node node;
    el_timer_t *timer;
} tsm_invoker_impl_t;

#endif  /* _TSM_DEF_H_ */

