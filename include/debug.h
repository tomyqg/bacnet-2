/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * debug.h
 * Original Author:  linzhixian, 2015-11-20
 *
 * Debug Service
 *
 * History
 */

#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define MIN_DBG_LEVEL                   (0)
#define MAX_DBG_LEVEL                   (7)

#define DEBUG_LEVEL_VERBOS              (0x01)
#define DEBUG_LEVEL_WARN                (0x02)
#define DEBUG_LEVEL_ERROR               (0x04)

typedef enum {
    DEBUG_SET_DEBUG_DBG_STATUS = 0,
    DEBUG_SET_EVENT_LOOP_STATUS = 1,
    DEBUG_SET_CONNECT_MNG_DBG_STATUS = 2,
    DEBUG_SET_WEB_DBG_STATUS = 3,
    DEBUG_SET_APP_DBG_STATUS = 4,
    DEBUG_SET_NETWORK_DBG_STATUS = 5,
    DEBUG_SET_DATALINK_DBG_STATUS = 6,
    DEBUG_SET_MSTP_DBG_STATUS = 7,
    DEBUG_SET_BIP_DBG_STATUS = 8,
    DEBUG_SET_ETHERNET_DBG_STATUS = 9,
    DEBUG_SHOW_NETWORK_ROUTE_TABLE = 10,
    MAX_DEBUG_SERVICE_CHOICE
} DEBUG_SERVICE_CHOICE;

extern void debug_print(void *data, uint32_t len);

extern int debug_service_init(void);

extern void debug_service_exit(void);

#ifdef __cplusplus
}
#endif

#endif /* _DEBUG_H_ */

