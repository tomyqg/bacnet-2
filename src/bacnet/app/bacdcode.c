/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bacdcode.c
 * Original Author:  linzhixian, 2015-1-12
 *
 * Bacnet APDU encoding/decoding
 *
 * History
 */

#include <string.h>

#include "bacnet/bacdef.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacenum.h"
#include "bacnet/bacstr.h"
#include "bacnet/bacint.h"
#include "bacnet/bacreal.h"
#include "misc/bits.h"

/** @file bacdcode.c  Functions to encode/decode BACnet data types */


/* max-segments-accepted
   B'000'      Unspecified number of segments accepted.
   B'001'      2 segments accepted.
   B'010'      4 segments accepted.
   B'011'      8 segments accepted.
   B'100'      16 segments accepted.
   B'101'      32 segments accepted.
   B'110'      64 segments accepted.
   B'111'      Greater than 64 segments accepted.
*/

/* max-APDU-length-accepted
   B'0000'  Up to MinimumMessageSize (50 octets)
   B'0001'  Up to 128 octets
   B'0010'  Up to 206 octets (fits in a LonTalk frame)
   B'0011'  Up to 480 octets (fits in an ARCNET frame)
   B'0100'  Up to 1024 octets
   B'0101'  Up to 1476 octets (fits in an ISO 8802-3 frame)
   B'0110'  reserved by ASHRAE
   B'0111'  reserved by ASHRAE
   B'1000'  reserved by ASHRAE
   B'1001'  reserved by ASHRAE
   B'1010'  reserved by ASHRAE
   B'1011'  reserved by ASHRAE
   B'1100'  reserved by ASHRAE
   B'1101'  reserved by ASHRAE
   B'1110'  reserved by ASHRAE
   B'1111'  reserved by ASHRAE
*/

/* Encoding of BACNET Length/Value/Type tag
   From clause 20.2.1.3.1

   B'000'   interpreted as Value  = FALSE if application class == BOOLEAN
   B'001'   interpreted as Value  = TRUE  if application class == BOOLEAN

   B'000'   interpreted as Length = 0     if application class != BOOLEAN
   B'001'   interpreted as Length = 1
   B'010'   interpreted as Length = 2
   B'011'   interpreted as Length = 3
   B'100'   interpreted as Length = 4
   B'101'   interpreted as Length > 4
   B'110'   interpreted as Type   = Opening Tag
   B'111'   interpreted as Type   = Closing Tag
*/


/* from clause 20.1.2.4 max-segments-accepted */
/* and clause 20.1.2.5 max-APDU-length-accepted */
/* returns the encoded octet */
uint8_t encode_max_segs_max_apdu(
    int max_segs,
    int max_apdu)
{
    uint8_t octet = 0;

    if (max_segs < 2)
        octet = 0;
    else if (max_segs < 4)
        octet = 0x10;
    else if (max_segs < 8)
        octet = 0x20;
    else if (max_segs < 16)
        octet = 0x30;
    else if (max_segs < 32)
        octet = 0x40;
    else if (max_segs < 64)
        octet = 0x50;
    else if (max_segs == 64)
        octet = 0x60;
    else
        octet = 0x70;

    /* max_apdu must be 50 octets minimum */
    if (max_apdu <= 50)
        octet |= 0x00;
    else if (max_apdu <= 128)
        octet |= 0x01;
    /*fits in a LonTalk frame */
    else if (max_apdu <= 206)
        octet |= 0x02;
    /*fits in an ARCNET or MS/TP frame */
    else if (max_apdu <= 480)
        octet |= 0x03;
    else if (max_apdu <= 1024)
        octet |= 0x04;
    /* fits in an ISO 8802-3 frame */
    else if (max_apdu <= 1476)
        octet |= 0x05;

    return octet;
}

/* from clause 20.1.2.4 max-segments-accepted */
/* and clause 20.1.2.5 max-APDU-length-accepted */
/* returns the encoded octet */
int decode_max_segs(
    uint8_t octet)
{
    int max_segs = 0;

    switch (octet & 0xF0) {
        case 0:
            max_segs = 0;
            break;
        case 0x10:
            max_segs = 2;
            break;
        case 0x20:
            max_segs = 4;
            break;
        case 0x30:
            max_segs = 8;
            break;
        case 0x40:
            max_segs = 16;
            break;
        case 0x50:
            max_segs = 32;
            break;
        case 0x60:
            max_segs = 64;
            break;
        case 0x70:
            max_segs = 65;
            break;
        default:
            break;
    }

    return max_segs;
}

int decode_max_apdu(
    uint8_t octet)
{
    int max_apdu = 0;

    switch (octet & 0x0F) {
        case 0:
            max_apdu = 50;
            break;
        case 1:
            max_apdu = 128;
            break;
        case 2:
            max_apdu = 206;
            break;
        case 3:
            max_apdu = 480;
            break;
        case 4:
            max_apdu = 1024;
            break;
        case 5:
            max_apdu = 1476;
            break;
        default:
            break;
    }

    return max_apdu;
}

