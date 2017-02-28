/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * wpm.c
 * Original Author:  linzhixian, 2015-1-15
 *
 * Write Property Multiple
 *
 * History
 */

#include <errno.h>
#include <string.h>

#include "bacnet/service/wpm.h"
#include "bacnet/bacenum.h"
#include "bacnet/bacapp.h"
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

struct wpm_object_data {
    BACNET_OBJECT_TYPE object_type;
    uint32_t object_instance;
    uint32_t property_count;
};

struct wpm_property_data {
    BACNET_PROPERTY_ID property_id;
    uint32_t array_index;
    uint8_t priority;
    uint8_t offset_to_last_end;
    uint16_t value_len;
};

#define WPM_DECODER_BUF_SIZE                ((MAX_APDU - 4 - 7) / 4 + 1)

typedef struct wpm_req_decodeer {
    struct wpm_property_data buffer[WPM_DECODER_BUF_SIZE];
    struct wpm_object_data *object_data;
    struct wpm_property_data *property_data;
    uint32_t object_count;
    uint8_t *last_value_end;
} WPM_REQ_DECODER;

/* return MAX_BACNET_REJECT_REASON if decode success */
static BACNET_REJECT_REASON wpm_req_decode(WPM_REQ_DECODER *decoder,
        uint8_t *service_data, uint32_t service_data_len)
{
    int len;
    struct wpm_object_data *object;
    struct wpm_property_data *property;
    uint8_t *last_end;

    if (!decoder || !service_data)
        return REJECT_REASON_OTHER;

    decoder->object_data = (struct wpm_object_data*)decoder->buffer;
    decoder->property_data = decoder->buffer + WPM_DECODER_BUF_SIZE - 1;
    decoder->last_value_end = service_data;

    object = decoder->object_data;
    property = decoder->property_data;
    last_end = decoder->last_value_end;

    do {
        len = decode_context_object_id(service_data, 0, &(object->object_type), &(object->object_instance));
        if (len < 0 || len >= service_data_len) {
            APP_ERROR("%s: invalid object_id\r\n", __func__);
            return REJECT_REASON_INVALID_TAG;
        }

        if (decode_opening_tag(&service_data[len], 1) < 0) {
            APP_ERROR("%s: invalid opening tag_number\r\n", __func__);
            return REJECT_REASON_INVALID_TAG;
        }
        len++;
        service_data += len;
        service_data_len -= len;
        object->property_count = 0;

        do {
            len = decode_context_enumerated(service_data, 0, &property->property_id);
            if (len < 0 || len >= service_data_len) {
                APP_ERROR("%s: invalid property_id\r\n", __func__);
                return REJECT_REASON_INVALID_TAG;
            }
            service_data += len;
            service_data_len -= len;

            /* Tag 1 - optional Property Array Index */
            if (decode_has_context_tag(service_data, 1)) {
                len = decode_context_unsigned(service_data, 1, &property->array_index);
                if (len < 0 || len >= service_data_len) {
                    APP_ERROR("%s: invalid array_index\r\n", __func__);
                    return REJECT_REASON_INVALID_TAG;
                }
                if (property->array_index == BACNET_ARRAY_ALL) {
                    APP_ERROR("%s: invalid array_index\r\n", __func__);
                    return REJECT_REASON_PARAMETER_OUT_OF_RANGE;
                }
                service_data += len;
                service_data_len -= len;
            } else
                property->array_index = BACNET_ARRAY_ALL;

            len = decode_constructed_tag(service_data, service_data_len, 2);
            if (len < 0 || len >= service_data_len) {
                APP_ERROR("%s: invaid property value\r\n", __func__);
                return REJECT_REASON_INVALID_TAG;
            }
            property->value_len = len - CONTEXT_TAG_LENGTH(2) * 2;
            property->offset_to_last_end = service_data + CONTEXT_TAG_LENGTH(2) - last_end;
            last_end = service_data + len - CONTEXT_TAG_LENGTH(2);

            service_data += len;
            service_data_len -= len;

            /* Tag 3 - optional Priority */
            if (decode_has_context_tag(service_data, 3)) {
                uint32_t unsigned_value;
                len = decode_context_unsigned(service_data, 3, &unsigned_value);
                if (len < 0 || len >= service_data_len) {
                    APP_ERROR("%s: invalid priority\r\n", __func__);
                    return REJECT_REASON_INVALID_TAG;
                }
                if ((unsigned_value >= BACNET_MIN_PRIORITY) && (unsigned_value <= BACNET_MAX_PRIORITY)) {
                    property->priority = (uint8_t)unsigned_value;
                } else {
                    APP_ERROR("%s: invalid priority(%d)\r\n", __func__, unsigned_value);
                    return REJECT_REASON_PARAMETER_OUT_OF_RANGE;
                }
                service_data += len;
                service_data_len -= len;
            } else
                property->priority = BACNET_MAX_PRIORITY;

            object->property_count++;
            property--;

            if (decode_closing_tag(service_data, 1) >= 0) {
                service_data++;
                service_data_len--;
                object++;
                break;
            }
        } while(1);
    } while (service_data_len);

    decoder->object_count = object - decoder->object_data;
    return MAX_BACNET_REJECT_REASON;
}

