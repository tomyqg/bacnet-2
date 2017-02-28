/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * rp.c
 * Original Author:  linzhixian, 2015-1-15
 *
 * Read Property
 *
 * History
 */

#include <string.h>
#include <errno.h>

#include "bacnet/service/rp.h"
#include "bacnet/bacdcode.h"
#include "bacnet/object/device.h"
#include "bacnet/service/error.h"
#include "bacnet/service/abort.h"
#include "bacnet/service/reject.h"
#include "bacnet/network.h"
#include "bacnet/bacdef.h"
#include "bacnet/service/dcc.h"
#include "bacnet/addressbind.h"
#include "bacnet/tsm.h"
#include "bacnet/network.h"
#include "bacnet/app.h"
#include "bacnet/bactext.h"

extern bool is_app_exist;

/**
 * rp_decode_service_request - 读属性服务请求的解码
 *
 * @request: 读属性服务请求数据流
 * @request_len: 读属性服务请求数据流的长度
 * @rp_data: 返回读属性请求数据信息
 *
 * @return: 成功返回读属性服务请求数据流的长度，失败返回负数
 *
 */
static int rp_decode_service_request(uint8_t *request, uint16_t request_len, 
            BACNET_READ_PROPERTY_DATA *rp_data)
{
    uint32_t property;
    int len, dec_len;

    if (rp_data == NULL) {
        APP_ERROR("%s: null rp_data\r\n", __func__);
        return BACNET_STATUS_REJECT;
    }
    
    if (request == NULL) {
        APP_ERROR("%s: null request\r\n", __func__);
        rp_data->reject_reason = REJECT_REASON_MISSING_REQUIRED_PARAMETER;
        return BACNET_STATUS_REJECT;
    }
    
    if (request_len < 7) {
        APP_ERROR("%s: missing required parameter\r\n", __func__);
        rp_data->reject_reason = REJECT_REASON_MISSING_REQUIRED_PARAMETER;
        return BACNET_STATUS_REJECT;
    }
    
    /* Tag 0: Object ID */
    len = decode_context_object_id(request, 0,
            &(rp_data->object_type), &(rp_data->object_instance));
    if (len < 0) {
        APP_ERROR("%s: invalid object_id\r\n", __func__);
        rp_data->reject_reason = REJECT_REASON_INVALID_TAG;
        return BACNET_STATUS_REJECT;
    }

    /* Tag 1: Property ID */
    dec_len = decode_context_enumerated(&request[len], 1, &property);
    if (dec_len < 0) {
        APP_ERROR("%s: invalid property\r\n", __func__);
        rp_data->reject_reason = REJECT_REASON_INVALID_TAG;
        return BACNET_STATUS_REJECT;
    }
    len += dec_len;
    rp_data->property_id = (BACNET_PROPERTY_ID)property;

    /* Tag 2: Optional Array Index */
    if (len < request_len && decode_has_context_tag(&request[len], 2)) {
        dec_len = decode_context_unsigned(&request[len], 2, &(rp_data->array_index));
        if (dec_len < 0 || len + dec_len > request_len) {
            APP_ERROR("%s: invalid ArrayIndex\r\n", __func__);
            rp_data->reject_reason = REJECT_REASON_INVALID_TAG;
            return BACNET_STATUS_REJECT;
        }
        len += dec_len;
        if (rp_data->array_index == BACNET_ARRAY_ALL) {
            APP_ERROR("%s: too large ArrayIndex\r\n", __func__);
            rp_data->reject_reason = REJECT_REASON_PARAMETER_OUT_OF_RANGE;
            return BACNET_STATUS_REJECT;
        }
    } else
        rp_data->array_index = BACNET_ARRAY_ALL;

    if (len != request_len) {
        if (len < request_len) {
            /* If something left over now, we have an invalid request */
            APP_ERROR("%s: too many arguments\r\n", __func__);
            rp_data->reject_reason = REJECT_REASON_TOO_MANY_ARGUMENTS;
            return BACNET_STATUS_REJECT;
        } else {
            APP_ERROR("%s: missing argument\r\n", __func__);
            rp_data->reject_reason = REJECT_REASON_MISSING_REQUIRED_PARAMETER;
            return BACNET_STATUS_REJECT;
        }
    }

    return len;
}

static int rp_encode_service(bacnet_buf_t *apdu, BACNET_OBJECT_TYPE object_type, unsigned instance,
            BACNET_PROPERTY_ID property_id, uint32_t array_index)
{
    uint8_t *pdu;
    int len;

    if ((!apdu) || (object_type >= BACNET_MAX_OBJECT) || (property_id >= MAX_BACNET_PROPERTY_ID)) {
        APP_ERROR("%s: invalid argument", __func__);
        return -EPERM;
    }

    pdu = &apdu->data[apdu->data_len];

    /* Tag 0: object_id */
    len = encode_context_object_id(pdu, 0, object_type, instance);

    /* Tag 1: property_id */
    len += encode_context_enumerated(&pdu[len], 1, property_id);

    /* Tag 2: optional property array index */
    if (array_index != BACNET_ARRAY_ALL) {
        len += encode_context_unsigned(&pdu[len], 2, array_index);
    }

    apdu->data_len += len;

    return len;
}

