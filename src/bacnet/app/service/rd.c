/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * rd.c
 * Original Author:  linzhixian, 2015-1-29
 *
 * Reinitialize Device
 *
 * History
 */

#include <errno.h>
#include <string.h>
#include "bacnet/service/rd.h"
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

extern bool is_app_exist;

static int rd_decode_service_request(uint8_t *request, uint16_t request_len, 
            BACNET_REINITIALIZE_DEVICE_DATA *rd_data)
{
    uint32_t unsigned_value;
    int len;

    if ((request == NULL) || (rd_data == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return BACNET_STATUS_REJECT;
    }

    /* Tag 0: reinitializedStateOfDevice */
    len = decode_context_enumerated(request, 0, &unsigned_value);
    if (len < 0) {
        APP_ERROR("%s: invalid state tag_number(%d)\r\n", __func__, request[0]);
        rd_data->reject_reason = REJECT_REASON_INVALID_TAG;
        return BACNET_STATUS_REJECT;
    }
    if (unsigned_value >= MAX_BACNET_REINITIALIZED_STATE) {
        APP_ERROR("%s: undefined state(%d)\r\n", __func__, unsigned_value);
        rd_data->reject_reason = REJECT_REASON_UNDEFINED_ENUMERATION;
        return BACNET_STATUS_REJECT;
    }
    rd_data->state = (BACNET_REINITIALIZED_STATE)unsigned_value;

    /* Tag 1: optional password */
    if (len < request_len && decode_has_context_tag(&request[len], 1)) {
        int dec_len = decode_context_character_string(&request[len], 1, &(rd_data->password));
        if (dec_len < 0) {
            APP_ERROR("%s: invalid password\r\n", __func__);
            rd_data->reject_reason = REJECT_REASON_INVALID_TAG;
            return BACNET_STATUS_REJECT;
        }
        len += dec_len;
    }

    if (len < request_len) {
        APP_ERROR("%s: too many arguments\r\n", __func__);
        rd_data->reject_reason = REJECT_REASON_TOO_MANY_ARGUMENTS;
        return BACNET_STATUS_REJECT;   
    } else if (len > request_len) {
        APP_ERROR("%s: missing required parameter\r\n", __func__);
        rd_data->reject_reason = REJECT_REASON_MISSING_REQUIRED_PARAMETER;
        return BACNET_STATUS_REJECT; 
    }

    return len;
}

static int rd_encode_apdu(bacnet_buf_t *apdu, uint8_t invoke_id, BACNET_REINITIALIZED_STATE state,
            BACNET_CHARACTER_STRING *password)
{
    uint8_t *pdu;
    int len;

    pdu = apdu->data;
    pdu[0] = PDU_TYPE_CONFIRMED_SERVICE_REQUEST;
    pdu[1] = encode_max_segs_max_apdu(0, MAX_APDU);
    pdu[2] = invoke_id;
    pdu[3] = SERVICE_CONFIRMED_REINITIALIZE_DEVICE;
    
    len = 4;
    len += encode_context_enumerated(&pdu[len], 0, state);

    /* optional password */
    if (password) {
        /* FIXME: must be at least 1 character, limited to 20 characters */
        len += encode_context_character_string(&pdu[len], 1, password);
    }

    apdu->data_len = len;

    return len;
}

void handler_reinitialize_device(BACNET_CONFIRMED_SERVICE_DATA *service_data, 
        bacnet_buf_t *reply_apdu, bacnet_addr_t *src)
{
    BACNET_REINITIALIZE_DEVICE_DATA rd_data;
    int len;

    len = rd_decode_service_request(service_data->service_request, service_data->service_request_len, 
        &rd_data);
    if (len < 0) {
        APP_ERROR("%s: decode service request failed(%d)\r\n", __func__, len);
        goto failed;
    }
    
    /* TODO: Check to see if the current Device supports routed service. */

    if (!device_reinitialize(&rd_data)) {
        APP_ERROR("%s: reinitialize device failed\r\n", __func__);
        goto failed;
    }

    len = encode_simple_ack(reply_apdu->data, service_data->invoke_id,
        SERVICE_CONFIRMED_REINITIALIZE_DEVICE);

    if (len < 0) {
        goto failed;
    }

    reply_apdu->data_len = len;
    return;

failed:
    reply_apdu->data_len = 0;
    
    if (len == BACNET_STATUS_ERROR) {
        len = bacerror_encode_apdu(reply_apdu, service_data->invoke_id,
            SERVICE_CONFIRMED_REINITIALIZE_DEVICE, rd_data.error_class, rd_data.error_code);
    } else if (len == BACNET_STATUS_ABORT) {
        len = abort_encode_apdu(reply_apdu, service_data->invoke_id,
            rd_data.abort_reason, true);
    } else if (len == BACNET_STATUS_REJECT) {
        len = reject_encode_apdu(reply_apdu, service_data->invoke_id,
            rd_data.reject_reason);
    } else {
        APP_ERROR("%s: unknown error(%d)\r\n", __func__, len);
        len = BACNET_STATUS_ERROR;
    }

    if (len < 0) {
        reply_apdu->data_len = 0;
    }
    
    return;
}

/** Send_Reinitialize_Device_Request - Sends a Reinitialize Device (RD) request.
 *
 * @device_id: [in] The index to the device address in our address cache.
 * @state: [in] Specifies the desired state of the device after reinitialization.
 * @password: [in] Optional password, up to 20 chars.
 *
 * @return 0 if successful, or negative if device is not bound or no tsm available
 *
 */
int Send_Reinitialize_Device_Request(uint8_t invoke_id, uint32_t device_id,
        BACNET_REINITIALIZED_STATE state, char *password)
{
    BACNET_CHARACTER_STRING password_string;
    bacnet_addr_t dst;
    DECLARE_BACNET_BUF(tx_apdu, MAX_APDU);
    uint32_t max_apdu;
    int len;
    int rv;

    if (!is_app_exist) {
        APP_ERROR("%s: application layer do not exist\r\n", __func__);
        return -EPERM;
    }
    
    /* if we are forbidden to send, don't send! */
    if (!dcc_communication_enabled()) {
        APP_ERROR("%s: dcc communication disabled\r\n", __func__);
        return -EPERM;
    }
    
    /* is the device bound? */
    if (!query_address_from_device(device_id, &max_apdu, &dst)) {
        APP_ERROR("%s: get address from device(%d) failed\r\n", __func__, 
            device_id);
        return -EPERM;
    }

    if ((dst.net == BACNET_BROADCAST_NETWORK) || (dst.len == 0)) {
        APP_ERROR("%s: invalid dst addr\r\n", __func__);
        return -EINVAL;
    }

    (void)characterstring_init_ansi(&password_string, password, strlen(password));

    (void)bacnet_buf_init(&tx_apdu.buf, MAX_APDU);
    len = rd_encode_apdu(&tx_apdu.buf, invoke_id, state, password? &password_string: NULL);
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

