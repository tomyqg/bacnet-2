/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * ai.h
 * Original Author:  linzhixian, 2015-2-2
 *
 * ai.h
 *
 * History
 */

#ifndef _AI_H_
#define _AI_H_

#include <stdint.h>
#include <stdbool.h>

#include "bacnet/bacenum.h"
#include "bacnet/bacstr.h"
#include "bacnet/object/object.h"
#include "misc/cJSON.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct object_ai_s {
    object_seor_t        base;
    float               present;
    BACNET_ENGINEERING_UNITS    units;
} object_ai_t;

object_impl_t *object_create_impl_ai(void);

extern int analog_input_init(cJSON *object);

#ifdef __cplusplus
}
#endif

#endif  /* _AI_H_ */

