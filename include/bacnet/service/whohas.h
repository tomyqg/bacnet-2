/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * whohas.h
 * Original Author:  linzhixian, 2015-1-15
 *
 * Who Has Request Service
 *
 * History
 */

#ifndef _WHOHAS_H_
#define _WHOHAS_H_

#include <stdint.h>
#include <stdbool.h>

#include "bacnet/bacenum.h"
#include "bacnet/bacapp.h"
#include "bacnet/bacstr.h"
#include "bacnet/bacnet_buf.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct BACnet_Who_Has_Data {
    int32_t low_limit;                  /* deviceInstanceRange */
    int32_t high_limit;
    bool is_object_name;                /* true if a string */
    union {
        BACNET_OBJECT_ID identifier;
        BACNET_CHARACTER_STRING name;
    } object;
} BACNET_WHO_HAS_DATA;

extern void handler_who_has(uint8_t *request, uint16_t request_len, bacnet_addr_t *src);

extern void Send_WhoHas_Object(int32_t low_limit, int32_t high_limit, BACNET_OBJECT_TYPE object_type,
                uint32_t object_instance);

extern void Send_WhoHas_Name(int32_t low_limit, int32_t high_limit, const char *object_name);

#ifdef __cplusplus
}
#endif

#endif  /* _WHOHAS_H_ */

