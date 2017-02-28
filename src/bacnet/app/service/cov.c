/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * cov.c
 * Original Author:  linzhixian, 2015-1-12
 *
 * COVNotification Service
 *
 * History
 */

#include <errno.h>

#include "bacnet/service/cov.h"

#include "bacnet/addressbind.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacdef.h"
#include "bacnet/service/dcc.h"
#include "bacnet/tsm.h"
#include "bacnet/network.h"
#include "bacnet/app.h"
#include "bacnet/object/device.h"

extern bool is_app_exist;

static int cov_subscribe_encode_apdu(bacnet_buf_t *apdu, uint8_t invoke_id,
            BACNET_SUBSCRIBE_COV_DATA *data)
{
    uint8_t *pdu;
    int len;

    pdu = apdu->data;
    pdu[0] = PDU_TYPE_CONFIRMED_SERVICE_REQUEST;
    pdu[1] = encode_max_segs_max_apdu(0, MAX_APDU);
    pdu[2] = invoke_id;
    pdu[3] = SERVICE_CONFIRMED_SUBSCRIBE_COV;
    len = 4;
    
    /* Tag 0: subscriberProcessIdentifier */
    len += encode_context_unsigned(&pdu[len], 0, data->subscriberProcessIdentifier);

    /* Tag 1: monitoredObjectIdentifier */
    len += encode_context_object_id(&pdu[len], 1, (int)data->monitoredObjectIdentifier.type,
        data->monitoredObjectIdentifier.instance);

    /* If both the 'Issue Confirmed Notifications' and 'Lifetime' parameters are absent, then this shall
        indicate a cancellation request.
    */
    if (!data->cancellationRequest) {
        /* Tag 2 - issueConfirmedNotifications */
        len += encode_context_boolean(&pdu[len], 2, data->issueConfirmedNotifications);

        /* Tag 3 - lifetime */
        len += encode_context_unsigned(&pdu[len], 3, data->lifetime);
    }

    apdu->data_len = len;

    return len;
}

bool ucov_notify_encode_init(bacnet_buf_t *pdu, uint32_t subscriber_id, uint32_t timeRemaining, 
        BACNET_OBJECT_TYPE object_type, uint32_t object_instance)
{
    int len;
    uint8_t *buf = pdu->data;

    buf[0] = false;
    buf[1] = SERVICE_UNCONFIRMED_COV_NOTIFICATION;
    len = 2;

    /* Tag 0: subscriberProcessIdentifier */
    len += encode_context_unsigned(&buf[len], 0, subscriber_id);

    /* Tag 1: initiatingDeviceIdentifier */
    len += encode_context_object_id(&buf[len], 1, OBJECT_DEVICE, device_object_instance_number());

    /* Tag 2: monitoredObjectIdentifier */
    len += encode_context_object_id(&buf[len], 2, object_type, object_instance);

    /* Tag 3: timeRemaining */
    len += encode_context_unsigned(&buf[len], 3, timeRemaining);

    /* Tag 4: listOfValues */
    len += encode_opening_tag(&buf[len], 4);

    pdu->data_len = len;
    
    return buf + len < pdu->end;
}

bool ucov_notify_encode_property(bacnet_buf_t *apdu, BACNET_PROPERTY_ID property_id,
        uint32_t array_index)
{
    int len;
    uint8_t *buf = apdu->data + apdu->data_len;

    if (apdu->data[0]) {
        len = encode_closing_tag(buf, 2);       /* end of property value */
    } else {
        len = 0;
    }
    
    len += encode_context_enumerated(&buf[len], 0, property_id);
    if (array_index != BACNET_ARRAY_ALL) {
        len += encode_context_unsigned(&buf[len], 1, array_index);
    }
    
    len += encode_opening_tag(&buf[len], 2);
    apdu->data_len += len;
    apdu->data[0] = true;
    
    return buf + len < apdu->end;
}

bool ucov_notify_encode_end(bacnet_buf_t *apdu)
{
    int len;
    uint8_t *buf = apdu->data + apdu->data_len;

    if (!apdu->data_len || !apdu->data[0]) {
        APP_ERROR("%s: no object or no property in object\r\n", __func__);
        return false;
    }

    len = encode_closing_tag(buf, 2);           /* end of property value */
    len += encode_closing_tag(&buf[len], 4);    /* end of property list */

    apdu->data[0] = PDU_TYPE_UNCONFIRMED_SERVICE_REQUEST << 4;
    apdu->data_len += len;
    
    return buf + len <= apdu->end;
}

static int ucov_notify_decode_init(uint8_t *request, uint16_t request_len, uint32_t *subscriber_id, 
            uint32_t *device_id, uint32_t *timeRemaining, BACNET_OBJECT_TYPE *object_type,
            uint32_t *object_instance)
{
    int dec_len;
    int len;

    len = decode_context_unsigned(request, 0, subscriber_id);
    if (len < 0) {
        APP_ERROR("%s: decode subscriber_id failed(%d)\r\n", __func__, len);
        return len;
    }
    
    dec_len = decode_context_object_id(&request[len], 1, object_type, device_id);
    if (dec_len < 0) {
        APP_ERROR("%s: decode device_id failed(%d)\r\n", __func__, dec_len);
        return dec_len;
    }
    
    if (*object_type != OBJECT_DEVICE) {
        APP_ERROR("%s: invalid object_type(%d)\r\n", __func__, *object_type);
        return BACNET_STATUS_ERROR;
    }
    len += dec_len;
    
    dec_len = decode_context_object_id(&request[len], 2, object_type, object_instance);
    if (dec_len < 0) {
        APP_ERROR("%s: decode object_id failed(%d)\r\n", __func__, dec_len);
        return dec_len;
    }
    len += dec_len;

    dec_len = decode_context_unsigned(&request[len], 3, timeRemaining);
    if (dec_len < 0) {
        APP_ERROR("%s: decode timeRemaining failed(%d)\r\n", __func__, dec_len);
        return dec_len;
    }
    len += dec_len;

    dec_len = decode_opening_tag(&request[len], 4);
    if (dec_len < 0) {
        APP_ERROR("%s: decode opening tag failed(%d)\r\n", __func__, dec_len);
        return dec_len;
    }
    len += dec_len;

    if (len >= request_len) {
        APP_ERROR("%s: invalid request_len(%d)\r\n", __func__, request_len);
        return BACNET_STATUS_REJECT;
    }
    
    return len;
}

