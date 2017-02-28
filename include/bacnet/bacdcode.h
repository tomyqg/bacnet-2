/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bacdcode.h
 * Original Author:  linzhixian, 2015-1-12
 *
 *
 * History
 */

#ifndef _BACDCODE_H_
#define _BACDCODE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include "bacnet/bacdef.h"
#include "bacnet/datetime.h"
#include "bacnet/bacstr.h"
#include "bacnet/bacint.h"
#include "bacnet/bacreal.h"
#include "bacnet/config.h"
#include "misc/bits.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define MAX_VALUE_LENGTH    (0x7FFFFFFFU - 2 * MAX_APDU)

/* from clause 20.2.1.2 Tag Number */
/* true if tag number == 15 and context tag */
#define IS_EXTENDED_TAG_NUMBER(x) ((x & 0xF8) == 0xF8)

/* from clause 20.2.1.3.1 Primitive Data */
/* true if the extended value is used */
#define IS_EXTENDED_VALUE(x) ((x & 0x07) == 5)

/* from clause 20.2.1.1 Class */
/* true if the tag is context specific */
#define IS_CONTEXT_SPECIFIC(x) ((x & BIT3) == BIT3)

/* from clause 20.2.1.3.2 Constructed Data */
/* true if the tag is an opening tag and context specific */
#define IS_OPENING_TAG(x) ((x & 0x0F) == 0x0E)

/* from clause 20.2.1.3.2 Constructed Data */
/* true if the tag is a closing tag and context specific */
#define IS_CLOSING_TAG(x) ((x & 0x0F) == 0x0F)

/* from clause 20.2.1.3.2 Constructed Data */
/* true if the tag is a opening or closing tag and context specific */
#define IS_OPENING_CLOSING_TAG(x) ((x & 0x0F) >= 0x0E)

/* if not application tag or opening closing len return MAX_BACNET_APPLICATION_TAG */
#define VALID_APPLICATION_TAG_NUMBER(x) ((x & 0x0F) > 5 ? MAX_BACNET_APPLICATION_TAG : x >> 4)

/* return bytes consumed by a context tag */
#define CONTEXT_TAG_LENGTH(x) ((x > 14) ? 2 : 1)

static inline uint8_t get_tag_number(const uint8_t *apdu)
{
    if (IS_EXTENDED_TAG_NUMBER(apdu[0]))
        return apdu[1];
    else
        return apdu[0] >> 4;
}

int decode_tag_number_and_value(
    const uint8_t * apdu,
    uint8_t * tag_number,
    uint32_t * value);

/* from clause 20.2.1.3.2 Constructed Data */
/* returns the number of apdu bytes consumed */
    int encode_opening_tag(
        uint8_t * apdu,
        uint8_t tag_number);

    int encode_closing_tag(
        uint8_t * apdu,
        uint8_t tag_number);

    int decode_opening_tag(
        const uint8_t * apdu,
        uint8_t tag_number);

    int decode_closing_tag(
        const uint8_t * apdu,
        uint8_t tag_number);

/* return true if there is a context tag of tag_number and not closing tag */
    bool decode_has_context_tag(
        const uint8_t *apdu,
        uint8_t tag_number);

/* from clause 20.2.2 Encoding of a Null Value */
    int encode_application_null(
        uint8_t * apdu);
    int encode_context_null(
        uint8_t * apdu,
        uint8_t tag_number);
    int decode_context_null(
        const uint8_t * apdu,
        uint8_t tag_number);
    int decode_application_null(
        const uint8_t * apdu);

/* from clause 20.2.3 Encoding of a Boolean Value */
    int encode_application_boolean(
        uint8_t * apdu,
        bool boolean_value);
    int encode_context_boolean(
        uint8_t * apdu,
        uint8_t tag_number,
        bool boolean_value);
/* returns the number of apdu bytes consumed or <0 if error */
    int decode_context_boolean(
        const uint8_t * apdu,
        uint8_t tag_number,
        bool * boolean_value);
    int decode_application_boolean(
        const uint8_t * apdu,
        bool * boolean_value);
/* from clause 20.2.10 Encoding of a Bit String Value */
/* after call, bit_string->value point to bit map buffer
 * returns the number of apdu bytes consumed or <0 if error */
    int decode_bitstring(
        uint8_t * apdu,
        uint32_t len_value,
        BACNET_BIT_STRING * bit_string);
    int decode_context_bitstring(
        uint8_t * apdu,
        uint8_t tag_number,
        BACNET_BIT_STRING * bit_string);
    int decode_application_bitstring(
        uint8_t * apdu,
        BACNET_BIT_STRING * bit_string);
