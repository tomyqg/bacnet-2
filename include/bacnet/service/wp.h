/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * wp.h
 * Original Author:  linzhixian, 2015-1-15
 *
 * Write Property
 *
 * History
 */

#ifndef _WP_H_
#define _WP_H_

#include <stdint.h>
#include <stdbool.h>

#include "bacnet/apdu.h"
#include "bacnet/bacapp.h"
#include "bacnet/bacenum.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct BACnet_Write_Property_Data {
    uint8_t priority;                       /* use BACNET_NO_PRIORITY if no priority */
    BACNET_OBJECT_TYPE object_type;
    uint32_t object_instance;
    BACNET_PROPERTY_ID property_id;
    uint32_t array_index;                   /* use BACNET_ARRAY_ALL when not setting */
    uint8_t *application_data;
    uint32_t application_data_len;
    union {
        struct {
            BACNET_ERROR_CLASS error_class;
            BACNET_ERROR_CODE error_code;
        };
        BACNET_ABORT_REASON abort_reason;
    };
} BACNET_WRITE_PROPERTY_DATA;

typedef bool (*write_property_handler)(BACNET_WRITE_PROPERTY_DATA *wp_data);

extern void handler_write_property(BACNET_CONFIRMED_SERVICE_DATA *service_data, 
                bacnet_buf_t *reply_apdu, bacnet_addr_t *src);

extern void wp_req_encode(bacnet_buf_t *apdu, BACNET_OBJECT_TYPE object_type,
            uint32_t object_instance, BACNET_PROPERTY_ID object_property,
            uint32_t array_index);

extern bool wp_req_encode_end(bacnet_buf_t *apdu, uint8_t invoke_id, uint8_t priority);

#ifdef __cplusplus
}
#endif

#endif  /* _WP_H_ */

