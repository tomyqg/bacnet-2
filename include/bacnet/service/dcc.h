/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * dcc.h
 * Original Author:  linzhixian, 2015-1-27
 *
 * Device Communication Control
 *
 * History
 */

#ifndef _DCC_H_
#define _DCC_H_

#include <stdint.h>
#include <stdbool.h>

#include "bacnet/apdu.h"
#include "bacnet/bacenum.h"
#include "bacnet/bacstr.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum {
    COMMUNICATION_ENABLE = 0,
    COMMUNICATION_DISABLE = 1,
    COMMUNICATION_DISABLE_INITIATION = 2,
    MAX_BACNET_COMMUNICATION_ENABLE_DISABLE = 3
} BACNET_COMMUNICATION_ENABLE_DISABLE;

typedef struct BACnet_DCC_Data {
    uint16_t timeDuration;
    BACNET_COMMUNICATION_ENABLE_DISABLE enable_disable;
    BACNET_CHARACTER_STRING password;
    union {
        struct {
            BACNET_ERROR_CLASS error_class;
            BACNET_ERROR_CODE error_code;
        };
        BACNET_REJECT_REASON reject_reason;
        BACNET_ABORT_REASON abort_reason;
    };
} BACNET_DCC_DATA;

extern bool dcc_communication_enabled(void);

extern bool dcc_communication_disabled(void);

extern bool dcc_communication_initiation_disabled(void);

extern bool dcc_set_status_duration(BACNET_COMMUNICATION_ENABLE_DISABLE status, uint16_t minutes);

extern void handler_device_communication_control(BACNET_CONFIRMED_SERVICE_DATA *service_data, 
                bacnet_buf_t *reply_apdu, bacnet_addr_t *src);

extern int Send_Device_Communication_Control_Request(uint8_t invoke_id, uint32_t device_id, 
                uint16_t timeDuration, BACNET_COMMUNICATION_ENABLE_DISABLE state, char *password);

#ifdef __cplusplus
}
#endif

#endif  /* _DCC_H_ */

