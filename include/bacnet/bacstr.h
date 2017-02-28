/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bacstr.h
 * Original Author:  linzhixian, 2015-1-27
 *
 * BACnet String
 *
 * History
 */

#ifndef _BACSTR_H_
#define _BACSTR_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "bacnet/bacdef.h"
#include "bacnet/bacenum.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct BACnet_Bit_String {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint32_t byte_len : 24;
    uint8_t last_byte_bits_unused;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    uint8_t last_byte_bits_unused;
    uint32_t byte_len : 24;
#else
#error "unsupport byte order"
#endif
    uint8_t *value;
} BACNET_BIT_STRING;

typedef struct BACnet_Character_String {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint32_t length : 24;
    uint8_t encoding;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    uint8_t encoding;
    uint32_t byte_len : 24;
#else
#error "unsupport byte order"
#endif
    char *value;
} BACNET_CHARACTER_STRING;

typedef struct BACnet_Octet_String {
    uint32_t length;
    uint8_t *value;
} BACNET_OCTET_STRING;

typedef struct vbuf_s {
#ifdef LARGE_V_BUF_SIZE
    uint32_t byte_len;
#elif defined(BIG_V_BUF_SIZE)
    uint16_t byte_len;
#else
    uint8_t length;
#endif
    uint8_t value[];
} vbuf_t;

#define DECLARE_VBUF(name, size) \
    struct { vbuf_t vbuf; uint8_t __padding[size];} name

#define VBUF_INIT(name, size, str) \
    struct { vbuf_t vbuf; uint8_t __padding[size]; \
    } name = {{sizeof(str) - 1, }, str}

#define VBUF_STR(name, str) \
    struct { vbuf_t vbuf; uint8_t __padding[sizeof(str)]; \
    } name = {{sizeof(name.__padding) - 1,}, str}

#define VBUF_SIZE(size) (sizeof(struct {vbuf_t vbuf; uint8_t __padding[size];}))

/**
* set vbuf from str with max_size
* @param max_size, capacity of vbuf
* @return true success
*/
bool vbuf_fr_str(
    vbuf_t *dest,
    const char* src,
    size_t max_size);

/**
* set vbuf from buf
* @param len, len of source buf
* @return true success
*/
bool vbuf_fr_buf(
    vbuf_t *dest,
    uint8_t *src,
    size_t len);

bool bitstring_init(
    BACNET_BIT_STRING *bit_string,
    uint8_t *value,
    size_t bit_number);

void bitstring_resize(
    BACNET_BIT_STRING *bit_string,
    uint32_t bit_number);

/* if length inside dest < length required, partially copy */
bool bitstring_copy(
    BACNET_BIT_STRING *dest,
    uint8_t *src,
    size_t bit_number);

void bitstring_set_bit(
    BACNET_BIT_STRING *bit_string,
    uint32_t bit_number,
    bool value);

bool bitstring_get_bit(
    BACNET_BIT_STRING *bit_string,
    uint32_t bit_number);

int bitstring_size(
    BACNET_BIT_STRING *bit_string);

bool octetstring_init(
    BACNET_OCTET_STRING *octet_string,
    uint8_t *value,
    size_t size);

static inline void octetstring_resize(
    BACNET_OCTET_STRING *octet_string,
    size_t size) {octet_string->length = size;}

/* if length < size, partially copy */
bool octetstring_copy(
    BACNET_OCTET_STRING *dest,
    uint8_t *src,
    size_t size);

/* use value as internal buffer */
bool characterstring_init(
    BACNET_CHARACTER_STRING * char_string,
    uint8_t encoding,
    char *value,
    size_t size);

/* use value as internal buffer */
bool characterstring_init_ansi(
    BACNET_CHARACTER_STRING * char_string,
    char *value,
    size_t size);

/* set internal buffer length */
static inline void characterstring_resize(
    BACNET_CHARACTER_STRING *char_string,
    size_t size) { char_string->length = size; }

/* if length < size, partially copy */
bool characterstring_copy(
    BACNET_CHARACTER_STRING *dest,
    BACNET_CHARACTER_STRING *src);

/* copy internal buffer to dest */
bool characterstring_cpto_ansi(
    char *dest,
    size_t dest_max_len,
    BACNET_CHARACTER_STRING * src);

/* if length < size, partially copy */
bool characterstring_cpfr_ansi(
    BACNET_CHARACTER_STRING * dest,
    char *src,
    size_t size);

bool characterstring_same(
    BACNET_CHARACTER_STRING *dest,
    BACNET_CHARACTER_STRING *src);

bool characterstring_ansi_same(
    BACNET_CHARACTER_STRING * dest,
    const char *src,
    size_t size);

bool characterstring_printable(
    BACNET_CHARACTER_STRING * char_string);

bool characterstring_valid(
    BACNET_CHARACTER_STRING * char_string);

bool utf8_isvalid(
    const char *str,
    size_t length);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
