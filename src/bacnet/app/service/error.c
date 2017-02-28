/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * error.c
 * Original Author:  linzhixian, 2015-2-11
 *
 * BACnet Error Choice
 *
 * History
 */

#include <errno.h>

#include "bacnet/service/error.h"
#include "bacnet/bacdcode.h"
#include "bacnet/app.h"

static int bacerror_decode_service_request(uint8_t *pdu, uint16_t pdu_len, 
            BACNET_ERROR_CLASS *error_class, BACNET_ERROR_CODE *error_code)
{
    uint32_t decoded_value;
    int len, dec_len;
    
    /* decode error class */
    len = decode_application_enumerated(pdu, &decoded_value);
    if (len < 0) {
        APP_ERROR("%s: invalid error class\r\n", __func__);
        return -EPERM;
    }
    *error_class = (BACNET_ERROR_CLASS)decoded_value;
    
    /* decode error code */
    dec_len = decode_application_enumerated(&pdu[len], &decoded_value);
    if (dec_len < 0) {
        APP_ERROR("%s: invalid error code\r\n", __func__);
        return -EPERM;
    }
    len += dec_len;
    *error_code = (BACNET_ERROR_CODE)decoded_value;
    
    if (len < pdu_len) {
        APP_ERROR("%s: too much parameter\r\n", __func__);
        return -EPERM;
    } else if (len > pdu_len) {
        APP_ERROR("%s: missing required parameter\r\n", __func__);
        return -EPERM;
    }

    return len;
}

/* return the total length of the apdu */
int bacerror_encode_apdu(bacnet_buf_t *apdu, uint8_t invoke_id, BACNET_CONFIRMED_SERVICE service,
            BACNET_ERROR_CLASS error_class, BACNET_ERROR_CODE error_code)
{
    uint8_t *pdu;
    int len;

    pdu = apdu->data;
    pdu[0] = PDU_TYPE_ERROR << 4;
    pdu[1] = invoke_id;
    pdu[2] = service;
    
    len = 3;
    len += encode_application_enumerated(&pdu[len], error_class);
    len += encode_application_enumerated(&pdu[len], error_code);

    apdu->data_len = len;

    return len;
}

/* decode the whole APDU - mainly used for unit testing */
int bacerror_decode_apdu(bacnet_buf_t *apdu, uint8_t *invoke_id, BACNET_CONFIRMED_SERVICE *service,
        BACNET_ERROR_CLASS *error_class, BACNET_ERROR_CODE *error_code)
{
    uint8_t *pdu;
    int len;

    if ((apdu == NULL) || (apdu->data == NULL) || (apdu->data_len < 4)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    pdu = apdu->data;
    if ((pdu[0] >> 4) != PDU_TYPE_ERROR) {
        APP_ERROR("%s: invalid pdu type(%d)\r\n", __func__, pdu[0]);
        return -EPERM;
    }

    if (invoke_id) {
        *invoke_id = pdu[1];
    }

    if (service) {
        *service = (BACNET_CONFIRMED_SERVICE)pdu[2];
    }

    len = 3;
    len += bacerror_decode_service_request(&pdu[len], apdu->data_len - len, error_class, error_code);
    if (len < 0) {
        APP_ERROR("%s: decode service request failed(%d)\r\n", __func__, len);
        return len;
    }

    return len;
}

void bacerror_handler(bacnet_buf_t *apdu, bacnet_addr_t *src)
{
    uint8_t *pdu;
    uint8_t invoke_id;
    uint8_t service_choice;
    uint32_t error_class;
    uint32_t error_code;
    int len;

    if ((apdu == NULL) || (apdu->data == NULL) || (src == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return;
    }

    pdu = apdu->data;
    invoke_id = pdu[1];
    service_choice = pdu[2];

    if (service_choice >= MAX_BACNET_CONFIRMED_SERVICE) {
        APP_ERROR("%s: invalid service choice(%d)\r\n", __func__, service_choice);
        return;
    }

    if (apdu->data_len < 3) {
        APP_ERROR("%s: invalid apdu\r\n", __func__);
        return;
    }

    pdu += 3;
    len = apdu->data_len - 3;

    if (service_choice == SERVICE_CONFIRMED_PRIVATE_TRANSFER
            || service_choice == SERVICE_CONFIRMED_WRITE_PROP_MULTIPLE) {
        /* this service encode error in constructed tag 0 */
        int tmp_len = decode_constructed_tag(pdu, len, 0);
        if (tmp_len < 0){
            APP_ERROR("%s: invalid opening tag 0\r\n", __func__);
            return;
        }
        pdu += CONTEXT_TAG_LENGTH(0);
        len = tmp_len - CONTEXT_TAG_LENGTH(0) * 2;
    }

    if (bacerror_decode_service_request(pdu, len, &error_class, &error_code) < 0) {
        APP_ERROR("%s: decode error type fail\r\n", __func__);
        return;
    }

    APP_VERBOS("%s: invoke_id(%d), service_choice(%d), error_class(%d), error_code(%d)\r\n", __func__,
        invoke_id, service_choice, error_class, error_code);
}