/* from clause 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
static int encode_tag(
    uint8_t * apdu,
    uint8_t tag_number,
    bool context_specific,
    uint32_t len_value_type)
{
    int len = 1;        /* return value */

    if (context_specific) {
        apdu[0] = BIT3;
        if (tag_number > 14) {
            apdu[1] = tag_number;
            len++;
            tag_number = 15;
        }
    } else {
        apdu[0] = 0;
    }
    
    apdu[0] |= (uint8_t)(tag_number << 4);

    /* NOTE: additional len byte(s) after extended tag byte */
    /* if larger than 4 */
    if (len_value_type <= 4) {
        apdu[0] |= (uint8_t)len_value_type;
    } else {
        apdu[0] |= 5;
        if (len_value_type <= 253) {
            apdu[len++] = (uint8_t) len_value_type;
        } else if (len_value_type <= 65535) {
            apdu[len++] = 254;
            len += encode_unsigned16(&apdu[len], (uint16_t) len_value_type);
        } else {
            apdu[len++] = 255;
            len += encode_unsigned32(&apdu[len], len_value_type);
        }
    }

    return len;
}

/* from clause 20.2.1.3.2 Constructed Data */
/* returns the number of apdu bytes consumed */
int encode_opening_tag(
    uint8_t * apdu,
    uint8_t tag_number)
{
    int len = 1;

    /* additional tag byte after this byte for extended tag byte */
    if (tag_number <= 14) {
        apdu[0] = BIT3 | 6 | (uint8_t)(tag_number << 4) ;
    } else {
        apdu[0] = 0xF0 | BIT3 | 6;
        apdu[1] = tag_number;
        len++;
    }

    return len;
}

/* from clause 20.2.1.3.2 Constructed Data */
/* returns the number of apdu bytes consumed */
int encode_closing_tag(
    uint8_t * apdu,
    uint8_t tag_number)
{
    int len = 1;

    /* additional tag byte after this byte for extended tag byte */
    if (tag_number <= 14) {
        apdu[0] = (uint8_t)(BIT3 | 7 | (tag_number << 4));
    } else {
        apdu[0] = 0xF0 | BIT3 | 7;
        apdu[1] = tag_number;
        len++;
    }

    return len;
}

int decode_opening_tag(
    const uint8_t * apdu,
    uint8_t tag_number)
{
  if (tag_number < 15) {
      if (apdu[0] != ((tag_number << 4) | 0x0E))
          return -1;
      return 1;
  }
  else {
      if (apdu[0] != 0xFE || apdu[1] != tag_number)
          return -1;
      return 2;
  }
}

/* from clause 20.2.1.3.2 Constructed Data */
/* returns the number of apdu bytes consumed */
int decode_closing_tag(
    const uint8_t * apdu,
    uint8_t tag_number)
{
    if (tag_number < 15) {
        if (apdu[0] != ((tag_number << 4) | 0x0F))
            return -1;
        return 1;
    } else {
        if (apdu[0] != 0xFF || apdu[1] != tag_number)
            return -1;
        return 2;
    }
}

static int decode_value_length(
    const uint8_t * apdu,
    uint8_t tag_byte,
    uint32_t *value)
{
    uint16_t value16;
    uint32_t value32;
    int len = 0;
    
    if (IS_EXTENDED_VALUE(tag_byte)) {
        len++;
        if (apdu[0] == 255) {               /* tagged as uint32_t */
            len += decode_unsigned32(&apdu[len], &value32);
            /* avoid consumed byte overflow */
            if (value32 > MAX_VALUE_LENGTH) {
                return -1;
            } else if (value32 < 65536) {
                return -1;
            }
            if (value) {
                *value = value32;
            }
        } else if (apdu[0] == 254) {        /* tagged as uint16_t */
            len += decode_unsigned16(&apdu[len], &value16);
            if (value16 < 254) {
                return -1;
            }
            if (value) {
                *value = value16;
            }
        } else if (apdu[0] < 5) {           /* no tag - must be uint8_t */
            return -1;
        } else {
            if (value) {
                *value = apdu[0];
            }
        }
    } else if (IS_OPENING_CLOSING_TAG(tag_byte)) {
        return -1;
    } else {
        if (value) {
            *value = tag_byte & 0x07;       /* small value */
        }
    }

    return len;
}

static int decode_tag_number(
    const uint8_t *apdu,
    uint8_t *tag_number)
{
    /* decode the tag number first */
    if (IS_EXTENDED_TAG_NUMBER(apdu[0])) {
        /* extended tag */
        if (apdu[1] < 15) {
            return -1;
        }

        if (tag_number) {
            *tag_number = apdu[1];
        }
        return 2;
    } else {
        if (tag_number) {
            *tag_number = (uint8_t) (apdu[0] >> 4);
        }
        return 1;
    }
}

/* from clause 20.2.1.3.2 Constructed Data */
/* returns the number of apdu bytes consumed */
int decode_tag_number_and_value(
    const uint8_t * apdu,
    uint8_t * tag_number,
    uint32_t * value)
{
    int len, value_tag_len;
    
    len = decode_tag_number(apdu, tag_number);
    if (len < 0) return len;

    value_tag_len = decode_value_length(&apdu[len], apdu[0], value);
    if (value_tag_len < 0) return value_tag_len;
    
    return len + value_tag_len;
}

/* from clause 20.2.1.3.2 Constructed Data */
/* returns the number of apdu bytes consumed or -1 if error */
static int decode_match_context_tag_and_value(
    const uint8_t * apdu,
    uint8_t tag_number,
    uint32_t * value)
{
    int tag_len, value_len;

    tag_len = 1;
    if (tag_number < 15){
        if ((apdu[0] & 0xF8) != ((tag_number << 4) + 0x08))
            return -1;
    } else {
        if ((apdu[0] & 0xF8) != 0xF8 || apdu[1] != tag_number)
            return -1;
        tag_len++;
    }
    if ((value_len = decode_value_length(&apdu[tag_len], apdu[0], value)) < 0)
        return -1;

    return tag_len + value_len;
}

