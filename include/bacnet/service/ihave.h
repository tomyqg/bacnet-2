/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * ihave.h
 * Original Author:  linzhixian, 2015-2-11
 *
 * I Have Request Service
 *
 * History
 */

#ifndef _IHAVE_H_
#define _IHAVE_H_

#include <stdint.h>

#include "bacnet/bacenum.h"
#include "bacnet/bacapp.h"
#include "bacnet/bacstr.h"
#include "bacnet/bacnet_buf.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct BACnet_I_Have_Data {
    uint32_t device_id;
    BACNET_OBJECT_ID object_id;
    BACNET_CHARACTER_STRING object_name;
} BACNET_I_HAVE_DATA;

extern void handler_i_have(uint8_t *request, uint16_t request_len, bacnet_addr_t *src);

extern void Send_I_Have(BACNET_OBJECT_TYPE object_type, uint32_t object_instance,
                BACNET_CHARACTER_STRING *object_name);

#ifdef __cplusplus
}
#endif

#endif  /* _IHAVE_H_ */