/* Encode ReadProperty-ACK PDU */
static int rp_ack_encode_apdu_init(bacnet_buf_t *apdu, uint8_t invoke_id, 
            BACNET_READ_PROPERTY_DATA *rp_data)
{
    uint8_t *pdu;
    int len;

    if (rp_data == NULL) {
        APP_ERROR("%s: invalid rp_data\r\n", __func__);
        return BACNET_STATUS_REJECT;
    }

    if ((apdu == NULL) || (apdu->data == NULL)) {
        APP_ERROR("%s: invalid apdu\r\n", __func__);
        rp_data->reject_reason = REJECT_REASON_MISSING_REQUIRED_PARAMETER;
        return BACNET_STATUS_REJECT;
    }

    pdu = apdu->data;
    pdu[0] = PDU_TYPE_COMPLEX_ACK << 4;                 /* complex ACK service */
    pdu[1] = invoke_id;                                 /* original invoke id from request */
    pdu[2] = SERVICE_CONFIRMED_READ_PROPERTY;           /* service choice */
    
    /* encode object_id */
    len = 3;
    len += encode_context_object_id(&pdu[len], 0, rp_data->object_type, rp_data->object_instance);

    /* encode property_id */
    len += encode_context_enumerated(&pdu[len], 1, rp_data->property_id);

    /* context 2 array index is optional */
    if (rp_data->array_index != BACNET_ARRAY_ALL) {
        len += encode_context_unsigned(&pdu[len], 2, rp_data->array_index);
    }
    
    apdu->data_len = len;

    return len;
}

void rp_ack_print_data(BACNET_READ_PROPERTY_DATA *rp_data)
{
    if (rp_data == NULL) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return;
    }

    fprintf(stdout, "%s #%lu ", bactext_object_type_name(rp_data->object_type),
        (unsigned long)rp_data->object_instance);

    if (rp_data->property_id < 512) {
        fprintf(stdout, "%s", bactext_property_name(rp_data->property_id));
    } else {
        fprintf(stdout, "proprietary %u", (unsigned)rp_data->property_id);
    }

    if (rp_data->array_index != BACNET_ARRAY_ALL) {
        fprintf(stdout, "[%d]", rp_data->array_index);
    }
    fprintf(stdout, ": ");

    bacapp_fprint_value(stdout, rp_data->application_data, rp_data->application_data_len);

    fprintf(stdout, "\r\n");
}

int rp_ack_decode(uint8_t *pdu, uint16_t pdu_len, BACNET_READ_PROPERTY_DATA *rp_data)
{
    uint32_t property;
    int len, dec_len;

    if ((pdu == NULL) || (rp_data == NULL) || (pdu_len == 0)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return BACNET_STATUS_ERROR;
    }
    
    /* Tag 0: Object ID */
    len = decode_context_object_id(pdu, 0, &(rp_data->object_type), &(rp_data->object_instance));
    if (len < 0) {
        APP_ERROR("%s: invalid objectID\r\n", __func__);
        return BACNET_STATUS_ERROR;
    }
    
    /* Tag 1: Property ID */
    dec_len = decode_context_enumerated(&pdu[len], 1, &property);
    if (dec_len < 0) {
        APP_ERROR("%s: invalid PropertyID\r\n", __func__);
        return BACNET_STATUS_ERROR;
    }
    len += dec_len;
    rp_data->property_id = (BACNET_PROPERTY_ID)property;
    
    /* Tag 2: Optional Array Index */
    if (decode_has_context_tag(&pdu[len], 2)) {
        dec_len = decode_context_unsigned(&pdu[len], 2, &(rp_data->array_index));
        if (dec_len < 0) {
            APP_ERROR("%s: invalid array index\r\n", __func__);
            return BACNET_STATUS_ERROR;
        }
        len += dec_len;
    } else {
        rp_data->array_index = BACNET_ARRAY_ALL;
    }
    
    /* Tag 3: PropertyValue */
    if ((decode_opening_tag(&pdu[len], 3) < 0) || (decode_closing_tag(&pdu[pdu_len - 1], 3) < 0)) {
        APP_ERROR("%s: invalid PropertyValue tag\r\n", __func__);
        return BACNET_STATUS_ERROR;
    }

    if (len + 2 > pdu_len) {
        APP_ERROR("%s: invalid pdu_len(%d)\r\n", __func__, pdu_len);
        return BACNET_STATUS_ERROR;
    }

    /* don't decode the application tag number or its data here */
    rp_data->application_data = &pdu[len + 1];
    rp_data->application_data_len = pdu_len - len - 2;  /* each one for opening/closing tag */

    len = pdu_len;

    return len;
}

