/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * rd.h
 * Original Author:  linzhixian, 2015-1-29
 *
 * Write Property
 *
 * History
 */

#ifndef _RD_H_
#define _RD_H_

#include "bacnet/apdu.h"
#include "bacnet/bacenum.h"
#include "bacnet/bacstr.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum {
    BACNET_REINIT_COLDSTART = 0,
    BACNET_REINIT_WARMSTART = 1,
    BACNET_REINIT_STARTBACKUP = 2,
    BACNET_REINIT_ENDBACKUP = 3,
    BACNET_REINIT_STARTRESTORE = 4,
    BACNET_REINIT_ENDRESTORE = 5,
    BACNET_REINIT_ABORTRESTORE = 6,
    MAX_BACNET_REINITIALIZED_STATE = 7,
    BACNET_REINIT_IDLE = 255
} BACNET_REINITIALIZED_STATE;

typedef struct BACnet_Reinitialize_Device_Data {
    BACNET_REINITIALIZED_STATE state;
    BACNET_CHARACTER_STRING password;
    union {
        struct {
            BACNET_ERROR_CLASS error_class;
            BACNET_ERROR_CODE error_code;
        };
        BACNET_REJECT_REASON reject_reason;
        BACNET_ABORT_REASON abort_reason;
    };
} BACNET_REINITIALIZE_DEVICE_DATA;

extern void handler_reinitialize_device(BACNET_CONFIRMED_SERVICE_DATA *service_data, 
                bacnet_buf_t *reply_apdu, bacnet_addr_t *src);

extern int Send_Reinitialize_Device_Request(uint8_t invoke_id, uint32_t device_id, 
                BACNET_REINITIALIZED_STATE state, char *password);

#ifdef __cplusplus
}
#endif

#endif  /* _RD_H_ */

