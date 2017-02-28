/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * rpm.h
 * Original Author:  linzhixian, 2015-1-15
 *
 * Read Property Multiple
 *
 * History
 */

#ifndef _RPM_H_
#define _RPM_H_

#include <stdint.h>
#include <stdbool.h>

#include "bacnet/apdu.h"
#include "bacnet/bacenum.h"
#include "bacnet/bacapp.h"
#include "bacnet/bacnet_buf.h"
#include "rp.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct property_list_s {
    const BACNET_PROPERTY_ID *pList;
    uint16_t count;
} property_list_t;

typedef struct special_property_list_s {
    property_list_t Required;
    property_list_t Optional;
    property_list_t Proprietary;
} special_property_list_t;

typedef struct bacnet_rpm_ack_decoder {
    uint32_t state;
    uint8_t *data;
    uint32_t data_len;
} BACNET_RPM_ACK_DECODER;

extern void rpm_ack_print_data(uint8_t *service_data, uint32_t service_data_len);

/**
 * init decoder
 * @return <0 if failed
 */
extern int rpm_ack_decode_init(BACNET_RPM_ACK_DECODER *decoder,
        uint8_t *service_data, uint32_t service_data_len);

/**
 * @return >0 has new objct, =0 no more object, <0 error
 */
extern int rpm_ack_decode_object(BACNET_RPM_ACK_DECODER *decoder,
        BACNET_READ_PROPERTY_DATA *rp_data);

/**
 * @return >0 has new property, =0 no more property, <0 error
 */
extern int rpm_ack_decode_property(BACNET_RPM_ACK_DECODER *decoder,
        BACNET_READ_PROPERTY_DATA *rp_data);

/*
 * @return: true if success, false = fail
 */
extern bool rpm_req_encode_object(bacnet_buf_t *pdu, BACNET_OBJECT_TYPE object_type,
                uint32_t instance);

/*
 * @return: true if success, false = fail
 */
extern bool rpm_req_encode_property(bacnet_buf_t *pdu, BACNET_PROPERTY_ID property_id,
                uint32_t array_index);

/*
 * @return: true if success, false = fail
 */
extern bool rpm_req_encode_end(bacnet_buf_t *pdu, uint8_t invoke_id);

extern void handler_read_property_multiple(BACNET_CONFIRMED_SERVICE_DATA *service_data, 
                bacnet_buf_t *reply_apdu, bacnet_addr_t *src);

#ifdef __cplusplus
}
#endif

#endif  /* _RPM_H_ */

