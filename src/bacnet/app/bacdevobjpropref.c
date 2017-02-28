/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bacdevobjpropref.c
 * Original Author:  linzhixian, 2015-5-25
 *
 * Bacnet Device Object Property Reference
 *
 * History
 */

#include <errno.h>

#include "bacnet/bacdevobjpropref.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacdef.h"
#include "bacnet/app.h"

int bacapp_encode_device_obj_property_ref(uint8_t *pdu, 
        BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE *value)
{
    int len;

    if ((pdu == NULL) || (value == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }
    
    len = encode_context_object_id(pdu, 0, (int)value->objectIdentifier.type, 
        value->objectIdentifier.instance);

    len += encode_context_enumerated(&pdu[len], 1, value->propertyIdentifier);

    /* Array index is optional so check if needed before inserting */
    if (value->arrayIndex != BACNET_ARRAY_ALL) {
        len += encode_context_unsigned(&pdu[len], 2, value->arrayIndex);
    }

    /* Likewise, device id is optional so see if needed(set type to non device to omit */
    if (value->deviceIndentifier.type == OBJECT_DEVICE) {
        len += encode_context_object_id(&pdu[len], 3, (int)value->deviceIndentifier.type,
            value->deviceIndentifier.instance);
    }
    
    return len;
}

int bacapp_decode_device_obj_property_ref(uint8_t *pdu,
            BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE *value)
{
    int len, dec_len;

    if ((pdu == NULL) || (value == NULL)) {
        APP_WARN("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    len = decode_context_object_id(pdu, 0, &value->deviceIndentifier.type, &value->deviceIndentifier.instance);
    if (len < 0) return len;

    dec_len = decode_context_enumerated(&pdu[len], 1, &value->propertyIdentifier);
    if (dec_len < 0) return dec_len;
    len += dec_len;

    if (decode_has_context_tag(&pdu[len], 2)) {
        dec_len = decode_context_unsigned(&pdu[len], 2, &value->arrayIndex);
        if (dec_len < 0) return dec_len;
        len += dec_len;
    } else
        value->arrayIndex = BACNET_ARRAY_ALL;

    if (decode_has_context_tag(&pdu[len], 3)) {
        dec_len = decode_context_object_id(pdu, 3, &value->deviceIndentifier.type, &value->deviceIndentifier.instance);
        if (dec_len < 0) return dec_len;
        if (value->deviceIndentifier.type != OBJECT_DEVICE)
            return BACNET_STATUS_ERROR;
        len += dec_len;
    } else
        value->deviceIndentifier.type = MAX_BACNET_OBJECT_TYPE;

    return len;
}

bool bacapp_device_obj_property_ref_same(BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE *src,
        BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE *dst)
{
    if ((src == NULL) || (dst == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return false;
    }
    
    if ((src->deviceIndentifier.type == dst->deviceIndentifier.type)
            && (src->deviceIndentifier.instance == dst->deviceIndentifier.instance)
            && (src->objectIdentifier.type == dst->objectIdentifier.type)
            && (src->objectIdentifier.instance == dst->objectIdentifier.instance)
            && (src->propertyIdentifier == dst->propertyIdentifier)
            && (src->arrayIndex == dst->arrayIndex)) {
        return true;
    }

    return false;
}