static int decode_match_application_tag_and_value(
    const uint8_t * apdu,
    uint8_t tag_number,
    uint32_t * value)
{
    int value_len;

    if ((apdu[0] & 0xF8) != (tag_number << 4))
        return -1;

    if ((value_len = decode_value_length(&apdu[1], apdu[0], value)) < 0)
        return -1;

    return value_len + 1;
}

bool decode_has_context_tag(
    const uint8_t *apdu,
    uint8_t tag_number)
{
    if (tag_number < 15) {
        tag_number = (tag_number << 4) + 0x0f;
        return (apdu[0] | 0x07) == tag_number && apdu[0] != tag_number;
    } else {
        return (apdu[0] | 0x07) == 0xff && apdu[0] != 0xff
                && apdu[1] == tag_number;
    }
}

/* from clause 20.2.3 Encoding of a Boolean Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
int encode_application_boolean(
    uint8_t * apdu,
    bool boolean_value)
{
    int len = 0;
    uint32_t len_value = 0;

    if (boolean_value) {
        len_value = 1;
    }
    
    len = encode_tag(&apdu[0], BACNET_APPLICATION_TAG_BOOLEAN, false, len_value);

    return len;
}

/* context tagged is encoded differently */
int encode_context_boolean(
    uint8_t * apdu,
    uint8_t tag_number,
    bool boolean_value)
{
    int len = 0;        /* return value */

    len = encode_tag(&apdu[0], (uint8_t) tag_number, true, 1);
    apdu[len] = (bool) (boolean_value ? 1 : 0);
    len++;

    return len;
}

int decode_context_boolean(
    const uint8_t * apdu,
    uint8_t tag_number,
    bool * boolean_value)
{
    int len;
    uint32_t len_value;
    if ((len = decode_match_context_tag_and_value(apdu, tag_number, &len_value)) < 0
            || len_value != 1 || apdu[len] > 1)
        return -1;

    *boolean_value = apdu[len];
    return len + (int)len_value;
}

int decode_application_boolean(
    const uint8_t * apdu,
    bool * boolean_value)
{
    int len;
    uint32_t len_value;
    if ((len = decode_match_application_tag_and_value(apdu, BACNET_APPLICATION_TAG_BOOLEAN, &len_value)) < 0
            || len_value > 1)
        return -1;

    *boolean_value = len_value;
    return len;
}

/* from clause 20.2.2 Encoding of a Null Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
int encode_application_null(
    uint8_t * apdu)
{
    return encode_tag(&apdu[0], BACNET_APPLICATION_TAG_NULL, false, 0);
}

int encode_context_null(
    uint8_t * apdu,
    uint8_t tag_number)
{
    return encode_tag(&apdu[0], tag_number, true, 0);
}

int decode_context_null(
    const uint8_t * apdu,
    uint8_t tag_number)
{
    int len;
    uint32_t len_value;
    if ((len = decode_match_context_tag_and_value(apdu, tag_number, &len_value)) < 0
            || len_value != 0)
        return -1;

    return len;
}

int decode_application_null(
    const uint8_t * apdu)
{
    int len;
    uint32_t len_value;
    if ((len = decode_match_application_tag_and_value(apdu, BACNET_APPLICATION_TAG_NULL, &len_value)) < 0
            || len_value)
        return -1;

    return len;
}

/* from clause 20.2.10 Encoding of a Bit String Value */
/* returns true if success or error */
int decode_bitstring(
    uint8_t * apdu,
    uint32_t len_value,
    BACNET_BIT_STRING * bit_string)
{
    if (len_value == 0 || apdu[0] > 7 || (len_value == 1 && apdu[0]))
        return -1;

    bit_string->byte_len = len_value - 1;
    bit_string->last_byte_bits_unused = apdu[0];

    bit_string->value = &apdu[1];

    return len_value;
}

int decode_context_bitstring(
    uint8_t * apdu,
    uint8_t tag_number,
    BACNET_BIT_STRING * bit_string)
{
    uint32_t len_value;
    int len;

    if ((len = decode_match_context_tag_and_value(apdu, tag_number, &len_value)) < 0
            || !decode_bitstring(&apdu[len], len_value, bit_string))
        return -1;

    len += (int)len_value;
    return len;
}

int decode_application_bitstring(
    uint8_t * apdu,
    BACNET_BIT_STRING * bit_string)
{
    uint32_t len_value;
    int len;

    if ((len = decode_match_application_tag_and_value(apdu, BACNET_APPLICATION_TAG_BIT_STRING, &len_value)) < 0
            || !decode_bitstring(&apdu[len], len_value, bit_string))
        return -1;

    len += (int)len_value;
    return len;
}

/* from clause 20.2.10 Encoding of a Bit String Value */
/* returns the number of apdu bytes consumed */
int encode_bitstring(
    uint8_t * apdu,
    BACNET_BIT_STRING * bit_string)
{
    int len = bit_string->byte_len;
    apdu[0] = bit_string->last_byte_bits_unused;

    memcpy(&apdu[1], bit_string->value, len);

    return len + 1;
}

