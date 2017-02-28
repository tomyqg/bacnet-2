/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * addressbind_def.h
 * Original Author:  linzhixian, 2015-2-13
 *
 * BACnet Address Binding
 *
 * History
 */

#ifndef _ADDRESSBIND_DEF_H_
#define _ADDRESSBIND_DEF_H_

#include <stdint.h>
#include <pthread.h>

#include "bacnet/bacdef.h"
#include "misc/hashtable.h"
#include "bacnet/addressbind.h"

#define ADDRESS_CACHE_BITS                  (8)
#define WHOIS_CACHE_BITS                    (8)
#define WHOIS_MIN_INTERVAL                  (10)

#define MIN_CACHE_SIZE                      (64)

typedef struct Address_Manager_s {
    pthread_mutex_t lock;
    DECLARE_HASHTABLE(d2a_table, ADDRESS_CACHE_BITS);   /* device_id to address */
    DECLARE_HASHTABLE(a2d_table, ADDRESS_CACHE_BITS);   /* address to device_id */
    uint16_t active_count;
    uint16_t static_count;
    struct list_head static_list;
    struct list_head active_list;
} Address_Manager_t;

typedef struct Whois_Manager_s {
    pthread_mutex_t lock;
    DECLARE_HASHTABLE(table, WHOIS_CACHE_BITS);
    uint32_t count;
    struct list_head list;
} Whois_Manager_t;

typedef struct Address_Cache_Entry_s {
    bool is_static;
    uint32_t device_id : 22;
    uint16_t max_apdu;
    bacnet_addr_t address;
    unsigned update_time;                           /* 表项的更新时间点 */
    struct list_head l_node;                        /* active_list or static_list */
    struct hlist_node did_node;                     /* device_id to entry map */
    struct hlist_node adr_node;                     /* address to entry map */
} Address_Cache_Entry_t;

typedef struct Whois_Cache_Entry_s {
    uint32_t device_id;
    uint32_t timeout;
    unsigned last_time;
    struct list_head l_node;
    struct hlist_node h_node;
} Whois_Cache_Entry_t;

#endif /* _ADDRESSBIND_DEF_H_ */

