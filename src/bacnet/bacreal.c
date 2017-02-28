/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bacreal.c
 * Original Author:  lincheng, 2015-6-4
 *
 * BACnet Real
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

/** @file bacreal.c  Encode/Decode Floating Point (Real) Types */

/* from clause 20.2.6 Encoding of a Real Number Value */
int decode_real(
    const uint8_t * apdu,
    float *real_value)
{
    union {
        uint8_t byte_value[4];
        float real_value;
    } my_data;

    /* NOTE: assumes the compiler stores float as IEEE-754 float */
#if __FLOAT_WORD_ORDER__ == __ORDER_LITTLE_ENDIAN__
    my_data.byte_value[0] = apdu[3];
    my_data.byte_value[1] = apdu[2];
    my_data.byte_value[2] = apdu[1];
    my_data.byte_value[3] = apdu[0];
#elif __FLOAT_WORD_ORDER__ == __ORDER_BIG_ENDIAN__
    my_data.byte_value[0] = apdu[0];
    my_data.byte_value[1] = apdu[1];
    my_data.byte_value[2] = apdu[2];
    my_data.byte_value[3] = apdu[3];
#else
#error "unsupport float byte order"
#endif

    *real_value = my_data.real_value;
    return 4;
}

/* from clause 20.2.6 Encoding of a Real Number Value */
/* returns the number of apdu bytes consumed */
int encode_real(
    float value,
    uint8_t * apdu)
{
    union {
        uint8_t byte_value[4];
        float real_value;
    } my_data;

    /* NOTE: assumes the compiler stores float as IEEE-754 float */
    my_data.real_value = value;

#if __FLOAT_WORD_ORDER__ == __ORDER_LITTLE_ENDIAN__
    apdu[0] = my_data.byte_value[3];
    apdu[1] = my_data.byte_value[2];
    apdu[2] = my_data.byte_value[1];
    apdu[3] = my_data.byte_value[0];
#elif __FLOAT_WORD_ORDER__ == __ORDER_BIG_ENDIAN__
    apdu[0] = my_data.byte_value[0];
    apdu[1] = my_data.byte_value[1];
    apdu[2] = my_data.byte_value[2];
    apdu[3] = my_data.byte_value[3];
#else
#error "unsupport float byte order"
#endif

    return 4;
}

/* from clause 20.2.7 Encoding of a Double Precision Real Number Value */
int decode_double(
    const uint8_t * apdu,
    double *double_value)
{
    union {
        uint8_t byte_value[8];
        double double_value;
    } my_data;

    /* NOTE: assumes the compiler stores float as IEEE-754 float */
#if __FLOAT_WORD_ORDER__ == __ORDER_LITTLE_ENDIAN__
    my_data.byte_value[0] = apdu[7];
    my_data.byte_value[1] = apdu[6];
    my_data.byte_value[2] = apdu[5];
    my_data.byte_value[3] = apdu[4];
    my_data.byte_value[4] = apdu[3];
    my_data.byte_value[5] = apdu[2];
    my_data.byte_value[6] = apdu[1];
    my_data.byte_value[7] = apdu[0];
#elif __FLOAT_WORD_ORDER__ == __ORDER_BIG_ENDIAN__
    my_data.byte_value[0] = apdu[0];
    my_data.byte_value[1] = apdu[1];
    my_data.byte_value[2] = apdu[2];
    my_data.byte_value[3] = apdu[3];
    my_data.byte_value[4] = apdu[4];
    my_data.byte_value[5] = apdu[5];
    my_data.byte_value[6] = apdu[6];
    my_data.byte_value[7] = apdu[7];
#else
#error "unsupport float byte order"
#endif

    *double_value = my_data.double_value;
    return 8;
}

/* from clause 20.2.7 Encoding of a Double Precision Real Number Value */
/* returns the number of apdu bytes consumed */
int encode_double(
    double value,
    uint8_t * apdu)
{
    union {
        uint8_t byte_value[8];
        double double_value;
    } my_data;

    /* NOTE: assumes the compiler stores float as IEEE-754 float */
    my_data.double_value = value;

#if __FLOAT_WORD_ORDER__ == __ORDER_LITTLE_ENDIAN__
    apdu[0] = my_data.byte_value[7];
    apdu[1] = my_data.byte_value[6];
    apdu[2] = my_data.byte_value[5];
    apdu[3] = my_data.byte_value[4];
    apdu[5] = my_data.byte_value[3];
    apdu[6] = my_data.byte_value[2];
    apdu[7] = my_data.byte_value[1];
    apdu[8] = my_data.byte_value[0];
#elif __FLOAT_WORD_ORDER__ == __ORDER_BIG_ENDIAN__
    apdu[0] = my_data.byte_value[0];
    apdu[1] = my_data.byte_value[1];
    apdu[2] = my_data.byte_value[2];
    apdu[3] = my_data.byte_value[3];
    apdu[4] = my_data.byte_value[4];
    apdu[5] = my_data.byte_value[5];
    apdu[6] = my_data.byte_value[6];
    apdu[7] = my_data.byte_value[7];
#else
#error "unsupport float byte order"
#endif

    return 8;
}