int encode_application_bitstring(
    uint8_t * apdu,
    BACNET_BIT_STRING * bit_string)
{
    int len ;
    len = encode_tag(&apdu[0], BACNET_APPLICATION_TAG_BIT_STRING, false,
            bit_string->byte_len + 1);
    len += encode_bitstring(&apdu[len], bit_string);

    return len;
}

int encode_context_bitstring(
    uint8_t * apdu,
    uint8_t tag_number,
    BACNET_BIT_STRING * bit_string)
{
    int len;
    len = encode_tag(&apdu[0], tag_number, true, bit_string->byte_len + 1);
    len += encode_bitstring(&apdu[len], bit_string);

    return len;
}

/* from clause 20.2.14 Encoding of an Object Identifier Value */
int decode_object_id(
    const uint8_t * apdu,
    BACNET_OBJECT_TYPE * object_type,
    uint32_t * instance)
{
    uint32_t value = 0;

    decode_unsigned32(apdu, &value);
    *object_type =
        (((value >> BACNET_INSTANCE_BITS) & BACNET_MAX_OBJECT));
    *instance = (value & BACNET_MAX_INSTANCE);

    return 4;
}

int decode_context_object_id(
    const uint8_t * apdu,
    uint8_t tag_number,
    BACNET_OBJECT_TYPE *object_type,
    uint32_t * instance)
{
    uint32_t len_value;
    int len;

    if ((len = decode_match_context_tag_and_value(apdu, tag_number, &len_value)) < 0
            || len_value != 4)
        return -1;

    decode_object_id(&apdu[len], object_type, instance);
    len += (int)len_value;
    return len;
}

int decode_application_object_id(
    const uint8_t * apdu,
    BACNET_OBJECT_TYPE * object_type,
    uint32_t * instance)
{
    uint32_t len_value;
    int len;

    if ((len = decode_match_application_tag_and_value(apdu, BACNET_APPLICATION_TAG_OBJECT_ID, &len_value)) < 0
            || len_value != 4)
        return -1;

    decode_object_id(&apdu[len], object_type, instance);
    len += (int)len_value;
    return len;
}

/* from clause 20.2.14 Encoding of an Object Identifier Value */
/* returns the number of apdu bytes consumed */
int encode_bacnet_object_id(
    uint8_t * apdu,
    BACNET_OBJECT_TYPE object_type,
    uint32_t instance)
{
    uint32_t value = 0;
    uint32_t type = 0;
    int len = 0;

    type = (uint32_t) object_type;
    value =
        ((type & BACNET_MAX_OBJECT) << BACNET_INSTANCE_BITS) | (instance &
        BACNET_MAX_INSTANCE);
    len = encode_unsigned32(apdu, value);

    return len;
}

/* from clause 20.2.14 Encoding of an Object Identifier Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
int encode_context_object_id(
    uint8_t * apdu,
    uint8_t tag_number,
    BACNET_OBJECT_TYPE object_type,
    uint32_t instance)
{
    int len = 0;

    /* length of object id is 4 octets, as per 20.2.14 */

    len = encode_tag(&apdu[0], tag_number, true, 4);
    len += encode_bacnet_object_id(&apdu[len], object_type, instance);

    return len;
}

/* from clause 20.2.14 Encoding of an Object Identifier Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
int encode_application_object_id(
    uint8_t * apdu,
    BACNET_OBJECT_TYPE object_type,
    uint32_t instance)
{
    int len = 0;

    /* assumes that the tag only consumes 1 octet */
    len = encode_bacnet_object_id(&apdu[1], object_type, instance);
    len +=
        encode_tag(&apdu[0], BACNET_APPLICATION_TAG_OBJECT_ID, false,
        (uint32_t) len);

    return len;
}

/* from clause 20.2.8 Encoding of an Octet String Value */
/* returns the number of apdu bytes consumed */
int encode_octet_string(
    uint8_t * apdu,
    BACNET_OCTET_STRING * octet_string)
{
    memcpy(apdu, octet_string->value, octet_string->length);

    return (int)octet_string->length;
}

