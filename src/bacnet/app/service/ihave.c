/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * ihave.c
 * Original Author:  linzhixian, 2015-2-11
 *
 * I Have Request Service
 *
 * History
 */

#include "bacnet/service/ihave.h"
#include "bacnet/bacdcode.h"
#include "bacnet/service/dcc.h"
#include "bacnet/network.h"
#include "bacnet/bacdef.h"
#include "bacnet/app.h"
#include "bacnet/object/device.h"

extern bool client_device;

static int ihave_decode_service_request(uint8_t *pdu, uint16_t pdu_len, BACNET_I_HAVE_DATA *data)
{
    BACNET_OBJECT_TYPE decoded_type;
    int len, dec_len;

    if ((pdu == NULL) || (pdu_len == 0) || (data == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return BACNET_STATUS_ERROR;
    }

    /* deviceIdentifier */
    len = decode_application_object_id(pdu, &decoded_type, &(data->device_id));
    if (len < 0) {
        APP_ERROR("%s: invalid deviceIdentifier\r\n", __func__);
        return BACNET_STATUS_ERROR;
    } else if (decoded_type != OBJECT_DEVICE) {
        APP_ERROR("%s: deviceIdentifier has a device type(%d)\r\n", __func__,
                decoded_type);
        return BACNET_STATUS_ERROR;
    }

    /* objectIdentifier */
    dec_len = decode_application_object_id(&pdu[len], &(data->object_id.type), &(data->object_id.instance));
    if (dec_len < 0) {
        APP_ERROR("%s: invalid objectIdentifier\r\n", __func__);
        return BACNET_STATUS_ERROR;
    }
    len += dec_len;
     
    /* objectName */
    dec_len = decode_application_character_string(&pdu[len], &(data->object_name));
    if (dec_len < 0) {
        APP_ERROR("%s: invalid objectName\r\n", __func__);
        return BACNET_STATUS_ERROR;
    }
    len += dec_len;

    if (len != pdu_len) {
        APP_ERROR("%s: invalid pdu_len(%d)\r\n", __func__, pdu_len);
        return BACNET_STATUS_ERROR;
    }

    return len;
}

static int ihave_encode_apdu(bacnet_buf_t *apdu, BACNET_I_HAVE_DATA *data)
{
    uint8_t *pdu;
    int len;

    pdu = apdu->data;
    pdu[0] = PDU_TYPE_UNCONFIRMED_SERVICE_REQUEST << 4;
    pdu[1] = SERVICE_UNCONFIRMED_I_HAVE;
    len = 2;
    
    /* deviceIdentifier */
    len += encode_application_object_id(&pdu[len], OBJECT_DEVICE, data->device_id);

    /* objectIdentifier */
    len += encode_application_object_id(&pdu[len], (int)data->object_id.type, 
        data->object_id.instance);

    /* objectName */
    len += encode_application_character_string(&pdu[len], &data->object_name);

    apdu->data_len = len;

    return len;
}

void handler_i_have(uint8_t *request, uint16_t request_len, bacnet_addr_t *src)
{
    BACNET_I_HAVE_DATA data;
    int len;

    len = ihave_decode_service_request(request, request_len, &data);
    if (len < 0) {
        APP_ERROR("%s: decode service request failed(%d)\r\n", __func__, len);
    }

    return;
}

void Send_I_Have(BACNET_OBJECT_TYPE object_type, uint32_t object_instance,
        BACNET_CHARACTER_STRING *object_name)
{
    BACNET_I_HAVE_DATA data;
    bacnet_addr_t dst;
    DECLARE_BACNET_BUF(tx_apdu, MIN_APDU);
    int rv;

    if (client_device) {
        APP_WARN("%s: failed cause device client enabled\r\n", __func__);
        return;
    }

    /* if we are forbidden to send, don't send! */
    if (!dcc_communication_enabled()) {
        APP_ERROR("%s: dcc communication disabled\r\n", __func__);
        return;
    }

    /* Who-Has is a global broadcast */
    dst.net = BACNET_BROADCAST_NETWORK;
    dst.len = 0;

    data.device_id = device_object_instance_number();
    data.object_id.type = (uint16_t)object_type;
    data.object_id.instance = object_instance;
    data.object_name = *object_name;

    (void)bacnet_buf_init(&tx_apdu.buf, MIN_APDU);
    (void)ihave_encode_apdu(&tx_apdu.buf, &data);

    rv = network_send_pdu(&dst, &tx_apdu.buf, PRIORITY_NORMAL, false);
    if (rv < 0) {
        APP_ERROR("%s: network send failed(%d)\r\n", __func__, rv);
    }

    return;
}

