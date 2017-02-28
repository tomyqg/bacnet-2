/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * ao.h
 * Original Author:  linzhixian, 2015-2-2
 *
 * ao.h
 *
 * History
 */

#ifndef _AO_H_
#define _AO_H_

#include <stdint.h>
#include <stdbool.h>

#include "bacnet/bacstr.h"
#include "bacnet/object/object.h"
#include "misc/cJSON.h"
#include "bacnet/object/ai.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct object_ao_s {
    object_ai_t     base;
    float           relinquish_default;
    float           priority_array[BACNET_MAX_PRIORITY];
    uint16_t        priority_bits;
    uint8_t         active_bit;
} object_ao_t;

object_impl_t *object_create_impl_ao(void);

extern bool analog_output_present_value_set(object_ao_t *ao, float value, uint8_t priority);

extern bool analog_output_present_value_relinquish(object_ao_t *ao, uint8_t priority);

extern int analog_output_init(cJSON *object);

#ifdef __cplusplus
}
#endif

#endif  /* _AO_H_ */

