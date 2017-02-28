/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * whohas.c
 * Original Author:  linzhixian, 2015-2-10
 *
 * Who Has Request Service
 *
 * History
 */

#include <string.h>
#include "bacnet/service/whohas.h"
#include "bacnet/bacdcode.h"
#include "bacnet/service/dcc.h"
#include "bacnet/object/device.h"
#include "bacnet/service/ihave.h"
#include "bacnet/network.h"
#include "bacnet/bacdef.h"
#include "bacnet/app.h"

static int whohas_decode_service_request(uint8_t *pdu, uint16_t pdu_len, BACNET_WHO_HAS_DATA *data)
{
    uint32_t decoded_value;
    int len, dec_len;

    if ((pdu == NULL) || (pdu_len == 0) || (data == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return BACNET_STATUS_ERROR;
    }

    len = 0;
 
    data->low_limit = -1;
    data->high_limit = -1;
    /* optional limits - must be used as a pair */
    if (decode_has_context_tag(pdu, 0)) {
        dec_len = decode_context_unsigned(pdu, 0, &decoded_value);
        if (dec_len < 0) {
            APP_ERROR("%s: invalid low limit\r\n", __func__);
            return BACNET_STATUS_REJECT;
        }
        len += dec_len;
        if (decoded_value <= BACNET_MAX_INSTANCE) {
            data->low_limit = decoded_value;
        }

        dec_len = decode_context_unsigned(&pdu[len], 1, &decoded_value);
        if (dec_len < 0) {
            APP_ERROR("%s: invalid high limit\r\n", __func__);
            return BACNET_STATUS_REJECT;
        }
        len += dec_len;
        
        if (decoded_value <= BACNET_MAX_INSTANCE) {
            data->high_limit = decoded_value;
        }
    }
    
    /* object id */
    if (decode_has_context_tag(&pdu[len], 2)) {
        data->is_object_name = false;
        dec_len = decode_context_object_id(&pdu[len], 2, &(data->object.identifier.type),
            &(data->object.identifier.instance));
        if (dec_len < 0) {
            APP_ERROR("%s: invalid object id\r\n", __func__);
            return BACNET_STATUS_REJECT;
        }
        len += dec_len;
    } else if (decode_has_context_tag(&pdu[len], 3)) {
        data->is_object_name = true;
        dec_len = decode_context_character_string(&pdu[len], 3, &(data->object.name));
        if (dec_len < 0) {
            APP_ERROR("%s: invalid object name\r\n", __func__);
            return BACNET_STATUS_REJECT;
        }
        len += dec_len;
    } else {
        APP_ERROR("%s: missing required parameters\r\n", __func__);
        return BACNET_STATUS_ERROR;
    }
 
    if (len > pdu_len) {
        APP_ERROR("%s: missing required parameter\r\n", __func__);
        return BACNET_STATUS_REJECT;
    } else if (len < pdu_len) {
        APP_ERROR("%s: too many arguments\r\n", __func__);
        return BACNET_STATUS_REJECT;
    }

    return len;
}

/* encode who_has service: use -1 for limit for unlimited */
static int whohas_encode_apdu(bacnet_buf_t *apdu, BACNET_WHO_HAS_DATA *data)
{
    uint8_t *pdu;
    int len;

    pdu = apdu->data;
    pdu[0] = PDU_TYPE_UNCONFIRMED_SERVICE_REQUEST << 4;
    pdu[1] = SERVICE_UNCONFIRMED_WHO_HAS;
    len = 2;
    
    /* optional limits - must be used as a pair */
    if ((data->low_limit >= 0) && (data->low_limit <= BACNET_MAX_INSTANCE) 
            && (data->high_limit >= 0) && (data->high_limit <= BACNET_MAX_INSTANCE)) {
        len += encode_context_unsigned(&pdu[len], 0, data->low_limit);
        len += encode_context_unsigned(&pdu[len], 1, data->high_limit);
    }
    
    if (data->is_object_name) {
        len += encode_context_character_string(&pdu[len], 3, &data->object.name);
    } else {
        len += encode_context_object_id(&pdu[len], 2, (int)data->object.identifier.type,
            data->object.identifier.instance);
    }

    apdu->data_len = len;

    return len;
}

static void match_name_or_object(BACNET_WHO_HAS_DATA *data)
{
    BACNET_CHARACTER_STRING object_name;
    BACNET_OBJECT_TYPE object_type;
    uint32_t object_instance;
    bool status;

    if (data->is_object_name) {
        status = object_find_name(&data->object.name, &object_type, &object_instance);
        if (status) {
            Send_I_Have((BACNET_OBJECT_TYPE)object_type, object_instance, &data->object.name);
        }
    } else {
        status = object_get_name((BACNET_OBJECT_TYPE)data->object.identifier.type,
            data->object.identifier.instance, &object_name);
        if (status) {
            Send_I_Have((BACNET_OBJECT_TYPE)data->object.identifier.type,
                data->object.identifier.instance, &object_name);
        }
    }
}

void handler_who_has(uint8_t *request, uint16_t request_len, bacnet_addr_t *src)
{
    BACNET_WHO_HAS_DATA data;
    bool directed_to_me;
    int len;

    len = whohas_decode_service_request(request, request_len, &data);
    if (len < 0) {
        APP_ERROR("%s: decode service request failed(%d)\r\n", __func__, len);
        return;
    }

    directed_to_me = false;
    if ((data.low_limit == -1) || (data.high_limit == -1)) {
        directed_to_me = true;
    } else if ((device_object_instance_number() >= (uint32_t)data.low_limit)
            && (device_object_instance_number() <= (uint32_t)data.high_limit)) {
        directed_to_me = true;
    }
    
    if (directed_to_me) {
        match_name_or_object(&data);
    }
}

/** Send_WhoHas_Name - Send a Who-Has request for a device which has a named Object.
 *
 * @low_limit: [in] Device Instance Low Range, 0 - 4,194,303 or -1
 * @high_limit:  [in] Device Instance High Range, 0 - 4,194,303 or -1
 * @object_name: [in] The Name of the desired Object.
 *
 * If low_limit and high_limit both are -1, then the device ID range is unlimited.
 * If low_limit and high_limit have the same non-negative value, then only
 * that device will respond.
 * Otherwise, low_limit must be less than high_limit for a range.
 *
 */
void Send_WhoHas_Name(int32_t low_limit, int32_t high_limit, const char *object_name)
{
    BACNET_WHO_HAS_DATA data;
    bacnet_addr_t dst;
    DECLARE_BACNET_BUF(tx_apdu, MAX_APDU);
    int rv;

    if (object_name == NULL) {
        APP_ERROR("%s: invalid object_name\r\n", __func__);
        return;   
    }

    if ((low_limit > high_limit) || (low_limit < -1) || (high_limit > BACNET_MAX_INSTANCE)
            || ((low_limit < 0) && (high_limit >= 0))) {
        APP_ERROR("%s: invalid arguments low_limit(%d) high_limit(%d)\r\n", __func__,
            low_limit, high_limit);
        return;
    }
    
    if (!dcc_communication_enabled()) {
        APP_ERROR("%s: dcc communication disable\r\n", __func__);
        return;
    }
    
    /* Who-Has is a global broadcast */
    dst.net = BACNET_BROADCAST_NETWORK;
    dst.len = 0;

    data.low_limit = low_limit;
    data.high_limit = high_limit;
    data.is_object_name = true;
    if (!characterstring_init_ansi(&data.object.name, (char*)object_name, strlen(object_name))) {
        APP_ERROR("%s: characterstring init failed\r\n", __func__);
        return;
    }
    
    (void)bacnet_buf_init(&tx_apdu.buf, MAX_APDU);
    (void)whohas_encode_apdu(&tx_apdu.buf, &data);

    rv = network_send_pdu(&dst, &tx_apdu.buf, PRIORITY_NORMAL, false);
    if (rv < 0) {
        APP_ERROR("%s: network send failed(%d)\r\n", __func__, rv);
    }

    return;
}

/** Send_WhoHas_Object - Send a Who-Has request for a device which has a specific Object type and ID.
 *
 * @low_limit: [in] Device Instance Low Range, 0 - 4,194,303 or -1
 * @high_limit: [in] Device Instance High Range, 0 - 4,194,303 or -1
 * @object_type: [in] The BACNET_OBJECT_TYPE of the desired Object.
 * @object_instance: [in] The ID of the desired Object.
 *
 * If low_limit and high_limit both are -1, then the device ID range is unlimited.
 * If low_limit and high_limit have the same non-negative value, then only
 * that device will respond.
 * Otherwise, low_limit must be less than high_limit for a range.
 *
 */
void Send_WhoHas_Object(int32_t low_limit, int32_t high_limit, BACNET_OBJECT_TYPE object_type,
        uint32_t object_instance)
{
    BACNET_WHO_HAS_DATA data;
    bacnet_addr_t dst;
    DECLARE_BACNET_BUF(tx_apdu, MIN_APDU);
    int rv;

    if ((low_limit > high_limit) || (low_limit < -1) || (high_limit > BACNET_MAX_INSTANCE)
            || ((low_limit < 0) && (high_limit >= 0))) {
        APP_ERROR("%s: invalid arguments low_limit(%d) high_limit(%d)\r\n", __func__,
            low_limit, high_limit);
        return;
    }
    
    if (!dcc_communication_enabled()) {
        APP_ERROR("%s: dcc communication disabled\r\n", __func__);
        return;
    }
    
    /* Who-Has is a global broadcast */
    dst.net = BACNET_BROADCAST_NETWORK;
    dst.len = 0;

    data.low_limit = low_limit;
    data.high_limit = high_limit;
    data.is_object_name = false;
    data.object.identifier.type = object_type;
    data.object.identifier.instance = object_instance;

    (void)bacnet_buf_init(&tx_apdu.buf, MIN_APDU);
    (void)whohas_encode_apdu(&tx_apdu.buf, &data);

    rv = network_send_pdu(&dst, &tx_apdu.buf, PRIORITY_NORMAL, false);
    if (rv < 0) {
        APP_ERROR("%s: network send failed(%d)\r\n", __func__, rv);
    }

    return;
}

