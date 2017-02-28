/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * rp.h
 * Original Author:  linzhixian, 2015-1-15
 *
 * Read Property
 *
 * History
 */

#ifndef _RP_H_
#define _RP_H_

#include <stdint.h>

#include "bacnet/apdu.h"
#include "bacnet/bacenum.h"
#include "bacnet/bacnet_buf.h"
#include "bacnet/tsm.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct BACnet_Read_Property_Data {
    BACNET_OBJECT_TYPE object_type;
    uint32_t object_instance;
    BACNET_PROPERTY_ID property_id;
    uint32_t array_index;
    uint8_t *application_data;
    uint16_t application_data_len;
    union {
        struct {
            BACNET_ERROR_CLASS error_class;
            BACNET_ERROR_CODE error_code;
        };
        BACNET_REJECT_REASON reject_reason;
        BACNET_ABORT_REASON abort_reason;
    };
} BACNET_READ_PROPERTY_DATA;

extern void handler_read_property(BACNET_CONFIRMED_SERVICE_DATA *service_data, 
                bacnet_buf_t *reply_apdu, bacnet_addr_t *src);

extern void rp_ack_print_data(BACNET_READ_PROPERTY_DATA *rp_data);

extern int rp_ack_decode(uint8_t *pdu, uint16_t pdu_len, BACNET_READ_PROPERTY_DATA *rp_data);

extern int rp_encode_apdu(bacnet_buf_t *apdu, uint8_t invoke_id, BACNET_OBJECT_TYPE object_type,
            uint32_t object_instance, BACNET_PROPERTY_ID object_property, uint32_t array_index);

extern int Send_Read_Property_Request(tsm_invoker_t *invoker, BACNET_OBJECT_TYPE object_type,
            uint32_t object_instance, BACNET_PROPERTY_ID object_property, uint32_t array_index);

#ifdef __cplusplus
}
#endif

#endif  /* _RP_H_ */