/* from clause 20.2.8 Encoding of an Octet String Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
int encode_application_octet_string(
    uint8_t * apdu,
    BACNET_OCTET_STRING * octet_string)
{
    int apdu_len = 0;

    apdu_len =
        encode_tag(&apdu[0], BACNET_APPLICATION_TAG_OCTET_STRING, false,
        octet_string->length);
    apdu_len += encode_octet_string(&apdu[apdu_len], octet_string);

    return apdu_len;
}

/* from clause 20.2.8 Encoding of an Octet String Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
int encode_context_octet_string(
    uint8_t * apdu,
    uint8_t tag_number,
    BACNET_OCTET_STRING * octet_string)
{
    int apdu_len = 0;

    apdu_len =
        encode_tag(&apdu[0], tag_number, true, octet_string->length);
    apdu_len += encode_octet_string(&apdu[apdu_len], octet_string);

    return apdu_len;
}

int encode_application_raw_octet_string(
    uint8_t *apdu,
    const uint8_t *value,
    size_t size)
{
    int apdu_len;

    apdu_len = encode_tag(&apdu[0], BACNET_APPLICATION_TAG_OCTET_STRING, false, size);
    memcpy(&apdu[apdu_len], value, size);
    apdu_len += size;

    return apdu_len;
}

int encode_context_raw_octet_string(
    uint8_t *apdu,
    uint8_t tag_number,
    const uint8_t *value,
    size_t size)
{
    int apdu_len;

    apdu_len = encode_tag(&apdu[0], tag_number, true, size);
    memcpy(&apdu[apdu_len], value, size);
    apdu_len += size;

    return apdu_len;
}

/* from clause 20.2.8 Encoding of an Octet String Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
int decode_octet_string(
    uint8_t * apdu,
    uint32_t len_value,
    BACNET_OCTET_STRING * octet_string)
{
    octet_string->length = (uint16_t)len_value;
    octet_string->value = (uint8_t*)apdu;

    return len_value;
}

int decode_context_octet_string(
    uint8_t * apdu,
    uint8_t tag_number,
    BACNET_OCTET_STRING * octet_string)
{
    uint32_t len_value;
    int len;

    if ((len = decode_match_context_tag_and_value(apdu, tag_number, &len_value)) < 0)
        return len;

    decode_octet_string(&apdu[len], len_value, octet_string);

    len += (int)len_value;
    return len;
}

int decode_application_octet_string(
    uint8_t * apdu,
    BACNET_OCTET_STRING * octet_string)
{
    uint32_t len_value;
    int len;

    if ((len = decode_match_application_tag_and_value(apdu, BACNET_APPLICATION_TAG_OCTET_STRING, &len_value)) < 0)
        return len;

    decode_octet_string(&apdu[len], len_value, octet_string);

    len += (int)len_value;
    return len;
}

int encode_character_string(
    uint8_t * apdu,
    BACNET_CHARACTER_STRING * char_string)
{
    apdu[0] = char_string->encoding;

    memcpy(&apdu[1], char_string->value, char_string->length);

    return (int)char_string->length + 1;
}

/* from clause 20.2.9 Encoding of a Character String Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
int encode_application_character_string(
    uint8_t * apdu,
    BACNET_CHARACTER_STRING * char_string)
{
    int len;

    len = encode_tag(apdu, BACNET_APPLICATION_TAG_CHARACTER_STRING, false,
            char_string->length + 1);
    len += encode_character_string(&apdu[len], char_string);

    return len;
}

int encode_context_character_string(
    uint8_t * apdu,
    uint8_t tag_number,
    BACNET_CHARACTER_STRING * char_string)
{
    int len;

    len = encode_tag(&apdu[0], tag_number, true, char_string->length + 1);
    len += encode_character_string(&apdu[len], char_string);

    return len;
}

/* returns the number of apdu bytes consumed */
int encode_application_ansi_character_string(
    uint8_t * apdu,
    const char *string,
    uint32_t size)
{
    int len;

    len = encode_tag(apdu, BACNET_APPLICATION_TAG_CHARACTER_STRING, false, size+1);
    apdu[len++] = CHARACTER_ANSI_X34;
    memcpy(&apdu[len], string, size);
    len += (int)size;

    return len;
}

/* returns the number of apdu bytes consumed */
int encode_context_ansi_character_string(
    uint8_t * apdu,
    uint8_t tag_number,
    const char *string,
    uint32_t size)
{
    int len;

    len = encode_tag(apdu, tag_number, true, size+1);
    apdu[len++] = CHARACTER_ANSI_X34;
    memcpy(&apdu[len], string, size);
    len += (int)size;

    return len;
}

/* from clause 20.2.9 Encoding of a Character String Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* return true if succes or false if error */
int decode_character_string(
    uint8_t * apdu,
    uint32_t len_value,
    BACNET_CHARACTER_STRING * char_string)
{
    if (!len_value) {
        return -1;
    }
    
    char_string->encoding = apdu[0];
    char_string->value = (char *)&apdu[1];
    char_string->length = len_value - 1;

    return len_value;
}

int decode_context_character_string(
    uint8_t * apdu,
    uint8_t tag_number,
    BACNET_CHARACTER_STRING * char_string)
{
    int len;        /* return value */
    uint32_t len_value;

    if ((len = decode_match_context_tag_and_value(apdu, tag_number, &len_value)) < 0
            || !decode_character_string(&apdu[len], len_value, char_string))
        return -1;

    len += (int)len_value;
    return len;
}

int decode_application_character_string(
    uint8_t * apdu,
    BACNET_CHARACTER_STRING * char_string)
{
    int len;        /* return value */
    uint32_t len_value;

    if ((len = decode_match_application_tag_and_value(apdu, BACNET_APPLICATION_TAG_CHARACTER_STRING, &len_value)) < 0
            || !decode_character_string(&apdu[len], len_value, char_string))
        return -1;

    len += (int)len_value;
    return len;
}

/* from clause 20.2.4 Encoding of an Unsigned Integer Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* return true if success or false if error */
int decode_unsigned(
    const uint8_t * apdu,
    uint32_t len_value,
    uint32_t * value)
{
    uint16_t unsigned16_value = 0;

    switch (len_value) {
        case 1:
            *value = apdu[0];
            break;
        case 2:
            decode_unsigned16(&apdu[0], &unsigned16_value);
            *value = unsigned16_value;
            break;
        case 3:
            decode_unsigned24(&apdu[0], value);
            break;
        case 4:
            decode_unsigned32(&apdu[0], value);
            break;
        default:
            return -1;
    }

    return len_value;
}

