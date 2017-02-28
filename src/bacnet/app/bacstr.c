/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bacstr.c
 * Original Author:  linzhixian, 2015-1-28
 *
 * BACnet Character String
 *
 * History
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>     /* for strlen */
#include "bacnet/bacstr.h"
#include "bacnet/bacenum.h"
#include "misc/bits.h"

/** @file bacstr.c  Manipulate Bit/Char/Octet Strings */
bool vbuf_fr_str(
    vbuf_t *dest,
    const char* src,
    size_t max_size)
{
    if (!dest || !src)
        return false;

    int len = strlen(src);
    if (len > max_size)
        return false;

    memcpy(dest->value, src, len);
    dest->length = len;
    return true;
}

bool vbuf_fr_buf(
    vbuf_t *dest,
    uint8_t *src,
    size_t len)
{
    if (!dest || !src)
        return false;

    memcpy(dest->value, src, len);
    dest->length = len;
    return true;
}

bool bitstring_init(
    BACNET_BIT_STRING *bit_string,
    uint8_t *value,
    size_t bit_number)
{
    if (!bit_string || !value)
        return false;

    bit_string->value = value;
    bitstring_resize(bit_string, bit_number);
    return true;
}

void bitstring_resize(
    BACNET_BIT_STRING *bit_string,
    uint32_t bit_number)
{
    bit_string->byte_len = (bit_number + 7) >> 3;
    bit_string->last_byte_bits_unused = (8 - bit_number) & 0x07;
}

/* if length inside dest < length required, partially copy */
bool bitstring_copy(
    BACNET_BIT_STRING *dest,
    uint8_t *src,
    size_t bit_number)
{
    if (!dest || !src || (bit_number && !dest->value))
        return false;

    int length = (bit_number + 7) >> 3;
    uint8_t unused = (8 - bit_number) & 0x07;
    if (length > dest->byte_len) {
        memcpy(dest->value, src, dest->byte_len);
        return false;
    } else {
        dest->byte_len = length;
        dest->last_byte_bits_unused = unused;
        memcpy(dest->value, src, length);
        return true;
    }
}

void bitstring_set_bit(
    BACNET_BIT_STRING  *bit_string,
    uint32_t bit_number,
    bool value)
{
    if (value) {
        bit_string->value[bit_number >> 3] |= 0x80 >> (bit_number & 7);
    } else {
        bit_string->value[bit_number >> 3] &= ~(0x80 >> (bit_number & 7));
    }
}

bool bitstring_get_bit(
    BACNET_BIT_STRING *bit_string,
    uint32_t bit_number)
{
    return (bit_string->value[bit_number >> 3] & (0x80 >> (bit_number & 7))) != 0;
}

int bitstring_size(
        BACNET_BIT_STRING *bit_string)
{
    return bit_string->byte_len * 8 - bit_string->last_byte_bits_unused;
}

bool octetstring_init(
    BACNET_OCTET_STRING *octet_string,
    uint8_t *value,
    size_t size)
{
    if (!octet_string || !value)
        return false;

    octet_string->value = value;
    octet_string->length = size;
    return true;
}

/* if length < size, partially copy */
bool octetstring_copy(
    BACNET_OCTET_STRING *dest,
    uint8_t *src,
    size_t size)
{
    if (!dest || !src || (size && !dest->value))
        return false;

    if (size > dest->length) {
        memcpy(dest->value, src, dest->length);
        return false;
    } else {
        dest->length = size;
        memcpy(dest->value, src, size);
        return true;
    }
}

bool characterstring_init(
    BACNET_CHARACTER_STRING * char_string,
    uint8_t encoding,
    char *value,
    size_t size)
{
    if (!char_string || !value)
        return false;

    char_string->length = size;
    char_string->encoding = encoding;
    char_string->value = value;

    return true;
}

bool characterstring_init_ansi(
    BACNET_CHARACTER_STRING * char_string,
    char *value,
    size_t size)
{
    return characterstring_init(char_string, CHARACTER_ANSI_X34, value, size);
}

/* if length < size, partially copy */
bool characterstring_copy(
    BACNET_CHARACTER_STRING *dest,
    BACNET_CHARACTER_STRING *src)
{
    if (!dest || !src)
        return false;

    if (src->length && (!src->value || !dest->value))
        return false;

    dest->encoding = src->encoding;
    if (src->length > dest->length) {
        memcpy(dest->value, src->value, dest->length);
        return false;
    } else {
        memcpy(dest->value, src->value, src->length);
        dest->length = src->length;
        return true;
    }
}

/* return true if all bytes successfully copied */
bool characterstring_cpto_ansi(
    char *dest,
    size_t dest_max_len,
    BACNET_CHARACTER_STRING * src)
{
    if (!dest || !src || (src->length && !src->value))
        return false;

    memcpy(dest, src->value, dest_max_len >= src->length ? src->length : dest_max_len);

    return dest_max_len >= src->length;
}

