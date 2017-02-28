/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * abort.c
 * Original Author:  linzhixian, 2015-2-12
 *
 * BACnet Abort Choice
 *
 * History
 */

#include "bacnet/service/abort.h"
#include "bacnet/app.h"

BACNET_ABORT_REASON abort_convert_error_code(BACNET_ERROR_CODE error_code)
{
    BACNET_ABORT_REASON abort_code;

    switch (error_code) {
        case ERROR_CODE_ABORT_BUFFER_OVERFLOW:
            abort_code = ABORT_REASON_BUFFER_OVERFLOW;
            break;
        case ERROR_CODE_ABORT_INVALID_APDU_IN_THIS_STATE:
            abort_code = ABORT_REASON_INVALID_APDU_IN_THIS_STATE;
            break;
        case ERROR_CODE_ABORT_PREEMPTED_BY_HIGHER_PRIORITY_TASK:
            abort_code = ABORT_REASON_PREEMPTED_BY_HIGHER_PRIORITY_TASK;
            break;
        case ERROR_CODE_ABORT_SEGMENTATION_NOT_SUPPORTED:
            abort_code = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
            break;
        case ERROR_CODE_ABORT_PROPRIETARY:
            abort_code = FIRST_PROPRIETARY_ABORT_REASON;
            break;
        case ERROR_CODE_ABORT_OTHER:
        default:
            abort_code = ABORT_REASON_OTHER;
            break;
    }

    return abort_code;
}

/* return the total length of the apdu */
int abort_encode_apdu(bacnet_buf_t *apdu, uint8_t invoke_id, uint8_t abort_reason, bool server)
{
    uint8_t *pdu;

    pdu = apdu->data;
    if (server) {
        pdu[0] = (PDU_TYPE_ABORT << 4) | 1;
    } else {
        pdu[0] = PDU_TYPE_ABORT << 4;
    }

    pdu[1] = invoke_id;
    pdu[2] = abort_reason;

    apdu->data_len = 3;
    
    return apdu->data_len;
}