int decode_context_unsigned(
    const uint8_t * apdu,
    uint8_t tag_number,
    uint32_t * value)
{
    uint32_t len_value;
    int len;

    if ((len = decode_match_context_tag_and_value(apdu, tag_number, &len_value)) < 0
            || !decode_unsigned(&apdu[len], len_value, value))
        return -1;

    len += (int)len_value;
    return len;
}

int decode_application_unsigned(
    const uint8_t * apdu,
    uint32_t * value)
{
    uint32_t len_value;
    int len;

    if ((len = decode_match_application_tag_and_value(apdu, BACNET_APPLICATION_TAG_UNSIGNED_INT, &len_value)) < 0
            || !decode_unsigned(&apdu[len], len_value, value))
        return -1;

    len += (int)len_value;
    return len;
}

/* from clause 20.2.4 Encoding of an Unsigned Integer Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
int encode_unsigned(
    uint8_t * apdu,
    uint32_t value)
{
    int len = 0;        /* return value */

    if (value < 0x100) {
        apdu[0] = (uint8_t) value;
        len = 1;
    } else if (value < 0x10000) {
        len = encode_unsigned16(&apdu[0], (uint16_t) value);
    } else if (value < 0x1000000) {
        len = encode_unsigned24(&apdu[0], value);
    } else {
        len = encode_unsigned32(&apdu[0], value);
    }

    return len;
}

/* from clause 20.2.4 Encoding of an Unsigned Integer Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
int encode_context_unsigned(
    uint8_t * apdu,
    uint8_t tag_number,
    uint32_t value)
{
    int len = 0;

    /* length of unsigned is variable, as per 20.2.4 */
    if (value < 0x100) {
        len = 1;
    } else if (value < 0x10000) {
        len = 2;
    } else if (value < 0x1000000) {
        len = 3;
    } else {
        len = 4;
    }

    len = encode_tag(&apdu[0], tag_number, true, (uint32_t) len);
    len += encode_unsigned(&apdu[len], value);

    return len;
}

/* from clause 20.2.4 Encoding of an Unsigned Integer Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
int encode_application_unsigned(
    uint8_t * apdu,
    uint32_t value)
{
    int len = 0;

    len = encode_unsigned(&apdu[1], value);
    len +=
        encode_tag(&apdu[0], BACNET_APPLICATION_TAG_UNSIGNED_INT, false,
        (uint32_t) len);

    return len;
}

/* from clause 20.2.11 Encoding of an Enumerated Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
int encode_application_enumerated(
    uint8_t * apdu,
    uint32_t value)
{
    int len = 0;        /* return value */

    /* assumes that the tag only consumes 1 octet */
    len = encode_enumerated(&apdu[1], value);
    len += encode_tag(&apdu[0], BACNET_APPLICATION_TAG_ENUMERATED, false, (uint32_t)len);

    return len;
}

int decode_application_enumerated(
    const uint8_t * apdu,
    uint32_t * value)
{
    uint32_t len_value;
    int len;

    if ((len = decode_match_application_tag_and_value(apdu, BACNET_APPLICATION_TAG_ENUMERATED, &len_value)) < 0
            || !decode_unsigned(&apdu[len], len_value, value))
        return -1;

    len += (int)len_value;
    return len;
}

/* from clause 20.2.5 Encoding of a Signed Integer Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* return true if success or false if error */
int decode_signed(
    const uint8_t * apdu,
    uint32_t len_value,
    int32_t * value)
{
    if (value) {
        switch (len_value) {
            case 1:
                decode_signed8(&apdu[0], value);
                break;
            case 2:
                decode_signed16(&apdu[0], value);
                break;
            case 3:
                decode_signed24(&apdu[0], value);
                break;
            case 4:
                decode_signed32(&apdu[0], value);
                break;
            default:
                return -1;
        }
    }

    return len_value;
}

int decode_context_signed(
    const uint8_t * apdu,
    uint8_t tag_number,
    int32_t * value)
{
    uint32_t len_value;
    int len;

    if ((len = decode_match_context_tag_and_value(apdu, tag_number, &len_value)) < 0
            || !decode_signed(&apdu[len], len_value, value))
        return -1;

    len += (int)len_value;
    return len;
}

int decode_application_signed(
    const uint8_t * apdu,
    int32_t * value)
{
    uint32_t len_value;
    int len;

    if ((len = decode_match_application_tag_and_value(apdu, BACNET_APPLICATION_TAG_SIGNED_INT, &len_value)) < 0
            || !decode_signed(&apdu[len], len_value, value))
        return -1;

    len += (int)len_value;
    return len;
}

/* from clause 20.2.5 Encoding of a Signed Integer Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
int encode_signed(
    uint8_t * apdu,
    int32_t value)
{
    int len = 0;        /* return value */

    /* don't encode the leading X'FF' or X'00' of the two's compliment.
       That is, the first octet of any multi-octet encoded value shall
       not be X'00' if the most significant bit (bit 7) of the second
       octet is 0, and the first octet shall not be X'FF' if the most
       significant bit of the second octet is 1. */
    if ((value >= -128) && (value < 128)) {
        len = encode_signed8(&apdu[0], (int8_t) value);
    } else if ((value >= -32768) && (value < 32768)) {
        len = encode_signed16(&apdu[0], (int16_t) value);
    } else if ((value > -8388608) && (value < 8388608)) {
        len = encode_signed24(&apdu[0], value);
    } else {
        len = encode_signed32(&apdu[0], value);
    }

    return len;
}