/* if length < size, partially copy */
bool characterstring_cpfr_ansi(
    BACNET_CHARACTER_STRING * dest,
    char *src,
    size_t size)
{
    if (!dest || !src || (size && !dest->value))
        return false;

    dest->encoding = CHARACTER_ANSI_X34;
    if (size > dest->length) {
        memcpy(dest->value, src, dest->length);
        return false;
    } else {
        dest->length = size;
        memcpy(dest->value, src, size);
        return true;
    }
}

bool characterstring_same(
    BACNET_CHARACTER_STRING *dest,
    BACNET_CHARACTER_STRING *src)
{
    if (!dest && !src)
        return true;

    if (!dest || !src)
        return false;

    if (src->length != dest->length || src->encoding != dest->encoding)
        return false;

    if (src->length) {
        return memcmp(dest->value, src->value, src->length) == 0;
    }

    return true;
}

bool characterstring_ansi_same(
    BACNET_CHARACTER_STRING * dest,
    const char *src,
    uint32_t size)
{
    if (dest->encoding != CHARACTER_ANSI_X34) {
        return false;
    } else if (size != dest->length)
        return false;

    return memcmp(src, dest->value, size) == 0;
}

/* returns true if string is printable */
/* used to assist in the requirement that
   "The set of characters used in the Object_Name shall be
   restricted to printable characters." */
/* printable character: a character that represents a printable
symbol as opposed to a device control character. These
include, but are not limited to, upper- and lowercase letters,
punctuation marks, and mathematical symbols. The exact set
depends upon the character set being used. In ANSI X3.4 the
printable characters are represented by single octets in the range
X'20' - X'7E'.*/
bool characterstring_printable(
    BACNET_CHARACTER_STRING * char_string)
{
    bool status = false;        /* return value */
    size_t i;   /* counter */

    if (!char_string) {
        return false;
    } else if (char_string->encoding != CHARACTER_ANSI_X34) {
        return false;
    }


    const char *value = char_string->value;
    uint32_t length = char_string->length;

    if (length > 0 && !value) {
        return false;
    }

    status = true;
    for (i = 0; i < length; i++) {
        if ((value[i] < 0x20) || (value[i] > 0x7E)) {
            status = false;
            break;
        }
    }

    return status;
}

/* Basic UTF-8 manipulation routines
  by Jeff Bezanson
  placed in the public domain Fall 2005 */
static const char trailingBytesForUTF8[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4,
    4, 4, 4, 5, 5, 5, 5
};

/* based on the valid_utf8 routine from the PCRE library by Philip Hazel
   length is in bytes, since without knowing whether the string is valid
   it's hard to know how many characters there are! */
bool utf8_isvalid(
    const char *str,
    size_t length)
{
    const unsigned char *p, *pend = (unsigned char *) str + length;
    unsigned char c;
    size_t ab;

    /* empty string is valid */
    if (length == 0) {
        return true;
    }
    for (p = (const unsigned char *) str; p < pend; p++) {
        c = *p;
        /* null in middle of string */
        if (c == 0) {
            return false;
        }
        /* ASCII character */
        if (c < 128) {
            continue;
        }
        if ((c & 0xc0) != 0xc0) {
            return false;
        }
        ab = (size_t) trailingBytesForUTF8[c];
        if (length < ab) {
            return false;
        }
        length -= ab;

        p++;
        /* Check top bits in the second byte */
        if ((*p & 0xc0) != 0x80) {
            return false;
        }
        /* Check for overlong sequences for each different length */
        switch (ab) {
                /* Check for xx00 000x */
            case 1:
                if ((c & 0x3e) == 0)
                    return false;
                continue;       /* We know there aren't any more bytes to check */

                /* Check for 1110 0000, xx0x xxxx */
            case 2:
                if (c == 0xe0 && (*p & 0x20) == 0)
                    return false;
                break;

                /* Check for 1111 0000, xx00 xxxx */
            case 3:
                if (c == 0xf0 && (*p & 0x30) == 0)
                    return false;
                break;

                /* Check for 1111 1000, xx00 0xxx */
            case 4:
                if (c == 0xf8 && (*p & 0x38) == 0)
                    return false;
                break;

                /* Check for leading 0xfe or 0xff,
                   and then for 1111 1100, xx00 00xx */
            case 5:
                if (c == 0xfe || c == 0xff || (c == 0xfc && (*p & 0x3c) == 0))
                    return false;
                break;
        }

        /* Check for valid bytes after the 2nd, if any; all must start 10 */
        while (--ab > 0) {
            if ((*(++p) & 0xc0) != 0x80)
                return false;
        }
    }

    return true;
}

bool characterstring_valid(
    BACNET_CHARACTER_STRING * char_string)
{
    if (!char_string) {
        return false;
    } else if (char_string->length && !char_string->value) {
        return false;
    } else if (char_string->encoding != CHARACTER_ANSI_X34) {
        return false;
    }

    return utf8_isvalid(char_string->value, char_string->length);
}
