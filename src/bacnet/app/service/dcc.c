/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * dcc.c
 * Original Author:  linzhixian, 2015-1-27
 *
 * Device Communication Control
 *
 * History
 */

#include <errno.h>
#include <string.h>
#include "bacnet/service/error.h"
#include "bacnet/service/abort.h"
#include "bacnet/service/reject.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacdef.h"
#include "bacnet/service/dcc.h"

#include "bacnet/addressbind.h"
#include "bacnet/tsm.h"
#include "bacnet/network.h"
#include "bacnet/app.h"

static char My_Password[32] = "filister";

static uint32_t DCC_Time_Duration_Seconds = 0;

static BACNET_COMMUNICATION_ENABLE_DISABLE DCC_Enable_Disable = COMMUNICATION_ENABLE;

extern bool is_app_exist;

bool dcc_communication_enabled(void)
{
    return (DCC_Enable_Disable == COMMUNICATION_ENABLE);
}

/* When network communications are completely disabled,
   only DeviceCommunicationControl and ReinitializeDevice APDUs
   shall be processed and no messages shall be initiated.*/
bool dcc_communication_disabled(void)
{
    return (DCC_Enable_Disable == COMMUNICATION_DISABLE);
}

/* When the initiation of communications is disabled,
   all APDUs shall be processed and responses returned as
   required and no messages shall be initiated with the
   exception of I-Am requests, which shall be initiated only in
   response to Who-Is messages. In this state, a device that
   supports I-Am request initiation shall send one I-Am request
   for any Who-Is request that is received if and only if
   the Who-Is request does not contain an address range or
   the device is included in the address range. */
bool dcc_communication_initiation_disabled(void)
{
    return (DCC_Enable_Disable == COMMUNICATION_DISABLE_INITIATION);
}

