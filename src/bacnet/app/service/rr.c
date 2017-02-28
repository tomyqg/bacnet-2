/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * rr.c
 * Original Author:  linzhixian, 2016-5-26
 *
 * Read Range
 *
 * History
 */

#include <errno.h>

#include "bacnet/service/rr.h"
#include "bacnet/bacdcode.h"
#include "bacnet/addressbind.h"
#include "bacnet/object/device.h"
#include "bacnet/service/error.h"
#include "bacnet/service/abort.h"
#include "bacnet/service/reject.h"
#include "bacnet/service/dcc.h"
#include "bacnet/app.h"
#include "bacnet/network.h"

extern bool is_app_exist;

int rr_encode_service_request(bacnet_buf_t *apdu, uint8_t invoke_id, BACNET_READ_RANGE_DATA *rrdata)
{
    uint8_t *pdu;
    int len;

    if ((apdu == NULL) || (rrdata == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    pdu = apdu->data;
    pdu[0] = PDU_TYPE_CONFIRMED_SERVICE_REQUEST;
    pdu[1] = encode_max_segs_max_apdu(0, MAX_APDU);
    pdu[2] = invoke_id;
    pdu[3] = SERVICE_CONFIRMED_READ_RANGE;
    len = 4;
    
    len += encode_context_object_id(&pdu[len], 0, rrdata->rpdata.object_type,
            rrdata->rpdata.object_instance);

    len += encode_context_enumerated(&pdu[len], 1, rrdata->rpdata.property_id);
    
    /* optional array index */
    if (rrdata->rpdata.array_index != BACNET_ARRAY_ALL) {
        len += encode_context_unsigned(&pdu[len], 2, rrdata->rpdata.array_index);
    }
    
    /* Build the appropriate (optional) range parameter based on the request type */
    switch (rrdata->Range.RequestType) {
    case RR_BY_POSITION:
        len += encode_opening_tag(&pdu[len], 3);
        len += encode_application_unsigned(&pdu[len], rrdata->Range.Range.RefIndex);
        len += encode_application_signed(&pdu[len], rrdata->Range.Count);
        len += encode_closing_tag(&pdu[len], 3);
        break;

    case RR_BY_SEQUENCE:
        len += encode_opening_tag(&pdu[len], 6);
        len += encode_application_unsigned(&pdu[len], rrdata->Range.Range.RefSeqNum);
        len += encode_application_signed(&pdu[len], rrdata->Range.Count);
        len += encode_closing_tag(&pdu[len], 6);
        break;

    case RR_BY_TIME:
        len += encode_opening_tag(&pdu[len], 7);
        len += encode_application_date(&pdu[len], &rrdata->Range.Range.RefTime.date);
        len += encode_application_time(&pdu[len], &rrdata->Range.Range.RefTime.time);
        len += encode_application_signed(&pdu[len], rrdata->Range.Count);
        len += encode_closing_tag(&pdu[len], 7);
        break;

    case RR_READ_ALL:
        /* to attempt a read of the whole array or list, omit the range parameter */
        break;
    
    default:
        APP_ERROR("%s: invalid RR RequestType(%d)\r\n", __func__, rrdata->Range.RequestType);
        return -EPERM;
    }

    apdu->data_len += len;
    
    return len;
}

static int rr_decode_service_request(uint8_t *apdu, uint32_t apdu_len,
            BACNET_READ_RANGE_DATA *rrdata)
{
    uint8_t tag_number;
    int dec_len;
    int len;
    
    /* Tag 0: Object ID */
    len = decode_context_object_id(apdu, 0, &rrdata->rpdata.object_type,
        &rrdata->rpdata.object_instance);
    if (len < 0) {
        APP_ERROR("%s: decode Object ID failed(%d)\r\n", __func__, len);
        rrdata->rpdata.reject_reason = REJECT_REASON_INVALID_TAG;
        return BACNET_STATUS_REJECT;
    }

    /* Tag 1: Property ID */
    dec_len = decode_context_enumerated(&apdu[len], 1, &rrdata->rpdata.property_id);
    if (dec_len < 0) {
        APP_ERROR("%s: decode Property ID failed(%d)\r\n", __func__, dec_len);
        rrdata->rpdata.reject_reason = REJECT_REASON_INVALID_TAG;
        return BACNET_STATUS_REJECT;
    }
    len += dec_len;

    /* Tag 2: Optional Array Index - set to ALL if not present */
    rrdata->rpdata.array_index = BACNET_ARRAY_ALL;
    if ((len < apdu_len) && (decode_has_context_tag(&apdu[len], 2))) {
        dec_len = decode_context_unsigned(&apdu[len], 2, &rrdata->rpdata.array_index);
        if (dec_len < 0) {
            APP_ERROR("%s: decode Array Index failed(%d)\r\n", __func__, dec_len);
            rrdata->rpdata.reject_reason = REJECT_REASON_INVALID_TAG;
            return BACNET_STATUS_REJECT;
        }
        len += dec_len;
    }

    /* Optional Range */
    rrdata->Range.RequestType = RR_READ_ALL;
    if (len < apdu_len) {
        if (!IS_OPENING_TAG(apdu[len])) {
            APP_ERROR("%s: decode Range opening tag failed\r\n", __func__);
            rrdata->rpdata.reject_reason = REJECT_REASON_INVALID_TAG;
            return BACNET_STATUS_REJECT;
        }
    
        tag_number = get_tag_number(&apdu[len++]);
        switch (tag_number) {
        case 3:
            rrdata->Range.RequestType = RR_BY_POSITION;
            dec_len = decode_application_unsigned(&apdu[len], &rrdata->Range.Range.RefIndex);
            if (dec_len < 0) {
                APP_ERROR("%s: decode ByPosition Reference Index failed(%d)\r\n", __func__, dec_len);
                rrdata->rpdata.reject_reason = REJECT_REASON_INVALID_TAG;
                return BACNET_STATUS_REJECT;
            }
            len += dec_len;

            dec_len = decode_application_signed(&apdu[len], &rrdata->Range.Count);
            if (dec_len < 0) {
                APP_ERROR("%s: decode ByPosition Count failed(%d)\r\n", __func__, dec_len);
                rrdata->rpdata.reject_reason = REJECT_REASON_INVALID_TAG;
                return BACNET_STATUS_REJECT;
            }
            len += dec_len;
            
            break;

        case 6:
            rrdata->Range.RequestType = RR_BY_SEQUENCE;
            dec_len = decode_application_unsigned(&apdu[len], &rrdata->Range.Range.RefSeqNum);
            if (dec_len < 0) {
                APP_ERROR("%s: decode BySequence Number failed(%d)\r\n", __func__, dec_len);
                rrdata->rpdata.reject_reason = REJECT_REASON_INVALID_TAG;
                return BACNET_STATUS_REJECT;
            }
            len += dec_len;

            dec_len = decode_application_signed(&apdu[len], &rrdata->Range.Count);
            if (dec_len < 0) {
                APP_ERROR("%s: decode BySequence Count failed(%d)\r\n", __func__, dec_len);
                rrdata->rpdata.reject_reason = REJECT_REASON_INVALID_TAG;
                return BACNET_STATUS_REJECT;
            }
            len += dec_len;
            
            break;

        case 7:
            rrdata->Range.RequestType = RR_BY_TIME;
            dec_len = decode_application_date(&apdu[len], &rrdata->Range.Range.RefTime.date);
            if (dec_len < 0) {
                APP_ERROR("%s: decode ByTime Reference Time failed(%d)\r\n", __func__, dec_len);
                rrdata->rpdata.reject_reason = REJECT_REASON_INVALID_TAG;
                return BACNET_STATUS_REJECT;
            }
            len += dec_len;

            dec_len = decode_application_time(&apdu[len], &rrdata->Range.Range.RefTime.time);
            if (dec_len < 0) {
                APP_ERROR("%s: decode ByTime Count failed(%d)\r\n", __func__, dec_len);
                rrdata->rpdata.reject_reason = REJECT_REASON_INVALID_TAG;
                return BACNET_STATUS_REJECT;
            }
            len += dec_len;

            break;
        
        default:
            APP_ERROR("%s: invalid Range tag number(%d)\r\n", __func__, tag_number);
            rrdata->rpdata.reject_reason = REJECT_REASON_INVALID_TAG;
            return BACNET_STATUS_REJECT;
        }

        dec_len = decode_closing_tag(&apdu[len], tag_number);
        if (dec_len < 0) {
            APP_ERROR("%s: decode Range closing tag_number failed(%d)\r\n", __func__, dec_len);
            rrdata->rpdata.reject_reason = REJECT_REASON_INVALID_TAG;
            return BACNET_STATUS_REJECT;
        }
        len += dec_len;
    }

    if (len != apdu_len) {
        if (len < apdu_len) {
            /* If something left over now, we have an invalid request */
            APP_ERROR("%s: too many arguments\r\n", __func__);
            rrdata->rpdata.reject_reason = REJECT_REASON_TOO_MANY_ARGUMENTS;
            return BACNET_STATUS_REJECT;
        } else {
            APP_ERROR("%s: missing argument\r\n", __func__);
            rrdata->rpdata.reject_reason = REJECT_REASON_MISSING_REQUIRED_PARAMETER;
            return BACNET_STATUS_REJECT;
        }
    }

    return len;
}

int rr_ack_decode(uint8_t *apdu, uint16_t apdu_len, BACNET_READ_RANGE_DATA *rrdata)
{
    uint32_t len_value_type;
    int dec_len;
    int len;

    /* Tag 0: Object ID */
    len = decode_context_object_id(apdu, 0, &rrdata->rpdata.object_type,
        &rrdata->rpdata.object_instance);
    if (len < 0) {
        APP_ERROR("%s: decode Object ID failed(%d)\r\n", __func__, len);
        return len;
    }
    
    /* Tag 1: Property ID */
    dec_len = decode_context_enumerated(&apdu[len], 1, &rrdata->rpdata.property_id);
    if (dec_len < 0) {
        APP_ERROR("%s: decode Property ID failed(%d)\r\n", __func__, dec_len);
        return dec_len;
    }
    len += dec_len;
    
    /* Tag 2: Optional Array Index */
    rrdata->rpdata.array_index = BACNET_ARRAY_ALL;
    if ((len < apdu_len) && (decode_has_context_tag(&apdu[len], 2))) {
        dec_len = decode_context_unsigned(&apdu[len], 2, &rrdata->rpdata.array_index);
        if (dec_len < 0) {
            APP_ERROR("%s: decode Array Index failed(%d)\r\n", __func__, dec_len);
            return -EPERM;
        }
        len += dec_len;
    }

    /* Tag 3: Result Flags */
    dec_len = decode_context_bitstring(&apdu[len], 3, &rrdata->ResultFlags);
    if (dec_len < 0) {
        APP_ERROR("%s: decode Result Flags failed(%d)\r\n", __func__, dec_len);
        return -EPERM;
    }
    len += dec_len;
        
    /* Tag 4: Item count */
    dec_len = decode_context_unsigned(&apdu[len], 4, &rrdata->ItemCount);
    if (dec_len < 0) {
        APP_ERROR("%s: decode Item Count failed(%d)\r\n", __func__, dec_len);
        return -EPERM;
    }
    len += dec_len;

    /* Tag 5: Item data */
    dec_len = decode_opening_tag(&apdu[len], 5);
    if (dec_len < 0) {
        APP_ERROR("%s: decode Item Data opending tag failed(%d)\r\n", __func__, dec_len);
        return dec_len;
    }
    len += dec_len;
    
    /* Setup the start position and length of the data returned from the request
     * don't decode the application tag number or its data here */
    rrdata->rpdata.application_data = &apdu[len];
    dec_len = len;
    while (len < apdu_len) {
        if (decode_closing_tag(&apdu[len], 5) > 0) {
            rrdata->rpdata.application_data_len = len - dec_len;
            len++;          /* Step over single byte closing tag */
            break;
        } else {
            /* Don't care about tag number, just skipping over anyway */
            len += decode_tag_number_and_value(&apdu[len], NULL, &len_value_type);
            len += len_value_type;      /* Skip over data value as well */
            if (len >= apdu_len) {      /* APDU is exhausted so we have failed to find closing tag */
                return -EPERM;
            }
        }
    }
    
    /* Tag 6: firstSequenceNumber */
    if (len < apdu_len) {
        dec_len = decode_context_unsigned(&apdu[len], 6, &rrdata->FirstSequence);
        if (dec_len < 0) {
            APP_ERROR("%s: decode firstSequenceNumber failed(%d)\r\n", __func__, dec_len);
            return -EPERM;
        }
        len += dec_len;
    }

    if (len != apdu_len) {
        return -EPERM;
    }
        
    return len;
}

void handler_read_range(BACNET_CONFIRMED_SERVICE_DATA *service_data, bacnet_buf_t *reply_apdu, 
        bacnet_addr_t *src)
{
    BACNET_READ_RANGE_DATA rrdata;
    uint8_t *pdu;
    int len;
    
    memset(&rrdata, 0, sizeof(rrdata));
    len = rr_decode_service_request(service_data->service_request, service_data->service_request_len,
        &rrdata);
    if (len < 0) {
        APP_ERROR("%s: decode service request failed(%d)\r\n", __func__, len);
        goto failed;
    }

    /* Test for case of indefinite Device object instance */
    if ((rrdata.rpdata.object_type == OBJECT_DEVICE)
            && (rrdata.rpdata.object_instance == BACNET_MAX_INSTANCE)) {
        rrdata.rpdata.object_instance = device_object_instance_number();
    }

    pdu = reply_apdu->data;
    pdu[0] = PDU_TYPE_COMPLEX_ACK << 4;
    pdu[1] = service_data->invoke_id;
    pdu[2] = SERVICE_CONFIRMED_READ_RANGE;
    len = 3;

    /* context 0 object id */
    len += encode_context_object_id(&pdu[len], 0, rrdata.rpdata.object_type,
            rrdata.rpdata.object_instance);

    /* context 1 property id */
    len += encode_context_enumerated(&pdu[len], 1, rrdata.rpdata.property_id);

    /* context 2 array index is optional */
    if (rrdata.rpdata.array_index != BACNET_ARRAY_ALL) {
        len += encode_context_unsigned(&pdu[len], 2, rrdata.rpdata.array_index);
    }

    reply_apdu->data_len = len;
    rrdata.rpdata.application_data = reply_apdu->data + reply_apdu->data_len;
    rrdata.rpdata.application_data_len = reply_apdu->end - reply_apdu->data - reply_apdu->data_len;

    len = object_read_property(&rrdata.rpdata, &rrdata.Range);
    if (len < 0) {
        APP_ERROR("%s: read Object(%d) Instance(%d) Property(%d) failed(%d)\r\n", __func__, 
            rrdata.rpdata.object_type, rrdata.rpdata.object_instance, rrdata.rpdata.property_id, len);
        goto failed;
    }
    reply_apdu->data_len += len;

    if (reply_apdu->data + reply_apdu->data_len > reply_apdu->end) {
        APP_ERROR("%s: Message is too large\r\n", __func__);
        rrdata.rpdata.abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
        len = BACNET_STATUS_ABORT;
        goto failed;
    }

    return;

failed:
    reply_apdu->data_len = 0;
    
    if (len == BACNET_STATUS_ERROR) {
        len = bacerror_encode_apdu(reply_apdu, service_data->invoke_id,
            SERVICE_CONFIRMED_READ_PROPERTY, rrdata.rpdata.error_class, rrdata.rpdata.error_code);
    } else if (len == BACNET_STATUS_ABORT) {
        len = abort_encode_apdu(reply_apdu, service_data->invoke_id, rrdata.rpdata.abort_reason, true);
    } else if (len == BACNET_STATUS_REJECT) {
        len = reject_encode_apdu(reply_apdu, service_data->invoke_id, rrdata.rpdata.reject_reason);
    } else {
        APP_ERROR("%s: unknown error(%d)\r\n", __func__, len);
        len = BACNET_STATUS_ERROR;
    }

    if (len < 0) {
        reply_apdu->data_len = 0;
    }
    
    return;
}

int Send_ReadRange_Request(tsm_invoker_t *invoker, BACNET_READ_RANGE_DATA *rrdata)
{
    DECLARE_BACNET_BUF(tx_apdu, MIN_APDU);
    int rv;

    if ((invoker == NULL) || (rrdata == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    (void)bacnet_buf_init(&tx_apdu.buf, MIN_APDU);
    rv = rr_encode_service_request(&tx_apdu.buf, invoker->invokeID, rrdata);
    if ((rv < 0) || (rv > MIN_APDU)) {
        APP_ERROR("%s: encode RR request failed(%d)\r\n", __func__, rv);
        return -EPERM;
    }

    rv = tsm_send_apdu(invoker, &tx_apdu.buf, PRIORITY_NORMAL, 0);
    if (rv < 0) {
        APP_ERROR("%s: tsm send failed(%d)\r\n", __func__, rv);
    }

    return rv;
}