void handler_read_property(BACNET_CONFIRMED_SERVICE_DATA *service_data, bacnet_buf_t *reply_apdu, 
        bacnet_addr_t *src)
{
    BACNET_READ_PROPERTY_DATA rp_data;
    int len;

    len = rp_decode_service_request(service_data->service_request, service_data->service_request_len, 
        &rp_data);
    if (len < 0) {
        APP_ERROR("%s: decode service request failed(%d)\r\n", __func__, len);
        goto failed;
    }

    /* Test for case of indefinite Device object instance */
    if ((rp_data.object_type == OBJECT_DEVICE) && (rp_data.object_instance == BACNET_MAX_INSTANCE)) {
        rp_data.object_instance = device_object_instance_number();
    }

    len = rp_ack_encode_apdu_init(reply_apdu, service_data->invoke_id, &rp_data);
    if (len < 0) {
        APP_ERROR("%s: rp ack encode apdu init failed\r\n", __func__);
        goto failed;
    }

    reply_apdu->data_len += encode_opening_tag(reply_apdu->data + reply_apdu->data_len, 3);
    rp_data.application_data = reply_apdu->data + reply_apdu->data_len;
    rp_data.application_data_len = reply_apdu->end - reply_apdu->data - reply_apdu->data_len;

    len = object_read_property(&rp_data, NULL);
    if (len < 0) {
        APP_ERROR("%s: read Object(%d) Instance(%d) Property(%d) failed(%d)\r\n", __func__, 
            rp_data.object_type, rp_data.object_instance, rp_data.property_id, len);
        goto failed;
    }
    reply_apdu->data_len += len;
    reply_apdu->data_len += encode_closing_tag(reply_apdu->data + reply_apdu->data_len, 3);

    if (reply_apdu->data + reply_apdu->data_len > reply_apdu->end) {
        APP_ERROR("%s: Message is too large\r\n", __func__);
        rp_data.abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
        len = BACNET_STATUS_ABORT;
        goto failed;
    }

    return;

failed:
    reply_apdu->data_len = 0;
    
    if (len == BACNET_STATUS_ERROR) {
        len = bacerror_encode_apdu(reply_apdu, service_data->invoke_id,
            SERVICE_CONFIRMED_READ_PROPERTY, rp_data.error_class, rp_data.error_code);
    } else if (len == BACNET_STATUS_ABORT) {
        len = abort_encode_apdu(reply_apdu, service_data->invoke_id,
            rp_data.abort_reason, true);
    } else if (len == BACNET_STATUS_REJECT) {
        len = reject_encode_apdu(reply_apdu, service_data->invoke_id,
            rp_data.reject_reason);
    } else {
        APP_ERROR("%s: unknown error(%d)\r\n", __func__, len);
        len = BACNET_STATUS_ERROR;
    }

    if (len < 0) {
        reply_apdu->data_len = 0;
    }
    
    return;
}

int rp_encode_apdu(bacnet_buf_t *apdu, uint8_t invoke_id, BACNET_OBJECT_TYPE object_type, 
        uint32_t object_instance, BACNET_PROPERTY_ID object_property, uint32_t array_index)
{
    int head_len, service_len;

    head_len = apdu_encode_confirmed_service_request(apdu, invoke_id, SERVICE_CONFIRMED_READ_PROPERTY);
    if (head_len < 0) {
        APP_ERROR("%s: encode req header failed\r\n", __func__);
        return head_len;
    }

    service_len = rp_encode_service(apdu, object_type, object_instance, object_property, array_index);
    if (service_len < 0) {
        APP_ERROR("%s: encode req service failed\r\n", __func__);
        return service_len;
    }

    return head_len + service_len;
}

int Send_Read_Property_Request(tsm_invoker_t *invoker, BACNET_OBJECT_TYPE object_type,
        uint32_t object_instance, BACNET_PROPERTY_ID object_property, uint32_t array_index)
{
    DECLARE_BACNET_BUF(tx_apdu, MIN_APDU);
    int rv;

    if (invoker == NULL) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    (void)bacnet_buf_init(&tx_apdu.buf, MIN_APDU);
    rv = rp_encode_apdu(&tx_apdu.buf, invoker->invokeID, object_type, object_instance,
        object_property, array_index);
    if ((rv < 0) || (rv > MIN_APDU)) {
        APP_ERROR("%s: encode apdu failed(%d)\r\n", __func__, rv);
        return -EPERM;
    }

    rv = tsm_send_apdu(invoker, &tx_apdu.buf, PRIORITY_NORMAL, 0);
    if (rv < 0) {
        APP_ERROR("%s: tsm send failed(%d)\r\n", __func__, rv);
    }

    return rv;
}