static int ucov_notify_decode_property(uint8_t *request, uint16_t request_len,
            BACNET_PROPERTY_VALUE *property_value)
{
    int dec_len;
    int len;

    len = decode_closing_tag(request, 4);
    if (len > 0) {
        if (len != 1) {
            APP_ERROR("%s: invalid closing tag len(%d)\r\n", __func__, len);
            return BACNET_STATUS_REJECT;
        }

        return len;
    }

    len = decode_context_enumerated(request, 0, &property_value->propertyIdentifier);
    if (len < 0) {
        APP_ERROR("%s: decode PropertyID failed(%d)\r\n", __func__, len);
        return len;
    }
    
    if (decode_has_context_tag(&request[len], 1)) {
        dec_len = decode_context_unsigned(&request[len], 1, &property_value->propertyArrayIndex);
        if (dec_len < 0) {
            APP_ERROR("%s: decode property array index failed(%d)\r\n", __func__,
                dec_len);
            return dec_len;
        }
        len += dec_len;
    } else {
        property_value->propertyArrayIndex = BACNET_ARRAY_ALL;
    }
    
    if (len >= request_len) {
        APP_ERROR("%s: invalid request_len(%d)\r\n", __func__, request_len);
        return BACNET_STATUS_REJECT;
    }
    
    dec_len = decode_constructed_tag(&request[len], request_len - len, 2);
    if (dec_len < 0) {
        APP_ERROR("%s: decode property value failed(%d)\r\n", __func__, dec_len);
        return dec_len;
    }

    property_value->value_data = &request[len] + CONTEXT_TAG_LENGTH(2);
    property_value->value_data_len = dec_len - CONTEXT_TAG_LENGTH(2) * 2;
    len += dec_len;
    
    return len;
}

void handler_ucov_notification(uint8_t *request, uint16_t request_len, bacnet_addr_t *src)
{
    uint32_t subscriber_id, device_id, timeRemaining, object_instance;
    BACNET_OBJECT_TYPE object_type;
    BACNET_PROPERTY_VALUE property_value;
    int dec_len;
    int len;
    
    if ((request == NULL) || (src == NULL) || (request_len == 0)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return;
    }

    len = ucov_notify_decode_init(request, request_len, &subscriber_id, &device_id, &timeRemaining,
            &object_type, &object_instance);
    if (len < 0) {
        APP_ERROR("%s: decode apdu header failed(%d)\r\n", __func__, len);
        return;
    }

    if (len >= request_len) {
        APP_ERROR("%s: invalid request_len(%d)\r\n", __func__, request_len);
        return;
    }

    while(1) {
        dec_len = ucov_notify_decode_property(&request[len], request_len - len, &property_value);
        if (dec_len < 0) {
            APP_ERROR("%s: decode property failed(%d)\r\n", __func__, dec_len);
            return;
        }
        len += dec_len;

        if (len > request_len) {
            APP_ERROR("%s: length(%d) out of range\r\n", __func__, len);
            return;
        }

        if (len == request_len) {
            break;
        }
        
        /* TODO: print property value */
    }
}

/** Send_COV_Subscribe - Sends a COV Subscription request.
 *
 * @device_id: [in] ID of the destination device
 * @cov_data: [in]  The COV subscription information to be encoded.
 *
 * @return 0 if successful, or negative if device is not bound or no tsm available
 *
 */
int Send_COV_Subscribe(uint8_t invoke_id, uint32_t device_id, BACNET_SUBSCRIBE_COV_DATA *cov_data)
{
    bacnet_addr_t dst;
    DECLARE_BACNET_BUF(tx_apdu, MAX_APDU);
    uint32_t max_apdu;
    int len;
    int rv;

    if (!is_app_exist) {
        APP_ERROR("%s: application layer do not exist\r\n", __func__);
        return -EPERM;
    }

    if (cov_data == NULL) {
        APP_ERROR("%s: null cov_data\r\n", __func__);
        return -EINVAL;
    }
    
    if (!dcc_communication_enabled()) {
        APP_ERROR("%s: dcc communication disabled\r\n", __func__);
        return -EPERM;
    }

    if (!query_address_from_device(device_id, &max_apdu, &dst)) {
        APP_ERROR("%s: get address from device_id(%d) failed\r\n", __func__, device_id);
        return -EPERM;
    }

    if ((dst.net == BACNET_BROADCAST_NETWORK) || (dst.len == 0)) {
        APP_ERROR("%s: invalid dst addr\r\n", __func__);
        return -EINVAL;
    }

    (void)bacnet_buf_init(&tx_apdu.buf, MAX_APDU);
    len = cov_subscribe_encode_apdu(&tx_apdu.buf, invoke_id, cov_data);
    if ((len < 0) || (len > max_apdu)) {
        APP_ERROR("%s: encode apdu failed(%d)\r\n", __func__, len);
        return -EPERM;
    }

    rv = network_send_pdu(&dst, &tx_apdu.buf, PRIORITY_NORMAL, true);
    if (rv < 0) {
        APP_ERROR("%s: network send failed(%d)\r\n", __func__, rv);
    } 

    return rv;
}