/* after call, bit_string->value point to buffer to put bit map
 * returns the number of apdu bytes consumed */
    int encode_bitstring(
        uint8_t * apdu,
        BACNET_BIT_STRING * bit_string);
    int encode_application_bitstring(
        uint8_t * apdu,
        BACNET_BIT_STRING * bit_string);
    int encode_context_bitstring(
        uint8_t * apdu,
        uint8_t tag_number,
        BACNET_BIT_STRING * bit_string);

/* from clause 20.2.6 Encoding of a Real Number Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
    int encode_application_real(
        uint8_t * apdu,
        float value);
    int encode_context_real(
        uint8_t * apdu,
        uint8_t tag_number,
        float value);
    int decode_context_real(
        const uint8_t * apdu,
        uint8_t tag_number,
        float *real_value);
    int decode_application_real(
        const uint8_t * apdu,
        float *real_value);

/* from clause 20.2.7 Encoding of a Double Precision Real Number Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
    int encode_application_double(
        uint8_t * apdu,
        double value);
    int encode_context_double(
        uint8_t * apdu,
        uint8_t tag_number,
        double value);
    int decode_context_double(
        const uint8_t * apdu,
        uint8_t tag_number,
        double *double_value);
    int decode_application_double(
        const uint8_t * apdu,
        double *double_value);
/* from clause 20.2.14 Encoding of an Object Identifier Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
    int decode_object_id(
        const uint8_t * apdu,
        BACNET_OBJECT_TYPE * object_type,
        uint32_t * instance);
    int decode_context_object_id(
        const uint8_t * apdu,
        uint8_t tag_number,
        BACNET_OBJECT_TYPE * object_type,
        uint32_t * instance);
    int decode_application_object_id(
        const uint8_t * apdu,
        BACNET_OBJECT_TYPE * object_type,
        uint32_t * instance);
    int encode_bacnet_object_id(
        uint8_t * apdu,
        BACNET_OBJECT_TYPE object_type,
        uint32_t instance);
    int encode_context_object_id(
        uint8_t * apdu,
        uint8_t tag_number,
        BACNET_OBJECT_TYPE object_type,
        uint32_t instance);
    int encode_application_object_id(
        uint8_t * apdu,
        BACNET_OBJECT_TYPE object_type,
        uint32_t instance);

/* from clause 20.2.8 Encoding of an Octet String Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* after call, octet_string->value point to buffer
 * returns the number of apdu bytes consumed */
    int encode_octet_string(
        uint8_t * apdu,
        BACNET_OCTET_STRING * octet_string);
    int encode_application_octet_string(
        uint8_t * apdu,
        BACNET_OCTET_STRING * octet_string);
    int encode_context_octet_string(
        uint8_t * apdu,
        uint8_t tag_number,
        BACNET_OCTET_STRING * octet_string);
    int encode_application_raw_octet_string(
        uint8_t *apdu,
        const uint8_t *value,
        size_t size);
    int encode_context_raw_octet_string(
        uint8_t *apdu,
        uint8_t tag_number,
        const uint8_t *value,
        size_t size);
/*
 * after call, octet_string->value point to buffer
 * @return the number of apdu bytes consumed */
    int decode_octet_string(
        uint8_t * apdu,
        uint32_t len_value,
        BACNET_OCTET_STRING * octet_string);
    int decode_context_octet_string(
        uint8_t * apdu,
        uint8_t tag_number,
        BACNET_OCTET_STRING * octet_string);
    int decode_application_octet_string(
        uint8_t * apdu,
        BACNET_OCTET_STRING * octet_string);
/*
 * after call, char_string->value point to buffer to put char string
 * @return the number of apdu bytes consumed
 */
    int encode_character_string(
        uint8_t * apdu,
        BACNET_CHARACTER_STRING * char_string);
    int encode_application_character_string(
        uint8_t * apdu,
        BACNET_CHARACTER_STRING * char_string);
    int encode_context_character_string(
        uint8_t * apdu,
        uint8_t tag_number,
        BACNET_CHARACTER_STRING * char_string);
    int encode_application_ansi_character_string(
        uint8_t * apdu,
        const char *string,
        uint32_t size);
    int encode_context_ansi_character_string(
        uint8_t * apdu,
        uint8_t tag_number,
        const char *string,
        uint32_t size);
