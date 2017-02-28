/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * web_ack.h
 * Original Author:  linzhixian, 2015-9-14
 *
 * Web ACK
 *
 * History
 */

#ifndef _WEB_ACK_H_
#define _WEB_ACK_H_

#include <stdint.h>
#include <stdbool.h>

#include "connect_mng.h"
#include "misc/eventloop.h"
#include "bacnet/bacapp.h"
#include "bacnet/tsm.h"

#define WEB_TIMER_TIMEOUT                           (30)

#define MAX_WEB_CACHE_ENTRY_NUM                     (40)

typedef struct web_cache_entry_s {
    bool valid;
    connect_info_t *conn;
    const char *choice;
    el_timer_t *timer;
    BACNET_DEVICE_OBJECT_PROPERTY property;
} web_cache_entry_t;

extern web_cache_entry_t *web_cache_entry_add(connect_info_t *conn, const char *choice,
                            BACNET_DEVICE_OBJECT_PROPERTY *property);

extern void web_cache_entry_delete(web_cache_entry_t *entry);

extern void web_confirmed_ack_handler(tsm_invoker_t *invoker, bacnet_buf_t *apdu,
        BACNET_PDU_TYPE apdu_type);

extern int web_ack_init(void);

extern void web_ack_exit(void);

#endif /* _WEB_ACK_H_ */