/* from clause 20.2.5 Encoding of a Signed Integer Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
int encode_application_signed(
    uint8_t * apdu,
    int32_t value)
{
    int len = 0;        /* return value */

    /* assumes that the tag only consumes 1 octet */
    len = encode_signed(&apdu[1], value);
    len +=
        encode_tag(&apdu[0], BACNET_APPLICATION_TAG_SIGNED_INT, false,
        (uint32_t) len);

    return len;
}

/* from clause 20.2.5 Encoding of a Signed Integer Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
int encode_context_signed(
    uint8_t * apdu,
    uint8_t tag_number,
    int32_t value)
{
    int len = 0;        /* return value */

    /* length of signed int is variable, as per 20.2.11 */
    if ((value >= -128) && (value < 128)) {
        len = 1;
    } else if ((value >= -32768) && (value < 32768)) {
        len = 2;
    } else if ((value > -8388608) && (value < 8388608)) {
        len = 3;
    } else {
        len = 4;
    }

    len = encode_tag(&apdu[0], tag_number, true, (uint32_t) len);
    len += encode_signed(&apdu[len], value);

    return len;
}

/* from clause 20.2.6 Encoding of a Real Number Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
int encode_application_real(
    uint8_t * apdu,
    float value)
{
    int len = 0;

    /* assumes that the tag only consumes 1 octet */
    len = encode_real(value, &apdu[1]);
    len +=
        encode_tag(&apdu[0], BACNET_APPLICATION_TAG_REAL, false,
        (uint32_t) len);

    return len;
}

int encode_context_real(
    uint8_t * apdu,
    uint8_t tag_number,
    float value)
{
    int len = 0;

    /* length of double is 4 octets, as per 20.2.6 */
    len = encode_tag(&apdu[0], tag_number, true, 4);
    len += encode_real(value, &apdu[len]);
    return len;
}

int decode_context_real(
    const uint8_t * apdu,
    uint8_t tag_number,
    float *real_value)
{
    uint32_t len_value;
    int len = 0;

    if ((len = decode_match_context_tag_and_value(apdu, tag_number, &len_value)) < 0
            || len_value != 4)
        return -1;

    return len + decode_real(&apdu[len], real_value);
}

int decode_application_real(
    const uint8_t * apdu,
    float *real_value)
{
    uint32_t len_value;
    int len = 0;

    if ((len = decode_match_application_tag_and_value(apdu, BACNET_APPLICATION_TAG_REAL, &len_value)) < 0
            || len_value != 4)
        return -1;

    return len + decode_real(&apdu[len], real_value);
}

/* from clause 20.2.7 Encoding of a Double Precision Real Number Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
int encode_application_double(
    uint8_t * apdu,
    double value)
{
    int len = 0;

    /* assumes that the tag only consumes 2 octet */
    len = encode_double(value, &apdu[2]);

    len +=
        encode_tag(&apdu[0], BACNET_APPLICATION_TAG_DOUBLE, false,
        (uint32_t) len);

    return len;
}

int encode_context_double(
    uint8_t * apdu,
    uint8_t tag_number,
    double value)
{
    int len = 0;

    /* length of double is 8 octets, as per 20.2.7 */
    len = encode_tag(&apdu[0], tag_number, true, 8);
    len += encode_double(value, &apdu[len]);

    return len;
}

int decode_context_double(
    const uint8_t * apdu,
    uint8_t tag_number,
    double *double_value)
{
    uint32_t len_value;
    int len = 0;

    if ((len = decode_match_context_tag_and_value(apdu, tag_number, &len_value)) < 0
            || len_value != 8)
        return -1;

    return len + decode_double(&apdu[len], double_value);
}

int decode_application_double(
    const uint8_t * apdu,
    double *double_value)
{
    uint32_t len_value;
    int len = 0;

    if ((len = decode_match_application_tag_and_value(apdu, BACNET_APPLICATION_TAG_DOUBLE, &len_value)) < 0
            || len_value != 8)
        return -1;

    return len + decode_double(&apdu[len], double_value);
}

/* from clause 20.2.13 Encoding of a Time Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
int encode_bacnet_time(
    uint8_t * apdu,
    const BACNET_TIME * btime)
{
    apdu[0] = btime->hour;
    apdu[1] = btime->min;
    apdu[2] = btime->sec;
    apdu[3] = btime->hundredths;

    return 4;
}

/* from clause 20.2.13 Encoding of a Time Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
int encode_application_time(
    uint8_t * apdu,
    const BACNET_TIME * btime)
{
    int len = 0;

    /* assumes that the tag only consumes 1 octet */
    len = encode_bacnet_time(&apdu[1], btime);
    len +=
        encode_tag(&apdu[0], BACNET_APPLICATION_TAG_TIME, false,
        (uint32_t) len);

    return len;
}

int encode_context_time(
    uint8_t * apdu,
    uint8_t tag_number,
    const BACNET_TIME * btime)
{
    int len = 0;        /* return value */

    /* length of time is 4 octets, as per 20.2.13 */
    len = encode_tag(&apdu[0], tag_number, true, 4);
    len += encode_bacnet_time(&apdu[len], btime);

    return len;
}

