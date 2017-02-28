/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * rpm.c
 * Original Author:  linzhixian, 2015-1-15
 *
 * Read Property Multiple
 *
 * History
 */

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "bacnet/service/rpm.h"
#include "bacnet/service/rp.h"
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
#include "bacnet/bactext.h"
#include "bacnet/app.h"
#include "bacnet/object/device.h"

typedef struct BACnet_RPM_Data {
    BACNET_OBJECT_TYPE object_type;
    uint32_t object_instance;
    BACNET_PROPERTY_ID object_property;
    uint32_t array_index;
    union {
        struct {
            BACNET_ERROR_CLASS error_class;
            BACNET_ERROR_CODE error_code;
        };
        BACNET_REJECT_REASON reject_reason;
        BACNET_ABORT_REASON abort_reason;
    };
} BACNET_RPM_DATA;

static int rpm_decode_object_id(uint8_t *pdu, uint16_t pdu_len, BACNET_RPM_DATA *rpm_data)
{
    BACNET_OBJECT_TYPE object_type;
    int len;
    
    if ((pdu == NULL) || (rpm_data == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return BACNET_STATUS_REJECT;
    }

    /* Must be at least 2 tags and an object id */
    if (pdu_len < 5) {
        APP_ERROR("%s: invalid pdu_len(%d)\r\n", __func__, pdu_len);
        rpm_data->reject_reason = REJECT_REASON_MISSING_REQUIRED_PARAMETER;
        return BACNET_STATUS_REJECT;
    }

    len = 0;
    
    /* Tag 0: Object ID */
    if (!decode_has_context_tag(&pdu[len++], 0)) {
        APP_ERROR("%s: non-context tag\r\n", __func__);
        rpm_data->reject_reason = REJECT_REASON_INVALID_TAG;
        return BACNET_STATUS_REJECT;
    }
    len += decode_object_id(&pdu[len], &object_type, &(rpm_data->object_instance));
    rpm_data->object_type = (BACNET_OBJECT_TYPE)object_type;

    /* Tag 1: opening tag of sequence of ReadAccessSpecification, opening tag is only one octet */
    if (decode_opening_tag(&pdu[len++], 1) < 0) {
        APP_ERROR("%s: non-opening tag number 1\r\n", __func__);
        rpm_data->reject_reason = REJECT_REASON_INVALID_TAG;
        return BACNET_STATUS_REJECT;
    }
    
    return len;
}

/* only decode the object property portion of the service request */
/*  BACnetPropertyReference ::= SEQUENCE {
        propertyIdentifier [0] BACnetPropertyIdentifier,
        propertyArrayIndex [1] Unsigned OPTIONAL
                                        --used only with array datatype
                                        -- if omitted with an array the entire array is referenced
    }
*/
static int rpm_decode_object_property(uint8_t *pdu, uint16_t pdu_len, BACNET_RPM_DATA *rpm_data)
{
    uint32_t property_id;
    int len, dec_len;

    if ((pdu == NULL) || (pdu_len == 0) || (rpm_data == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return BACNET_STATUS_REJECT;
    }

    /* Tag 0: propertyIdentifier */
    len = decode_context_enumerated(pdu, 0, &property_id);
    if (len < 0) {
        APP_ERROR("%s: invalid property id\r\n", __func__);
        rpm_data->reject_reason = REJECT_REASON_INVALID_TAG;
        return BACNET_STATUS_REJECT;
    }
    rpm_data->object_property = (BACNET_PROPERTY_ID)property_id;
    
    /* Tag 1: Optional propertyArrayIndex */
    if (len < pdu_len && decode_has_context_tag(&pdu[len], 1)) {
        dec_len = decode_context_unsigned(&pdu[len], 1, &(rpm_data->array_index));
        if (dec_len < 0) {
            APP_ERROR("%s: invalid array index\r\n", __func__);
            rpm_data->reject_reason = REJECT_REASON_INVALID_TAG;
            return BACNET_STATUS_REJECT;
        }
        len += dec_len;
    } else
        rpm_data->array_index = BACNET_ARRAY_ALL;

    if (len >= pdu_len ) {
        APP_ERROR("%s: invalid pdu_len(%d)\r\n", __func__,
                pdu_len);
        rpm_data->reject_reason = REJECT_REASON_MISSING_REQUIRED_PARAMETER;
        return BACNET_STATUS_REJECT;
    }

    return len;
}

static int rpm_ack_encode_apdu_init(bacnet_buf_t *apdu, uint8_t invoke_id)
{
    uint8_t *pdu;

    if ((apdu == NULL) || (apdu->data == NULL)) {
        APP_ERROR("%s: invalid apdu\r\n", __func__);
        return BACNET_STATUS_REJECT;
    }

    pdu = apdu->data;
    pdu[0] = PDU_TYPE_COMPLEX_ACK << 4;                     /* complex ACK service */
    pdu[1] = invoke_id;                                     /* original invoke id from request */
    pdu[2] = SERVICE_CONFIRMED_READ_PROP_MULTIPLE;          /* service choice */

    apdu->data_len = 3;
    
    return apdu->data_len;
}

static int rpm_ack_encode_apdu_object_begin(bacnet_buf_t *apdu, BACNET_RPM_DATA *rpm_data)
{
    uint8_t *pdu;
    int len;

    if ((apdu == NULL) || (apdu->data == NULL) || (rpm_data == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return BACNET_STATUS_REJECT;
    }

    pdu = apdu->data + apdu->data_len;

    /* Tag 0: ObjectIdentifier */
    len = encode_context_object_id(pdu, 0, rpm_data->object_type, rpm_data->object_instance);

    /* Tag 1: opening tag of listOfResults */
    len += encode_opening_tag(&pdu[len], 1);

    apdu->data_len += len;

    return len;
}

static int rpm_ack_encode_apdu_object_end(bacnet_buf_t *apdu)
{
    uint8_t *pdu;
    int len;

    if ((apdu == NULL) || (apdu->data == NULL)) {
        APP_ERROR("%s: null apdu", __func__);
        return BACNET_STATUS_REJECT;
    }

    pdu = apdu->data + apdu->data_len;

    /* Tag 1: closing tag of listOfResults */
    len = encode_closing_tag(pdu, 1);

    apdu->data_len += len;

    return len;
}

static int rpm_ack_encode_apdu_object_property(bacnet_buf_t *apdu, BACNET_PROPERTY_ID object_property,
            uint32_t array_index)
{
    uint8_t *pdu;
    int len;

    if ((apdu == NULL) || (apdu->data == NULL)) {
        APP_ERROR("%s: null apdu\r\n", __func__);
        return BACNET_STATUS_REJECT;
    }

    pdu = apdu->data + apdu->data_len;

    /* Tag 2: propertyIdentifier */
    len = encode_context_enumerated(pdu, 2, object_property);

    /* Tag 3: optional propertyArrayIndex */
    if (array_index != BACNET_ARRAY_ALL) {
        len += encode_context_unsigned(&pdu[len], 3, array_index);
    }

    apdu->data_len += len;
    
    return len;
}

static int rpm_ack_encode_apdu_object_property_error(bacnet_buf_t *apdu, BACNET_ERROR_CLASS error_class,
            BACNET_ERROR_CODE error_code)
{
    uint8_t *pdu;
    int len;

    if ((apdu == NULL) || (apdu->data == NULL)) {
        APP_ERROR("%s: null apdu\r\n", __func__);
        return BACNET_STATUS_REJECT;
    }

    pdu = apdu->data + apdu->data_len;
    
    /* Tag 5: propertyAccessError */
    len = encode_opening_tag(pdu, 5);
    len += encode_application_enumerated(&pdu[len], error_class);
    len += encode_application_enumerated(&pdu[len], error_code);
    len += encode_closing_tag(&pdu[len], 5);

    apdu->data_len += len;

    return len;
}

static BACNET_PROPERTY_ID RPM_Object_Property(special_property_list_t *pPropertyList, 
        BACNET_PROPERTY_ID special_property, uint16_t index)
{
    int property;
    uint16_t required, optional, proprietary;

    property = -1;
    required = pPropertyList->Required.count;
    optional = pPropertyList->Optional.count;
    proprietary = pPropertyList->Proprietary.count;
    
    if (special_property == PROP_ALL) {
        if (index < required) {
            property = pPropertyList->Required.pList[index];
        } else if (index < (required + optional)) {
            index -= required;
            property = pPropertyList->Optional.pList[index];
        } else if (index < (required + optional + proprietary)) {
            index -= (required + optional);
            property = pPropertyList->Proprietary.pList[index];
        }
    } else if (special_property == PROP_REQUIRED) {
        if (index < required) {
            property = pPropertyList->Required.pList[index];
        }
    } else if (special_property == PROP_OPTIONAL) {
        if (index < optional) {
            property = pPropertyList->Optional.pList[index];
        }
    }

    return (BACNET_PROPERTY_ID)property;
}

static uint16_t RPM_Object_Property_Count(special_property_list_t *pPropertyList, 
                    BACNET_PROPERTY_ID special_property)
{
    uint16_t count;

    count = 0;
    
    if (special_property == PROP_ALL) {
        count = pPropertyList->Required.count + pPropertyList->Optional.count + 
            pPropertyList->Proprietary.count;
    } else if (special_property == PROP_REQUIRED) {
        count = pPropertyList->Required.count;
    } else if (special_property == PROP_OPTIONAL) {
        count = pPropertyList->Optional.count;
    }

    return count;
}

static int RPM_Encode_Property(bacnet_buf_t *apdu, BACNET_RPM_DATA *rpm_data)
{
    BACNET_READ_PROPERTY_DATA rpdata;
    uint32_t save_len;
    int len;

    len = rpm_ack_encode_apdu_object_property(apdu, rpm_data->object_property, rpm_data->array_index);
    if (len < 0) {
        APP_ERROR("%s: encode property failed(%d)\r\n", __func__, len);
        return len;
    }

    rpdata.object_type = rpm_data->object_type;
    rpdata.object_instance = rpm_data->object_instance;
    rpdata.property_id = rpm_data->object_property;
    rpdata.array_index = rpm_data->array_index;

    save_len = apdu->data_len;
    apdu->data_len += encode_opening_tag(apdu->data + apdu->data_len, 4);
    rpdata.application_data = apdu->data + apdu->data_len;
    rpdata.application_data_len = apdu->end - apdu->data - apdu->data_len;

    len = object_read_property(&rpdata, NULL);
    if (len < 0) {
        if ((len == BACNET_STATUS_ABORT) || (len == BACNET_STATUS_REJECT)) {
            rpm_data->reject_reason = rpdata.reject_reason;
            return len;
        }

        apdu->data_len = save_len;
        /* error was returned - encode that for the response */
        len = rpm_ack_encode_apdu_object_property_error(apdu, rpdata.error_class, rpdata.error_code);
        if (len < 0) {
            APP_ERROR("%s: encode property error failed(%d)\r\n", __func__, len);
            return len;
        }
    } else {
        apdu->data_len += len;
        apdu->data_len += encode_closing_tag(apdu->data + apdu->data_len, 4);
    }

    return BACNET_STATUS_OK;
}

void rpm_ack_print_data(uint8_t *service_data, uint32_t service_data_len)
{
    BACNET_RPM_ACK_DECODER decoder;
    BACNET_READ_PROPERTY_DATA rp_data;
    int rv;

    if (service_data == NULL) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return;
    }

    rpm_ack_decode_init(&decoder, service_data, service_data_len);

    for (;;) {
        rv = rpm_ack_decode_object(&decoder, &rp_data);
        if (rv < 0) {
            APP_ERROR("%s: decode object id failed\r\n", __func__);
            return;
        } else if (rv == 0)
            break;

        fprintf(stdout, "%s #%lu\r\n", bactext_object_type_name(rp_data.object_type),
            (unsigned long)rp_data.object_instance);
        fprintf(stdout, "{\r\n");

        for (;;) {
            rv = rpm_ack_decode_property(&decoder, &rp_data);
            if (rv < 0) {
                APP_ERROR("%s: decode property failed\r\n", __func__);
                return;
            } else if (rv == 0)
                break;

            if (rp_data.property_id < 512) {
                fprintf(stdout, "    %s", bactext_property_name(rp_data.property_id));
            } else {
                fprintf(stdout, "    proprietary %u", (unsigned)rp_data.property_id);
            }

            if (rp_data.array_index != BACNET_ARRAY_ALL) {
                fprintf(stdout, "[%d]", rp_data.array_index);
            }
            fprintf(stdout, ": ");

            if (rp_data.application_data == NULL) {
                fprintf(stdout, "BACnet Error: %s: %s\r\n",
                bactext_error_class_name((int)rp_data.error_class),
                bactext_error_code_name((int)rp_data.error_code));
                continue;
            }

            bacapp_fprint_value(stdout, rp_data.application_data, rp_data.application_data_len);
            fprintf(stdout, "\r\n");
        }
        
        fprintf(stdout, "}\r\n");
    }
}

void handler_read_property_multiple(BACNET_CONFIRMED_SERVICE_DATA *service_data, 
        bacnet_buf_t *reply_apdu, bacnet_addr_t *src)
{
    BACNET_RPM_DATA rpm_data;
    uint8_t *service_request;
    uint16_t request_len;
    uint16_t decode_len;
    int len;
    
    len = rpm_ack_encode_apdu_init(reply_apdu, service_data->invoke_id);
    if (len < 0) {
        APP_ERROR("%s: encode ack apdu init failed(%d)\r\n", __func__, len);
        rpm_data.reject_reason = REJECT_REASON_OTHER;
        goto failed;
    }

    decode_len = 0;
    service_request = service_data->service_request;
    request_len = service_data->service_request_len;
    
    for (;;) {
        len = rpm_decode_object_id(&service_request[decode_len], request_len - decode_len, 
            &rpm_data);
        if (len < 0) {
            APP_ERROR("%s: decode object id failed(%d)\r\n", __func__, len);
            goto failed;
        }
        decode_len += len;

        /* Test for case of indefinite Device object instance */
        if ((rpm_data.object_type == OBJECT_DEVICE) &&
            (rpm_data.object_instance == BACNET_MAX_INSTANCE)) {
            rpm_data.object_instance = device_object_instance_number();
        }

        len = rpm_ack_encode_apdu_object_begin(reply_apdu, &rpm_data);
        if (len < 0) {
            APP_ERROR("%s: encode object begin failed(%d)\r\n", __func__, len);
            rpm_data.reject_reason = REJECT_REASON_OTHER;
            goto failed;
        }

        if (reply_apdu->data + reply_apdu->data_len > reply_apdu->end) {
            APP_ERROR("%s: response too big\r\n", __func__);
            rpm_data.abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
            len = BACNET_STATUS_ABORT;
            goto failed;
        }

        /* do each property of this object of the RPM request */
        for (;;) {
            len = rpm_decode_object_property(&(service_request[decode_len]), request_len, &rpm_data);
            if (len < 0) {
                APP_ERROR("%s: decode property failed(%d)\r\n", __func__, len);
                goto failed;
            }

            decode_len += len;

            /* handle the special properties */
            if ((rpm_data.object_property == PROP_ALL) 
                    || (rpm_data.object_property == PROP_REQUIRED) 
                    || (rpm_data.object_property == PROP_OPTIONAL)) {
                special_property_list_t property_list;
                uint16_t property_count;
                uint16_t index;
                BACNET_PROPERTY_ID special_object_property;

                if (rpm_data.array_index != BACNET_ARRAY_ALL) {
                    /*  No array index options for this special property.
                        Encode error for this object property response */
                    APP_ERROR("%s: No array index options for Property(%d)\r\n", __func__, 
                        rpm_data.object_property);
                    len = rpm_ack_encode_apdu_object_property(reply_apdu, rpm_data.object_property,
                        rpm_data.array_index);
                    if (len < 0) {
                        APP_ERROR("%s: encode property failed(%d)\r\n", __func__, 
                            len);
                        rpm_data.reject_reason = REJECT_REASON_OTHER;
                        goto failed;
                    }

                    len = rpm_ack_encode_apdu_object_property_error(reply_apdu, ERROR_CLASS_PROPERTY,
                        ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY);
                    if (len < 0) {
                        APP_ERROR("%s: encode property error failed(%d)\r\n", __func__, 
                            len);
                        rpm_data.reject_reason = REJECT_REASON_OTHER;
                        goto failed;
                    }
                } else {
                    if (!object_property_lists(rpm_data.object_type, rpm_data.object_instance, &property_list)) {
                        APP_ERROR("%s: get property list failed\r\n", __func__);
                        goto failed;
                    }
                    special_object_property = rpm_data.object_property;
                    property_count = RPM_Object_Property_Count(&property_list, 
                        special_object_property);
                    if (property_count == 0) {
                        /* handle the error code - but use the special property */
                        APP_WARN("%s: no property for special object(%d) property(%d)\r\n", __func__, 
                            rpm_data.object_type, special_object_property);
                        len = RPM_Encode_Property(reply_apdu, &rpm_data);
                        if (len < 0) {
                            APP_ERROR("%s: encode property failed(%d)\r\n", __func__, 
                                len);
                            goto failed;
                        }
                    } else {
                        for (index = 0; index < property_count; index++) {
                            rpm_data.object_property = RPM_Object_Property(&property_list, 
                                special_object_property, index);
                            len = RPM_Encode_Property(reply_apdu, &rpm_data);
                            if (len < 0) {
                                APP_ERROR("%s: encode property failed(%d)\r\n", __func__, 
                                    len);
                                goto failed;
                            }

                            if (reply_apdu->data + reply_apdu->data_len > reply_apdu->end) {
                                APP_ERROR("%s: response too big\r\n", __func__);
                                rpm_data.abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
                                len = BACNET_STATUS_ABORT;
                                goto failed;
                            }
                        }
                    }
                }
            } else {
                /* handle an individual property */
                len = RPM_Encode_Property(reply_apdu, &rpm_data);
                if (len < 0) {
                    APP_ERROR("%s: encode property failed(%d)\r\n", __func__, len);
                    goto failed;
                }
            }

            if (reply_apdu->data + reply_apdu->data_len > reply_apdu->end) {
                APP_ERROR("%s: response too big\r\n", __func__);
                rpm_data.abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
                len = BACNET_STATUS_ABORT;
                goto failed;
            }

            /* Reached end of property list so cap the result list */
            if (decode_closing_tag(&service_request[decode_len], 1) >= 0) {
                decode_len++;
                len = rpm_ack_encode_apdu_object_end(reply_apdu);
                if (len < 0) {
                    APP_ERROR("%s: encode object end failed(%d)\r\n", __func__, len);
                    goto failed;
                }

                if (reply_apdu->data + reply_apdu->data_len > reply_apdu->end) {
                    APP_ERROR("%s: response too big\r\n", __func__);
                    rpm_data.abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
                    len = BACNET_STATUS_ABORT;
                    goto failed;
                }

                break;      /* finished with this property list */
            }
        }

        /* Reached the end so finish up */
        if (decode_len >= request_len) {
            break;
        }
    }

    if (reply_apdu->data + reply_apdu->data_len > reply_apdu->end) {
        rpm_data.abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
        len = BACNET_STATUS_ABORT;
        goto failed;
    }

    return;
    
failed:
    reply_apdu->data_len = 0;
    
    if (len == BACNET_STATUS_ERROR) {
        len = bacerror_encode_apdu(reply_apdu, service_data->invoke_id,
            SERVICE_CONFIRMED_READ_PROP_MULTIPLE, rpm_data.error_class, rpm_data.error_code);
    } else if (len == BACNET_STATUS_ABORT) {
        len = abort_encode_apdu(reply_apdu, service_data->invoke_id,
            rpm_data.abort_reason, true);
    } else if (len == BACNET_STATUS_REJECT) {
        len = reject_encode_apdu(reply_apdu, service_data->invoke_id,
            rpm_data.reject_reason);
    } else {
        APP_ERROR("%s: unknown error(%d)\r\n", __func__, len);
        len = BACNET_STATUS_ERROR;
    }

    if (len < 0) {
        reply_apdu->data_len = 0;
    }
    
    return;
}

enum {
    _RPM_ACK_DECODE_OBJ = 0,
    _RPM_ACK_DECODE_OBJ_NXT,
    _RPM_ACK_DECODE_PRO,
    _RPM_ACK_DECODE_PRO_NXT,
};

/**
 * init decoder
 * @return <0 if failed
 */
int rpm_ack_decode_init(BACNET_RPM_ACK_DECODER *decoder,
        uint8_t *service_data, uint32_t service_data_len)
{
    if (!decoder || !service_data) {
        APP_ERROR("%s: null arguments\r\n", __func__);
        return BACNET_STATUS_REJECT;
    }

    decoder->data = service_data;
    decoder->data_len = service_data_len;
    decoder->state = _RPM_ACK_DECODE_OBJ;

    return OK;
}

/**
 * @return >0 has new objct, =0 no more object, <0 error
 */
int rpm_ack_decode_object(BACNET_RPM_ACK_DECODER *decoder,
        BACNET_READ_PROPERTY_DATA *rp_data)
{
    int len, dec_len;
    uint8_t *buf;

    if (!decoder || !rp_data) {
        APP_ERROR("%s: null arguments\r\n", __func__);
        return BACNET_STATUS_REJECT;
    }

    buf = decoder->data;

    if (!buf) {
        APP_ERROR("%s: decoder not initialized\r\n", __func__);
        return BACNET_STATUS_REJECT;
    }

next_decode:
    switch (decoder->state) {
    case _RPM_ACK_DECODE_OBJ:
        if (decoder->data_len == 0)
            return BACNET_STATUS_REJECT;

        decoder->state = _RPM_ACK_DECODE_OBJ_NXT;
        goto decode_obj;

    case _RPM_ACK_DECODE_OBJ_NXT:
        if (decoder->data_len == 0)
            return 0;

decode_obj:
        len = decode_context_object_id(buf, 0,
                &rp_data->object_type, &rp_data->object_instance);
        if (len < 0)
            return len;

        dec_len = decode_opening_tag(buf + len, 1);
        if (dec_len < 0)
            return dec_len;

        len += dec_len;
        if (len >= decoder->data_len)
            return BACNET_STATUS_REJECT;

        decoder->data += len;
        decoder->data_len -= len;

        decoder->state = _RPM_ACK_DECODE_PRO;
        return 1;

    case _RPM_ACK_DECODE_PRO:
    case _RPM_ACK_DECODE_PRO_NXT:
        if (rpm_ack_decode_property(decoder, rp_data) < 0)
            return BACNET_STATUS_REJECT;

        goto next_decode;
    default:
        return BACNET_STATUS_REJECT;
    }
}

/**
 * @return >0 has new property, =0 no more property, <0 error
 */
int rpm_ack_decode_property(BACNET_RPM_ACK_DECODER *decoder,
        BACNET_READ_PROPERTY_DATA *rp_data)
{
    int len, dec_len;
    uint8_t *buf;

    if (!decoder || !rp_data) {
        APP_ERROR("%s: null arguments\r\n", __func__);
        return BACNET_STATUS_REJECT;
    }

    buf = decoder->data;

    if (!buf) {
        APP_ERROR("%s: decoder not initialized\r\n", __func__);
        return BACNET_STATUS_REJECT;
    }

    switch (decoder->state) {
    case _RPM_ACK_DECODE_OBJ:
    case _RPM_ACK_DECODE_OBJ_NXT:
        return 0;

    case _RPM_ACK_DECODE_PRO:
        len = decode_closing_tag(buf, 1);
        if (len >= 0)
            return BACNET_STATUS_REJECT;

        decoder->state = _RPM_ACK_DECODE_PRO_NXT;
        goto decode_property;

    case _RPM_ACK_DECODE_PRO_NXT:
        len = decode_closing_tag(buf, 1);
        if (len >= 0) {
            decoder->data += len;
            decoder->data_len -= len;
            decoder->state = _RPM_ACK_DECODE_OBJ_NXT;
            return 0;
        }

decode_property:
        len = decode_context_enumerated(buf, 2, &rp_data->property_id);
        if (len < 0)
            return len;

        if (decode_has_context_tag(buf + len, 3)) {
            dec_len = decode_context_unsigned(buf + len,
                    3, &rp_data->array_index);
            if (dec_len < 0)
                return dec_len;

            if (rp_data->array_index == BACNET_ARRAY_ALL)
                return BACNET_STATUS_REJECT;

            len += dec_len;
        } else
            rp_data->array_index = BACNET_ARRAY_ALL;

        if (decode_opening_tag(&buf[len], 5) >= 0) {
            len++;
            rp_data->application_data = NULL;
            rp_data->application_data_len = 0;

            dec_len = decode_application_enumerated(&buf[len], &rp_data->error_class);
            if (dec_len < 0)
                return dec_len;
            len += dec_len;

            dec_len = decode_application_enumerated(&buf[len], &rp_data->error_code);
            if (dec_len < 0)
                return dec_len;
            len += dec_len;

            dec_len = decode_closing_tag(&buf[len], 5);
            if (dec_len < 0)
                return dec_len;

            len += dec_len;
        } else {
            if (len >= decoder->data_len)
                return BACNET_STATUS_REJECT;

            dec_len = decode_constructed_tag(&buf[len],
                    decoder->data_len - len, 4);
            if (dec_len < 0)
                return dec_len;

            rp_data->application_data = &buf[len] + CONTEXT_TAG_LENGTH(4);
            rp_data->application_data_len = dec_len - CONTEXT_TAG_LENGTH(4) * 2;
            len += dec_len;
        }
        if (len >= decoder->data_len)
            return BACNET_STATUS_REJECT;

        decoder->data += len;
        decoder->data_len -= len;
        return 1;

    default:
        return BACNET_STATUS_REJECT;
    }
}

/*
 * @return: true if success, false = fail
 */
bool rpm_req_encode_object(bacnet_buf_t *pdu, BACNET_OBJECT_TYPE object_type,
            uint32_t instance)
{
    int len;
    uint8_t *buf = pdu->data + pdu->data_len;

    if (pdu->data_len) {
        len = encode_closing_tag(buf, 1);
    } else {
        buf[0] = PDU_TYPE_CONFIRMED_SERVICE_REQUEST;
        buf[1] = encode_max_segs_max_apdu(0, MAX_APDU);
        buf[3] = SERVICE_CONFIRMED_READ_PROP_MULTIPLE;
        len = 4;
    }

    len += encode_context_object_id(&buf[len], 0, object_type, instance);
    len += encode_opening_tag(&buf[len], 1);

    pdu->data_len += len;
    pdu->data[2] = false;
    return buf + len < pdu->end;
}

bool rpm_req_encode_property(bacnet_buf_t *pdu,
            BACNET_PROPERTY_ID property_id, uint32_t array_index)
{
    uint8_t *buf = pdu->data + pdu->data_len;
    int len = encode_context_enumerated(buf, 0, property_id);
    if (array_index != BACNET_ARRAY_ALL) {
        len += encode_context_unsigned(&buf[len], 1, array_index);
    }

    pdu->data_len += len;
    pdu->data[2] = true;
    return buf + len < pdu->end;
}

bool rpm_req_encode_end(bacnet_buf_t *pdu, uint8_t invoke_id)
{
    uint8_t *buf = pdu->data + pdu->data_len;
    int len;

    if (!pdu->data_len || !pdu->data[2]) {
        APP_ERROR("%s: no object or no property in object\r\n", __func__);
        return false;
    }

    len = encode_closing_tag(buf, 1);
    pdu->data_len += len;

    pdu->data[2] = invoke_id;
    return buf + len <= pdu->end;
}