/*
 * return true if have another write
 */
static bool wpm_req_decoder_iter(WPM_REQ_DECODER *decoder,
        BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    if (!decoder || !wp_data)
        return false;

    while (decoder->object_count) {
        while(decoder->object_data->property_count) {
            wp_data->object_type = decoder->object_data->object_type;
            wp_data->object_instance = decoder->object_data->object_instance;
            wp_data->property_id = decoder->property_data->property_id;
            wp_data->array_index = decoder->property_data->array_index;
            wp_data->priority = decoder->property_data->priority;
            wp_data->application_data = decoder->last_value_end + decoder->property_data->offset_to_last_end;
            wp_data->application_data_len = decoder->property_data->value_len;

            decoder->last_value_end = wp_data->application_data + wp_data->application_data_len;
            decoder->property_data--;
            decoder->object_data->property_count--;
            return true;
        }
        decoder->object_data++;
        decoder->object_count--;
    }

    return false;
}

static int wpm_error_ack_encode_apdu(bacnet_buf_t *apdu, uint8_t invoke_id, 
            BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    uint8_t *pdu;
    int len;

    if ((apdu == NULL) || (apdu->data == NULL) || (wp_data == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return BACNET_STATUS_ERROR;
    }

    pdu = apdu->data;

    pdu[0] = PDU_TYPE_ERROR << 4;
    pdu[1] = invoke_id;
    pdu[2] = SERVICE_CONFIRMED_WRITE_PROP_MULTIPLE;
    
    len = 3;
    len += encode_opening_tag(&pdu[len], 0);
    len += encode_application_enumerated(&pdu[len], wp_data->error_class);
    len += encode_application_enumerated(&pdu[len], wp_data->error_code);
    len += encode_closing_tag(&pdu[len], 0);
    
    len += encode_opening_tag(&pdu[len], 1);
    len += encode_context_object_id(&pdu[len], 0, wp_data->object_type, wp_data->object_instance);
    len += encode_context_enumerated(&pdu[len], 1, wp_data->property_id);
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        len += encode_context_unsigned(&pdu[len], 2, wp_data->array_index);
    }
    len += encode_closing_tag(&pdu[len], 1);

    apdu->data_len = len;
    
    return len;
}

