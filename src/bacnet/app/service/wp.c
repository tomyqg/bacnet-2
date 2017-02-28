/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * wp.c
 * Original Author:  linzhixian, 2015-1-15
 *
 * Write Property
 *
 * History
 */

#include <errno.h>
#include <string.h>

#include "bacnet/service/wp.h"
#include "bacnet/bacapp.h"
#include "bacnet/bacdcode.h"
#include "bacnet/object/device.h"
#include "bacnet/service/error.h"
#include "bacnet/service/abort.h"
#include "bacnet/service/reject.h"
#include "bacnet/bacdef.h"
#include "bacnet/service/dcc.h"
#include "bacnet/addressbind.h"
#include "bacnet/tsm.h"
#include "bacnet/network.h"
#include "bacnet/app.h"

/* decode the service request only, return MAX_BACNET_REJECT_REASON if success */
static BACNET_REJECT_REASON wp_decode_service_request(uint8_t *request,
        uint16_t request_len, BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    uint32_t property;
    uint32_t unsigned_value;
    int tmp_len;
    int len;
    
    if ((request == NULL) || (wp_data == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return REJECT_REASON_OTHER;
    }

    if (request_len < 10) {
        APP_ERROR("%s: invalid request_len(%d)\r\n", __func__, request_len);
        return REJECT_REASON_MISSING_REQUIRED_PARAMETER;
    }

    /* Tag 0: Object ID */
    len = decode_context_object_id(request, 0, &(wp_data->object_type), &(wp_data->object_instance));
    if (len < 0) {
        APP_ERROR("%s: decode object_id fail\r\n", __func__);
        return REJECT_REASON_INVALID_TAG;
    }

    /* Tag 1: Property ID */
    tmp_len = decode_context_enumerated(&request[len], 1, &property);
    if (tmp_len < 0) {
        APP_ERROR("%s: invalid property\r\n", __func__);
        return REJECT_REASON_INVALID_TAG;
    }
    len += tmp_len;
    wp_data->property_id = (BACNET_PROPERTY_ID)property;

    /* Tag 2: Optional Array Index */
    if (decode_has_context_tag(&request[len], 2)) {
        tmp_len = decode_context_unsigned(&request[len], 2, &(wp_data->array_index));
        if (tmp_len < 0) {
            APP_ERROR("%s: invalid property\r\n", __func__);
            return REJECT_REASON_INVALID_TAG;
        }
        len += tmp_len;
    } else
        wp_data->array_index = BACNET_ARRAY_ALL;

    /* Tag 3: Property Value */
    if (len >= request_len) {
        return REJECT_REASON_MISSING_REQUIRED_PARAMETER;
    }

    tmp_len = decode_constructed_tag(&request[len], request_len - len, 3);
    if (tmp_len < 0) {
        return REJECT_REASON_INVALID_TAG;
    }
    wp_data->application_data = &request[len] + CONTEXT_TAG_LENGTH(3);
    wp_data->application_data_len = tmp_len - CONTEXT_TAG_LENGTH(3) * 2;
    len += tmp_len;
    
    /* Tag 4: optional Priority - assumed MAX if not explicitly set */
    if ((uint16_t)len < request_len && decode_has_context_tag(&request[len], 4)) {
        tmp_len = decode_context_unsigned(&request[len], 4, &unsigned_value);
        if (tmp_len < 0) {
            APP_ERROR("%s: invalid priority\r\n", __func__);
            return REJECT_REASON_INVALID_TAG;
        }
        len += tmp_len;

        if ((unsigned_value >= BACNET_MIN_PRIORITY) && (unsigned_value <= BACNET_MAX_PRIORITY)) {
            wp_data->priority = (uint8_t)unsigned_value;
        } else {
            APP_ERROR("%s: invalid priority(%d)\r\n", __func__, unsigned_value);
            return REJECT_REASON_INVALID_PARAMETER_DATA_TYPE;
        }
    } else
        wp_data->priority = BACNET_MAX_PRIORITY;

    if (len < request_len) {
        APP_ERROR("%s: too many arguments\r\n", __func__);
        return REJECT_REASON_TOO_MANY_ARGUMENTS;
    } else if (len > request_len) {
        APP_ERROR("%s: missing required parameter\r\n", __func__);
        return REJECT_REASON_MISSING_REQUIRED_PARAMETER;
    }

    return MAX_BACNET_REJECT_REASON;
}

void handler_write_property(BACNET_CONFIRMED_SERVICE_DATA *service_data,
        bacnet_buf_t *reply_apdu, bacnet_addr_t *src)
{
    BACNET_WRITE_PROPERTY_DATA wp_data;
    BACNET_REJECT_REASON reject_reason;
    int len;
    
    reject_reason = wp_decode_service_request(service_data->service_request,
            service_data->service_request_len, &wp_data);
    if (reject_reason != MAX_BACNET_REJECT_REASON) {
        APP_ERROR("%s: decode service request failed, reject reason: %d\r\n", __func__, reject_reason);
        goto rejected;
    }

    len = object_write_property(&wp_data);
    if (len >= 0) {
        len = encode_simple_ack(reply_apdu->data, service_data->invoke_id,
            SERVICE_CONFIRMED_WRITE_PROPERTY);
    } else {
        APP_ERROR("%s: write Object(%d) Instance(%d) Property(%d) failed\r\n", __func__, 
            wp_data.object_type, wp_data.object_instance, wp_data.property_id);
        goto failed;
    }

    reply_apdu->data_len = len;
    return;

rejected:
    len = reject_encode_apdu(reply_apdu, service_data->invoke_id, reject_reason);
    goto out;

failed:
    reply_apdu->data_len = 0;
    
    if (len == BACNET_STATUS_ERROR) {
        len = bacerror_encode_apdu(reply_apdu, service_data->invoke_id,
            SERVICE_CONFIRMED_WRITE_PROPERTY, wp_data.error_class, wp_data.error_code);
    } else if (len == BACNET_STATUS_ABORT) {
        len = abort_encode_apdu(reply_apdu, service_data->invoke_id,
            wp_data.abort_reason, true);
    } else {
        APP_ERROR("%s: unknown error(%d)\r\n", __func__, len);
    }

out:
    if (len < 0) {
        reply_apdu->data_len = 0;
    }
    
    return;
}

void wp_req_encode(bacnet_buf_t *apdu, BACNET_OBJECT_TYPE object_type,
        uint32_t object_instance, BACNET_PROPERTY_ID object_property, uint32_t array_index)
{
    uint8_t *pdu;
    int len;

    pdu = apdu->data;
    pdu[0] = PDU_TYPE_CONFIRMED_SERVICE_REQUEST;
    pdu[1] = encode_max_segs_max_apdu(0, MAX_APDU);
    pdu[3] = SERVICE_CONFIRMED_WRITE_PROPERTY;
    len = 4;

    /* Tag 0: object_id */
    len += encode_context_object_id(&pdu[len], 0, object_type, object_instance);

    /* Tag 1: property_id */
    len += encode_context_enumerated(&pdu[len], 1, object_property);

    /* Tag 2: optional array index; ALL is -1 which is assumed when missing */
    if (array_index != BACNET_ARRAY_ALL) {
        len += encode_context_unsigned(&pdu[len], 2, array_index);
    }

    /* Tag 3: propertyValue */
    len += encode_opening_tag(&pdu[len], 3);
    apdu->data_len = len;
}

bool wp_req_encode_end(bacnet_buf_t *apdu, uint8_t invoke_id,
        uint8_t priority)
{
    uint8_t *pdu;
    int len;

    pdu = apdu->data + apdu->data_len;
    len = encode_closing_tag(pdu, 3);

    if (priority)
        len += encode_context_unsigned(&pdu[len], 4, priority);

    apdu->data[2] = invoke_id;
    apdu->data_len += len;
    return pdu + len <= apdu->end;
}