/* from clause 20.2.13 Encoding of a Time Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
int decode_bacnet_time(
    const uint8_t * apdu,
    BACNET_TIME * btime)
{
    btime->hour = apdu[0];
    btime->min = apdu[1];
    btime->sec = apdu[2];
    btime->hundredths = apdu[3];

    return 4;
}

int decode_application_time(
    const uint8_t * apdu,
    BACNET_TIME * btime)
{
    int len;
    uint32_t len_value;

    if ((len = decode_match_application_tag_and_value(apdu, BACNET_APPLICATION_TAG_TIME, &len_value)) < 0
            || len_value != 4)
        return -1;

    decode_bacnet_time(&apdu[len], btime);
    len += (int)len_value;
    return len;
}

int decode_context_bacnet_time(
    const uint8_t * apdu,
    uint8_t tag_number,
    BACNET_TIME * btime)
{
    int len;
    uint32_t len_value;

    if ((len = decode_match_context_tag_and_value(apdu, tag_number, &len_value)) < 0
            || len_value != 4)
        return -1;

    decode_bacnet_time(&apdu[len], btime);
    len += (int)len_value;
    return len;
}

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
    const BACNET_DATE * bdate)
{
    /* allow 2 digit years */
    if (bdate->year >= 1900) {
        apdu[0] = (uint8_t) (bdate->year - 1900);
    } else if (bdate->year < 0x100) {
        apdu[0] = (uint8_t) bdate->year;

    } else {
        /*
         ** Don't try and guess what the user meant here. Just fail
         */
        return -1;
    }

    apdu[1] = bdate->month;
    apdu[2] = bdate->day;
    apdu[3] = bdate->wday;

    return 4;
}


/* from clause 20.2.12 Encoding of a Date Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
/* returns the number of apdu bytes consumed */
int encode_application_date(
    uint8_t * apdu,
    const BACNET_DATE * bdate)
{
    int len = 0;

    /* assumes that the tag only consumes 1 octet */
    len = encode_bacnet_date(&apdu[1], bdate);
    len +=
        encode_tag(&apdu[0], BACNET_APPLICATION_TAG_DATE, false,
        (uint32_t) len);

    return len;

}

int encode_context_date(
    uint8_t * apdu,
    uint8_t tag_number,
    const BACNET_DATE * bdate)
{
    int len = 0;        /* return value */

    /* length of date is 4 octets, as per 20.2.12 */
    len = encode_tag(&apdu[0], tag_number, true, 4);
    len += encode_bacnet_date(&apdu[len], bdate);

    return len;
}

/* from clause 20.2.12 Encoding of a Date Value */
/* and 20.2.1 General Rules for Encoding BACnet Tags */
int decode_date(
    const uint8_t * apdu,
    BACNET_DATE * bdate)
{
    bdate->year = (uint16_t) (apdu[0] + 1900);
    bdate->month = apdu[1];
    bdate->day = apdu[2];
    bdate->wday = apdu[3];

    return 4;
}

int decode_application_date(
    const uint8_t * apdu,
    BACNET_DATE * bdate)
{
    int len;
    uint32_t len_value;

    if ((len = decode_match_application_tag_and_value(apdu, BACNET_APPLICATION_TAG_DATE, &len_value)) < 0
            || len_value != 4)
        return -1;

    decode_date(&apdu[len], bdate);
    len += (int)len_value;
    return len;
}

int decode_context_date(
    const uint8_t * apdu,
    uint8_t tag_number,
    BACNET_DATE * bdate)
{
    int len;
    uint32_t len_value;

    if ((len = decode_match_context_tag_and_value(apdu, tag_number, &len_value)) < 0
            || len_value != 4)
        return -1;

    decode_date(&apdu[len], bdate);
    len += (int)len_value;
    return len;
}

/* returns the number of apdu bytes consumed */
int encode_simple_ack(
    uint8_t * apdu,
    uint8_t invoke_id,
    uint8_t service_choice)
{
    apdu[0] = PDU_TYPE_SIMPLE_ACK << 4;
    apdu[1] = invoke_id;
    apdu[2] = service_choice;

    return 3;
}

int decode_constructed_tag(
    const uint8_t *apdu,
    uint32_t remaining_bytes,
    uint8_t tag_number)
{
    int len, level;
    uint32_t len_value;
    uint8_t construct_tags[16]; /* should be enough for 16 nested level */

    if ((len = decode_opening_tag(apdu, tag_number)) < 0)
        return len;

    construct_tags[0] = tag_number;
    level = 1;

    for(;;) {
        uint8_t tag = apdu[len];
        if (IS_OPENING_TAG(tag)) {
            if (level >= sizeof(construct_tags))
                return -1;
            int dec_len = decode_tag_number(&apdu[len], &tag_number);
            if (dec_len < 0) return dec_len;
            len += dec_len;

            construct_tags[level++] = tag_number;
        } else if (IS_CLOSING_TAG(tag)) {
            int dec_len = decode_closing_tag(&apdu[len], construct_tags[--level]);
            if (dec_len < 0) return dec_len;
            len += dec_len;

            if (level == 0) {
                if (len > remaining_bytes)
                    return -1;
                break;
            }
        } else if (VALID_APPLICATION_TAG_NUMBER(tag) == BACNET_APPLICATION_TAG_BOOLEAN) {
            len++;
        } else {
            int dec_len = decode_tag_number_and_value(&apdu[len], &tag_number, &len_value);
            if (dec_len < 0) return dec_len;
            len += dec_len + len_value;
        }

        if (len >= remaining_bytes)
            return -1;
    }

    return len;
}