void handler_write_property_multiple(BACNET_CONFIRMED_SERVICE_DATA *service_data, 
        bacnet_buf_t *reply_apdu, bacnet_addr_t *src)
{
    BACNET_WRITE_PROPERTY_DATA wp_data;
    WPM_REQ_DECODER decoder;
    int len;
    BACNET_REJECT_REASON reject_reason;
    
    reject_reason = wpm_req_decode(&decoder, service_data->service_request,
            service_data->service_request_len);

    if (reject_reason != MAX_BACNET_REJECT_REASON) {
        APP_ERROR("%s: wpm decode failed\r\n", __func__);
        goto rejected;
    }

    while (wpm_req_decoder_iter(&decoder, &wp_data)) {
        len = object_write_property(&wp_data);
        if (len < 0) {
            APP_ERROR("%s: device write property failed\r\n", __func__);
            goto failed;
        }
    }

    /* ack */
    len = encode_simple_ack(reply_apdu->data, service_data->invoke_id,
        SERVICE_CONFIRMED_WRITE_PROP_MULTIPLE);

    if (len < 0) {
        reply_apdu->data_len = 0;
    } else {
        reply_apdu->data_len = len;
    }
    return;

rejected:
    len = reject_encode_apdu(reply_apdu, service_data->invoke_id, reject_reason);
    goto out;

failed:
    reply_apdu->data_len = 0;
    
    if (len == BACNET_STATUS_ERROR) {
        len = wpm_error_ack_encode_apdu(reply_apdu, service_data->invoke_id, &wp_data);
    } else if (len == BACNET_STATUS_ABORT) {
        len = abort_encode_apdu(reply_apdu, service_data->invoke_id,
            wp_data.abort_reason, true);
    } else {
        APP_ERROR("%s: unknown error(%d)\r\n", __func__, len);
    }

out:
    if (len < 0) {
        reply_apdu->data_len = 0;
    }
    
    return;
}

bool wpm_req_encode_object(bacnet_buf_t *apdu, BACNET_OBJECT_TYPE object_type,
                uint32_t object_instance)
{
    int len;
    uint8_t *buf = apdu->data + apdu->data_len;

    if (apdu->data_len) {
        len = encode_closing_tag(buf, 2); /* end of property value */
        if (apdu->data[1]) /* priority */
            len += encode_context_unsigned(&buf[len], 3, apdu->data[1]);
        len += encode_closing_tag(&buf[len], 1); /* end of property list */
    } else {
        len = 4;
    }

    len += encode_context_object_id(&buf[len], 0, object_type, object_instance);
    len += encode_opening_tag(&buf[len], 1);
    apdu->data_len += len;
    apdu->data[0] = false; /* temporary property set flag */
    return buf + len < apdu->end;
}

bool wpm_req_encode_property(bacnet_buf_t *apdu, BACNET_PROPERTY_ID property_id,
                uint32_t array_index, uint8_t priority)
{
    int len;
    uint8_t *buf = apdu->data + apdu->data_len;

    if (apdu->data[0]) {
        len = encode_closing_tag(buf, 2); /* end of property value */
        if (apdu->data[1]) /* priority */
            len += encode_context_unsigned(&buf[len], 3, apdu->data[1]);
    } else
        len = 0;

    len += encode_context_enumerated(&buf[len], 0, property_id);
    if (array_index != BACNET_ARRAY_ALL)
        len += encode_context_unsigned(&buf[len], 1, array_index);

    len += encode_opening_tag(&buf[len], 2);
    apdu->data_len += len;
    apdu->data[0] = true;
    apdu->data[1] = priority; /* temporarily save priority */
    return buf + len < apdu->end;
}

bool wpm_req_encode_end(bacnet_buf_t *apdu, uint8_t invoke_id)
{
    int len;
    uint8_t *buf = apdu->data + apdu->data_len;

    if (!apdu->data_len || !apdu->data[0]) {
        APP_ERROR("%s: no object or no property in object\r\n", __func__);
        return false;
    }

    len = encode_closing_tag(buf, 2); /* end of property value */
    if (apdu->data[1]) /* priority */
        len += encode_context_unsigned(&buf[len], 3, apdu->data[1]);
    len += encode_closing_tag(&buf[len], 1); /* end of property list */

    apdu->data[0] = PDU_TYPE_CONFIRMED_SERVICE_REQUEST;
    apdu->data[1] = encode_max_segs_max_apdu(0, MAX_APDU);
    apdu->data[2] = invoke_id;
    apdu->data[3] = SERVICE_CONFIRMED_WRITE_PROP_MULTIPLE;

    apdu->data_len += len;
    return buf + len <= apdu->end;
}