static int dcc_decode_service_request(uint8_t *service_request, uint16_t request_len, 
            BACNET_DCC_DATA *dcc_data)
{
    uint32_t unsigned_value;
    int len, dec_len;

    if ((service_request == NULL) || (dcc_data == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return BACNET_STATUS_REJECT;
    }

    /* Tag 0: optional timeDuration, in minutes. But if not included, take it as indefinite,
     * which we return as "very large" */
    len = 0;
    if (decode_has_context_tag(service_request, 0)) {
        dec_len = decode_context_unsigned(&service_request[len], 0, &unsigned_value);
        if (dec_len < 0) {
            dcc_data->reject_reason = REJECT_REASON_INVALID_TAG;
            return BACNET_STATUS_REJECT;
        }
        len += dec_len;
        dcc_data->timeDuration = (uint16_t)unsigned_value;
    } else {
        dcc_data->timeDuration = 0;     /* zero indicates infinite duration and results in no timeout */
    }

    /* Tag 1: enable-disable */
    dec_len = decode_context_enumerated(&service_request[len], 1, &unsigned_value);
    if (dec_len < 0) {
        APP_ERROR("%s: invalid enable-disable\r\n", __func__);
        dcc_data->reject_reason = REJECT_REASON_INVALID_TAG;
        return BACNET_STATUS_REJECT;
    }
    len += dec_len;
    dcc_data->enable_disable = (BACNET_COMMUNICATION_ENABLE_DISABLE)unsigned_value;


    /* Tag 2: optional password */
    if (len < request_len && decode_has_context_tag(&service_request[len], 2)) {
        dec_len = decode_context_character_string(&service_request[len], 2, &(dcc_data->password));
        if (dec_len < 0) {
            APP_ERROR("%s: invalid password\r\n", __func__);
            dcc_data->reject_reason = REJECT_REASON_INVALID_TAG;
            return BACNET_STATUS_REJECT;
        }
        len += dec_len;
    }

    if (len < request_len) {
        APP_ERROR("%s: too many arguments\r\n", __func__);
        dcc_data->reject_reason = REJECT_REASON_TOO_MANY_ARGUMENTS;
        return BACNET_STATUS_REJECT;
    } else if (len > request_len) {
        APP_ERROR("%s: missing required parameter\r\n", __func__);
        dcc_data->reject_reason = REJECT_REASON_MISSING_REQUIRED_PARAMETER;
        return BACNET_STATUS_REJECT;
    }

    return len;
}

static int dcc_encode_apdu(bacnet_buf_t *apdu, uint8_t invoke_id, uint16_t timeDuration,
            BACNET_COMMUNICATION_ENABLE_DISABLE enable_disable, BACNET_CHARACTER_STRING *password)
{
    uint8_t *pdu;
    int len;

    pdu = apdu->data;
    pdu[0] = PDU_TYPE_CONFIRMED_SERVICE_REQUEST;
    pdu[1] = encode_max_segs_max_apdu(0, MAX_APDU);
    pdu[2] = invoke_id;
    pdu[3] = SERVICE_CONFIRMED_DEVICE_COMMUNICATION_CONTROL;
    len = 4;
    
    /* optional timeDuration */
    if (timeDuration) {
        len += encode_context_unsigned(&pdu[len], 0, timeDuration);
    }

    /* enable disable */
    len += encode_context_enumerated(&pdu[len], 1, enable_disable);

    /* optional password */
    if (password) {
        /* FIXME: must be at least 1 character, limited to 20 characters */
        len += encode_context_character_string(&pdu[len], 2, password);
    }

    apdu->data_len = len;

    return len;
}

bool dcc_set_status_duration(BACNET_COMMUNICATION_ENABLE_DISABLE status, uint16_t minutes)
{
    bool valid;

    valid = false;
    if (status < MAX_BACNET_COMMUNICATION_ENABLE_DISABLE) {
        DCC_Enable_Disable = status;
        if (status == COMMUNICATION_ENABLE) {
            DCC_Time_Duration_Seconds = 0;      /* infinite time duration is defined as 0 */
        } else {
            DCC_Time_Duration_Seconds = minutes * 60;
        }
        valid = true;
    }

    return valid;
}

void handler_device_communication_control(BACNET_CONFIRMED_SERVICE_DATA *service_data, 
        bacnet_buf_t *reply_apdu, bacnet_addr_t *src)
{
    BACNET_DCC_DATA dcc_data;
    int len;

    len = dcc_decode_service_request(service_data->service_request, 
        service_data->service_request_len, &dcc_data);
    if (len < 0) {
        APP_ERROR("%s: decode service request failed(%d)\r\n", __func__, len);
        goto failed;
    }

    /* TODO: Check to see if the current Device supports routed service. */
    
    if (!characterstring_ansi_same(&(dcc_data.password), My_Password, strlen(My_Password))) {
        dcc_data.error_class = ERROR_CLASS_SECURITY;
        dcc_data.error_code = ERROR_CODE_PASSWORD_FAILURE;
        len = BACNET_STATUS_ERROR;
        goto failed;
    }

    (void)dcc_set_status_duration(dcc_data.enable_disable, dcc_data.timeDuration);
    
    len = encode_simple_ack(reply_apdu->data, service_data->invoke_id,
        SERVICE_CONFIRMED_DEVICE_COMMUNICATION_CONTROL);
    
    if (len < 0) {
        goto failed;
    }

    reply_apdu->data_len = len;
    return;
    
failed:
    reply_apdu->data_len = 0;

    if (len == BACNET_STATUS_ERROR) {
        len = bacerror_encode_apdu(reply_apdu, service_data->invoke_id,
            SERVICE_CONFIRMED_DEVICE_COMMUNICATION_CONTROL, dcc_data.error_class, dcc_data.error_code);
    } else if (len == BACNET_STATUS_ABORT) {
        len = abort_encode_apdu(reply_apdu, service_data->invoke_id,
            dcc_data.abort_reason, true);
    } else if (len == BACNET_STATUS_REJECT) {
        len = reject_encode_apdu(reply_apdu, service_data->invoke_id,
            dcc_data.reject_reason);
    } else {
        APP_ERROR("%s: unknown error(%d)\r\n", __func__, len);
        len = BACNET_STATUS_ERROR;
    }

    if (len < 0) {
        reply_apdu->data_len = 0;
    }
    
    return;
}

/** Send_Device_Communication_Control_Request - Sends a Device Communication Control (DCC) request.
 *
 * @device_id: [in] The index to the device address in our address cache.
 * @timeDuration: [in] If non-zero, the minutes that the remote device
 *            shall ignore all APDUs except DCC and, if supported, RD APDUs.
 * @state: [in] Choice to Enable or Disable communication.
 * @password: [in] Optional password, up to 20 chars.
 *
 * @return 0 if successful, or negative if device is not bound or no tsm available
 *
 */
int Send_Device_Communication_Control_Request(uint8_t invoke_id, uint32_t device_id, 
        uint16_t timeDuration, BACNET_COMMUNICATION_ENABLE_DISABLE state, char *password)
{
    BACNET_CHARACTER_STRING password_string;
    bacnet_addr_t dst;
    uint32_t max_apdu;
    DECLARE_BACNET_BUF(tx_apdu, MAX_APDU);
    int len;
    int rv;

    if (!is_app_exist) {
        APP_ERROR("%s: application layer do not exist\r\n", __func__);
        return -EPERM;
    }
    
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
    len = dcc_encode_apdu(&tx_apdu.buf, invoke_id, timeDuration, state,
        password ? &password_string : NULL);
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

