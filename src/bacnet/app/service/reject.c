/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * reject.c
 * Original Author:  linzhixian, 2015-2-12
 *
 * BACnet Reject Choice
 *
 * History
 */

#include "bacnet/service/reject.h"
#include "bacnet/app.h"

/* return the total length of the apdu */
int reject_encode_apdu(bacnet_buf_t *apdu, uint8_t invoke_id, uint8_t reject_reason)
{
    uint8_t *pdu;

    pdu = apdu->data;
    pdu[0] = PDU_TYPE_REJECT << 4;
    pdu[1] = invoke_id;
    pdu[2] = reject_reason;
    
    apdu->data_len = 3;

    return apdu->data_len;
}

BACNET_REJECT_REASON reject_convert_error_code(BACNET_ERROR_CODE error_code)
{
    BACNET_REJECT_REASON reject_code;

    switch (error_code) {
        case ERROR_CODE_REJECT_BUFFER_OVERFLOW:
            reject_code = REJECT_REASON_BUFFER_OVERFLOW;
            break;
        case ERROR_CODE_REJECT_INCONSISTENT_PARAMETERS:
            reject_code = REJECT_REASON_INCONSISTENT_PARAMETERS;
            break;
        case ERROR_CODE_REJECT_INVALID_PARAMETER_DATA_TYPE:
            reject_code = REJECT_REASON_INVALID_PARAMETER_DATA_TYPE;
            break;
        case ERROR_CODE_REJECT_INVALID_TAG:
            reject_code = REJECT_REASON_INVALID_TAG;
            break;
        case ERROR_CODE_REJECT_MISSING_REQUIRED_PARAMETER:
            reject_code = REJECT_REASON_MISSING_REQUIRED_PARAMETER;
            break;
        case ERROR_CODE_REJECT_PARAMETER_OUT_OF_RANGE:
            reject_code = REJECT_REASON_PARAMETER_OUT_OF_RANGE;
            break;
        case ERROR_CODE_REJECT_TOO_MANY_ARGUMENTS:
            reject_code = REJECT_REASON_TOO_MANY_ARGUMENTS;
            break;
        case ERROR_CODE_REJECT_UNDEFINED_ENUMERATION:
            reject_code = REJECT_REASON_UNDEFINED_ENUMERATION;
            break;
        case ERROR_CODE_REJECT_UNRECOGNIZED_SERVICE:
            reject_code = REJECT_REASON_UNRECOGNIZED_SERVICE;
            break;
        case ERROR_CODE_REJECT_PROPRIETARY:
            reject_code = FIRST_PROPRIETARY_REJECT_REASON;
            break;
        case ERROR_CODE_REJECT_OTHER:
        default:
            reject_code = REJECT_REASON_OTHER;
            break;
    }

    return reject_code;
}

void reject_handler(bacnet_buf_t *apdu, bacnet_addr_t *src)
{
    uint8_t *pdu;
    uint8_t invoke_id;
    uint8_t reason;

    if ((apdu == NULL) || (apdu->data == NULL) || (src == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return;
    }

    pdu = apdu->data;
    invoke_id = pdu[1];
    reason = pdu[2];
    
    APP_VERBOS("%s: invoke_id(%d), reason(%d)\r\n", __func__, invoke_id, reason);
}

