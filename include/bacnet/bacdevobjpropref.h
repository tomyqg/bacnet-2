/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bacdevobjpropref.h
 * Original Author:  linzhixian, 2015-5-25
 *
 * Bacnet Device Object Property Reference
 *
 * History
 */

#ifndef _BACDEVOBJPROPREF_H_
#define _BACDEVOBJPROPREF_H_

#include <stdint.h>

#include "bacnet/bacapp.h"
#include "bacnet/bacenum.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
    BACNET_OBJECT_ID deviceIndentifier;
    BACNET_OBJECT_ID objectIdentifier;
    BACNET_PROPERTY_ID propertyIdentifier;
    uint32_t arrayIndex;
} BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE;

extern int bacapp_encode_device_obj_property_ref(uint8_t *pdu, 
            BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE *value);

extern int bacapp_decode_device_obj_property_ref(uint8_t *pdu,
            BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE *value);

extern bool bacapp_device_obj_property_ref_same(BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE *src,
                BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE *dst);

#ifdef __cplusplus
}
#endif

#endif  /* _BACDEVOBJPROPREF_H_ */

