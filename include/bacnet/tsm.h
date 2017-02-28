/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * tsm.h
 * Original Author:  linzhixian, 2016-8-5
 *
 * BACnet Transaction State Machine
 *
 * History
 */

#ifndef _TSM_H_
#define _TSM_H_

#include <stdint.h>

#include "bacnet/bacdef.h"
#include "bacnet/bacenum.h"
#include "bacnet/bacnet_buf.h"
#include "misc/cJSON.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct tsm_invoker_s tsm_invoker_t;

typedef void (*invoker_handler)(tsm_invoker_t *invoker, bacnet_buf_t *apdu,
                BACNET_PDU_TYPE apdu_type);

struct tsm_invoker_s {
    bacnet_addr_t addr;
    uint8_t invokeID;
    uint8_t sent_count;
    BACNET_CONFIRMED_SERVICE choice;
    invoker_handler handler;
    void *data;
};

extern uint32_t tsm_get_apdu_timeout(void);
extern uint32_t tsm_set_apdu_timeout(uint32_t new_timeout);

extern uint32_t tsm_get_apdu_retries(void);
extern uint32_t tsm_set_apdu_retries(uint32_t new_retries);

extern void tsm_free_invokeID(tsm_invoker_t *invoker);

extern tsm_invoker_t *tsm_alloc_invokeID(bacnet_addr_t *addr, BACNET_CONFIRMED_SERVICE choice,
                        invoker_handler handler, void *data);

extern void tsm_invoker_callback(bacnet_addr_t *addr, bacnet_buf_t *apdu, BACNET_PDU_TYPE apdu_type);

extern int tsm_send_apdu(tsm_invoker_t *invoker, bacnet_buf_t *apdu, bacnet_prio_t prio,
            uint32_t timeout);

extern int tsm_init(cJSON *cfg);

extern void tsm_exit(void);

#ifdef __cplusplus
}
#endif

#endif  /* _TSM_H_ */

