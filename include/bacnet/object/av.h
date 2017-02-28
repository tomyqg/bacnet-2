/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * av.h
 * Original Author:  linzhixian, 2015-2-2
 *
 * av.h
 *
 * History
 */

#ifndef _AV_H_
#define _AV_H_

#include <stdint.h>
#include <stdbool.h>

#include "bacnet/bacstr.h"
#include "bacnet/object/object.h"
#include "bacnet/object/ai.h"
#include "bacnet/object/ao.h"
#include "bacnet/bacdef.h"
#include "misc/cJSON.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef object_ai_t object_av_t;

typedef object_ai_t object_av_writable_t;

typedef object_ao_t object_av_commandable_t;

extern object_impl_t *object_create_impl_av(void);

extern object_impl_t *object_create_impl_av_writable(void);

extern object_impl_t *object_create_impl_av_commandable(void);

extern int analog_value_init(cJSON *object);

#ifdef __cplusplus
}
#endif

#endif  /* _AV_H_ */