/*
 * after call, char_string->value point to buffer
 * @return the number of apdu bytes consumed
 */
    int decode_character_string(
        uint8_t * apdu,
        uint32_t len_value,
        BACNET_CHARACTER_STRING * char_string);
    int decode_context_character_string(
        uint8_t * apdu,
        uint8_t tag_number,
        BACNET_CHARACTER_STRING * char_string);
    int decode_application_character_string(
        uint8_t * apdu,
        BACNET_CHARACTER_STRING * char_string);


/* from clause 20.2.4 Encoding of an Unsigned Integer Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
    int encode_unsigned(
        uint8_t * apdu,
        uint32_t value);
    int encode_context_unsigned(
        uint8_t * apdu,
        uint8_t tag_number,
        uint32_t value);
    int encode_application_unsigned(
        uint8_t * apdu,
        uint32_t value);
/* return true if success or false if error */
    int decode_unsigned(
        const uint8_t * apdu,
        uint32_t len_value,
        uint32_t * value);
    int decode_context_unsigned(
        const uint8_t * apdu,
        uint8_t tag_number,
        uint32_t * value);
    int decode_application_unsigned(
        const uint8_t * apdu,
        uint32_t * value);

/* from clause 20.2.5 Encoding of a Signed Integer Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
    int encode_signed(
        uint8_t * apdu,
        int32_t value);
    int encode_application_signed(
        uint8_t * apdu,
        int32_t value);
    int encode_context_signed(
        uint8_t * apdu,
        uint8_t tag_number,
        int32_t value);
/* return true if success or false if error */
    int decode_signed(
        const uint8_t * apdu,
        uint32_t len_value,
        int32_t * value);
    int decode_context_signed(
        const uint8_t * apdu,
        uint8_t tag_number,
        int32_t * value);
    int decode_application_signed(
        const uint8_t * apdu,
        int32_t * value);

/* from clause 20.2.11 Encoding of an Enumerated Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
#define decode_enumerated               decode_unsigned
#define decode_context_enumerated       decode_context_unsigned
#define encode_enumerated               encode_unsigned
#define encode_context_enumerated       encode_context_unsigned

    int encode_application_enumerated(
        uint8_t * apdu,
        uint32_t value);

    int decode_application_enumerated(
        const uint8_t * apdu,
        uint32_t * value);

/* from clause 20.2.13 Encoding of a Time Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
    int encode_bacnet_time(
        uint8_t * apdu,
        const BACNET_TIME * btime);
    int encode_application_time(
        uint8_t * apdu,
        const BACNET_TIME * btime);
    int decode_bacnet_time(
        const uint8_t * apdu,
        BACNET_TIME * btime);
    int encode_context_time(
        uint8_t * apdu,
        uint8_t tag_number,
        const BACNET_TIME * btime);
    int decode_application_time(
        const uint8_t * apdu,
        BACNET_TIME * btime);
    int decode_context_bacnet_time(
        const uint8_t * apdu,
        uint8_t tag_number,
        BACNET_TIME * btime);


/* BACnet Date */
/* year = years since 1900 */
/* month 1=Jan */
/* day = day of month */
/* wday 1=Monday...7=Sunday */

/* from clause 20.2.12 Encoding of a Date Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
    int encode_bacnet_date(
        uint8_t * apdu,
        const BACNET_DATE * bdate);
    int encode_application_date(
        uint8_t * apdu,
        const BACNET_DATE * bdate);
    int encode_context_date(
        uint8_t * apdu,
        uint8_t tag_number,
        const BACNET_DATE * bdate);
    int decode_date(
        const uint8_t * apdu,
        BACNET_DATE * bdate);
    int decode_application_date(
        const uint8_t * apdu,
        BACNET_DATE * bdate);
    int decode_context_date(
        const uint8_t * apdu,
        uint8_t tag_number,
        BACNET_DATE * bdate);

/* from clause 20.1.2.4 max-segments-accepted */
/* and clause 20.1.2.5 max-APDU-length-accepted */
/* returns the encoded octet */
    uint8_t encode_max_segs_max_apdu(
        int max_segs,
        int max_apdu);
    int decode_max_segs(
        uint8_t octet);
    int decode_max_apdu(
        uint8_t octet);

/* returns the number of apdu bytes consumed */
    int encode_simple_ack(
        uint8_t * apdu,
        uint8_t invoke_id,
        uint8_t service_choice);

/* extracted inside data, return apdu bytes consumed or < 0 if error */
    int decode_constructed_tag(
        const uint8_t *apdu,
        uint32_t remaining_bytes,
        uint8_t tag_number);

#ifdef __cplusplus

}
#endif /* __cplusplus */
#endif
