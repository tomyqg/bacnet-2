/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bacint.h
 * Original Author:  linzhixian, 2015-03-31
 *
 * Encode/Decode Integer Types
 *
 * History
 */

#ifndef _BACINT_H_
#define _BACINT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

extern int encode_signed8(uint8_t *pdu, int8_t value);

extern int encode_signed16(uint8_t *pdu, int16_t value);

extern int encode_signed24(uint8_t *pdu, int32_t value);

extern int encode_signed32(uint8_t *pdu, int32_t value);

extern int encode_unsigned16(uint8_t *pdu, uint16_t value);

extern int encode_unsigned24(uint8_t *pdu, uint32_t value);

extern int encode_unsigned32(uint8_t *pdu, uint32_t value);

extern int decode_signed8(const uint8_t *pdu, int32_t *value);

extern int decode_signed16(const uint8_t *pdu, int32_t *value);

extern int decode_signed24(const uint8_t *pdu, int32_t *value);

extern int decode_signed32(const uint8_t *pdu, int32_t *value);

extern int decode_unsigned16(const uint8_t *pdu, uint16_t *value);

extern int decode_unsigned24(const uint8_t *pdu, uint32_t *value);

extern int decode_unsigned32(const uint8_t *pdu, uint32_t *value);

extern int bacnet_macstr_to_array(char *str, uint8_t *array, uint8_t array_size);

extern int bacnet_array_to_macstr(uint8_t *array, uint8_t size, char *str, uint8_t max_str_len);

#ifdef __cplusplus
}
#endif

#endif  /* _BACINT_H_ */

