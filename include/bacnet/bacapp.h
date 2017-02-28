/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bacapp.h
 * Original Author:  linzhixian, 2015-1-23
 *
 *
 * History
 */

#ifndef _BACAPP_H_
#define _BACAPP_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "bacnet/bacenum.h"
#include "bacnet/bacstr.h"
#include "bacnet/datetime.h"
#include "bacnet/bacnet_buf.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define BACAPP_NULL                     (0)
#define BACAPP_BOOLEAN                  (1)
#define BACAPP_UNSIGNED                 (2)
#define BACAPP_SIGNED                   (3)
#define BACAPP_REAL                     (4)
#define BACAPP_DOUBLE                   (5)
#define BACAPP_OCTET_STRING             (6)
#define BACAPP_CHARACTER_STRING         (7)
#define BACAPP_BIT_STRING               (8)
#define BACAPP_ENUMERATED               (9)
#define BACAPP_DATE                     (10)
#define BACAPP_TIME                     (11)
#define BACAPP_OBJECT_ID                (12)

typedef struct BACnet_Access_Error {
    BACNET_ERROR_CLASS error_class;
    BACNET_ERROR_CODE error_code;
} BACNET_ACCESS_ERROR;

typedef struct BACnet_Object_Id {
    BACNET_OBJECT_TYPE type;
    uint32_t instance;
} BACNET_OBJECT_ID;

typedef struct BACnet_Application_Data_Value {
    uint8_t tag;                    /* application tag data type */
    union {
        /* NULL - not needed as it is encoded in the tag alone */
        bool Boolean;
        uint32_t Unsigned_Int;
        int32_t Signed_Int;
        float Real;
        double Double;
        BACNET_OCTET_STRING Octet_String;
        BACNET_CHARACTER_STRING Character_String;
        BACNET_BIT_STRING Bit_String;
        uint32_t Enumerated;
        BACNET_DATE Date;
        BACNET_TIME Time;
        BACNET_OBJECT_ID Object_Id;
    } type;
} BACNET_APPLICATION_DATA_VALUE;

typedef struct BACnet_Property_Value {
    BACNET_PROPERTY_ID propertyIdentifier;
    uint32_t propertyArrayIndex;
    uint8_t *value_data;
    uint32_t value_data_len;
    uint8_t priority;
} BACNET_PROPERTY_VALUE;

typedef struct BACnet_Property_Reference {
    BACNET_PROPERTY_ID propertyIdentifier;
    uint32_t propertyArrayIndex;                    /* optional */
} BACNET_PROPERTY_REFERENCE;

typedef struct Bacnet_Device_Object_Property {
    uint32_t device_id;
    BACNET_OBJECT_ID object_id;
    BACNET_PROPERTY_ID property_id;
    uint32_t array_index;
} BACNET_DEVICE_OBJECT_PROPERTY;

extern int bacapp_encode_application_data(uint8_t *pdu, BACNET_APPLICATION_DATA_VALUE *value);

extern int bacapp_encode_context_data(uint8_t *pdu, uint8_t context_tag_number,
            BACNET_APPLICATION_DATA_VALUE *value);

extern int bacapp_decode_application_data(uint8_t *pdu, uint16_t max_pdu_len,
            BACNET_APPLICATION_DATA_VALUE *value);

extern void bacapp_fprint_value(FILE *stream, uint8_t *buf, uint32_t size);

extern bool bacapp_parse_application_data(bacnet_buf_t *pdu, char *argv);

extern bool bacapp_snprint_value(char *str, size_t str_len, uint8_t *buf, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif  /* _BACAPP_H_ */

