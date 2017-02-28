/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bacint.c
 * Original Author:  linzhixian, 2014-10-21
 *
 * Encode/Decode Integer Types
 *
 * History
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "bacnet/bacint.h"

int encode_signed8(uint8_t *pdu, int8_t value)
{
    pdu[0] = (uint8_t)value;

    return 1;
}

int encode_signed16(uint8_t *pdu, int16_t value)
{
    pdu[0] = (uint8_t)((value & 0xff00) >> 8);
    pdu[1] = (uint8_t)(value & 0x00ff);

    return 2;
}

int encode_signed24(uint8_t *pdu, int32_t value)
{
    pdu[0] = (uint8_t)((value & 0xff0000) >> 16);
    pdu[1] = (uint8_t)((value & 0x00ff00) >> 8);
    pdu[2] = (uint8_t)(value & 0x0000ff);

    return 3;
}

int encode_signed32(uint8_t *pdu, int32_t value)
{
    pdu[0] = (uint8_t)((value & 0xff000000) >> 24);
    pdu[1] = (uint8_t)((value & 0x00ff0000) >> 16);
    pdu[2] = (uint8_t)((value & 0x0000ff00) >> 8);
    pdu[3] = (uint8_t)(value & 0x000000ff);

    return 4;
}

int encode_unsigned16(uint8_t *pdu, uint16_t value)
{
    pdu[0] = (uint8_t)((value & 0xff00) >> 8);
    pdu[1] = (uint8_t)(value & 0x00ff);

    return 2;
}

int encode_unsigned24(uint8_t *pdu, uint32_t value)
{
    pdu[0] = (uint8_t)((value & 0xff0000) >> 16);
    pdu[1] = (uint8_t)((value & 0x00ff00) >> 8);
    pdu[2] = (uint8_t)(value & 0x0000ff);

    return 3;
}

int encode_unsigned32(uint8_t *pdu, uint32_t value)
{
    pdu[0] = (uint8_t)((value & 0xff000000) >> 24);
    pdu[1] = (uint8_t)((value & 0x00ff0000) >> 16);
    pdu[2] = (uint8_t)((value & 0x0000ff00) >> 8);
    pdu[3] = (uint8_t)(value & 0x000000ff);

    return 4;
}

int decode_signed8(const uint8_t *pdu, int32_t *value)
{
    if (value) {
        if (pdu[0] & 0x80) {                /* negative - bit 7 is set */
            *value = 0xFFFFFF00;
        } else {
            *value = 0;
        }
        
        *value |= ((int32_t)(((int32_t)pdu[0]) & 0x000000ff));
    }

    return 1;
}

int decode_signed16(const uint8_t *pdu, int32_t *value)
{
    if (value) {
        if (pdu[0] & 0x80) {                /* negative - bit 7 is set */
            *value = 0xFFFF0000;
        } else {
            *value = 0;
        }
        
        *value |= ((int32_t)((((int32_t)pdu[0]) << 8) & 0x0000ff00));
        *value |= ((int32_t)(((int32_t)pdu[1]) & 0x000000ff));
    }

    return 2;
}

int decode_signed24(const uint8_t *pdu, int32_t *value)
{
    if (value) {
        if (pdu[0] & 0x80) {                /* negative - bit 7 is set */
            *value = 0xFF000000;
        } else {
            *value = 0;
        }
        
        *value |= ((int32_t)((((int32_t)pdu[0]) << 16) & 0x00ff0000));
        *value |= ((int32_t)((((int32_t)pdu[1]) << 8) & 0x0000ff00));
        *value |= ((int32_t)(((int32_t)pdu[2]) & 0x000000ff));
    }

    return 3;
}

int decode_signed32(const uint8_t *pdu, int32_t *value)
{
    if (value) {
        *value = ((int32_t)((((int32_t)pdu[0]) << 24) & 0xff000000));
        *value |= ((int32_t)((((int32_t)pdu[1]) << 16) & 0x00ff0000));
        *value |= ((int32_t)((((int32_t)pdu[2]) << 8) & 0x0000ff00));
        *value |= ((int32_t)(((int32_t)pdu[3]) & 0x000000ff));
    }

    return 4;
}

int decode_unsigned16(const uint8_t *pdu, uint16_t *value)
{
    if (value) {
        *value = (uint16_t)((((uint16_t)pdu[0]) << 8) & 0xff00);
        *value |= ((uint16_t)(((uint16_t)pdu[1]) & 0x00ff));
    }

    return 2;
}

int decode_unsigned24(const uint8_t *pdu, uint32_t *value)
{
    if (value) {
        *value = ((uint32_t)((((uint32_t)pdu[0]) << 16) & 0x00ff0000));
        *value |= (uint32_t)((((uint32_t)pdu[1]) << 8) & 0x0000ff00);
        *value |= ((uint32_t)(((uint32_t)pdu[2]) & 0x000000ff));
    }

    return 3;
}

int decode_unsigned32(const uint8_t *pdu, uint32_t *value)
{
    if (value) {
        *value = ((uint32_t)((((uint32_t)pdu[0]) << 24) & 0xff000000));
        *value |= ((uint32_t)((((uint32_t)pdu[1]) << 16) & 0x00ff0000));
        *value |= ((uint32_t)((((uint32_t)pdu[2]) << 8) & 0x0000ff00));
        *value |= ((uint32_t)(((uint32_t)pdu[3]) & 0x000000ff));
    }

    return 4;
}

/* 将MAC字符串转换成数组形式，成功返回实际的MAC地址长度，否则返回负数 */
int bacnet_macstr_to_array(char *str, uint8_t *array, uint8_t array_size)
{
    char *p;
    uint32_t len;
    uint32_t value;
    int i;
    int rv;

    if ((str == NULL) || (array == NULL) || (array_size == 0)) {
        printf("bacnet_macstr_to_array: invalid argument\r\n");
        return -EINVAL;
    }

    len = strlen(str);
    if (len % 2 != 0) {
        printf("bacnet_macstr_to_array: invalid macstr_len(%d)\r\n", len);
        return -EINVAL;
    }

    len = len / 2;
    if ((len < 1) || (len > array_size)) {
        printf("bacnet_macstr_to_array: invalid mac_len(%d)\r\n", len);
        return -EINVAL;
    }

    p = str;
    for (i = 0; i < len; i++) {
        rv = sscanf(p, "%2x", &value);
        if (rv != 1) {
            printf("bacnet_macstr_to_array: sscanf failed(%d)\r\n", rv);
            return -EPERM;
        }
        
        p += 2;
        array[i] = (uint8_t)value;
    }
    
    return len;
}

int bacnet_array_to_macstr(uint8_t *array, uint8_t size, char *str, uint8_t max_str_len)
{
    int len, tmp_len;
    int remaining_len;
    int i;

    if ((array == NULL) || (size == 0) || (str == NULL) || (max_str_len == 0)) {
        printf("bacnet_array_to_macstr: invalid argument\r\n");
        return -EINVAL;
    }

    len = (size * 2) + 1;    
    if (len > max_str_len) {
        printf("bacnet_array_to_macstr: max_str_len is too small(%d)\r\n", max_str_len);
        return -EPERM;
    }

    remaining_len = len;
    len = 0;
    for (i = 0; i < size; i++) {
        tmp_len = snprintf(&str[len], remaining_len, "%02X", array[i]);
        if ((tmp_len < 0) || (tmp_len >= remaining_len)) {
            printf("bacnet_array_to_macstr: no space for snprintf\r\n");
            return -EPERM;
        }
        
        len += tmp_len;
        remaining_len -= tmp_len;
    }
    str[len++] = 0;

    return len;
}

