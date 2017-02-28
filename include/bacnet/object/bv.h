/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bv.h
 * Original Author:  linzhixian, 2015-2-2
 *
 * Binary Value Object
 *
 * History
 */

#ifndef _BV_H_
#define _BV_H_

#include <stdint.h>
#include <stdbool.h>

#include "bacnet/bacenum.h"
#include "bacnet/bacstr.h"
#include "bacnet/object/bo.h"
#include "misc/cJSON.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef object_bi_t object_bv_t;

typedef object_bi_t object_bv_writable_t;

typedef object_bo_t object_bv_commandable_t;

object_impl_t *object_create_impl_bv(void);

object_impl_t *object_create_impl_bv_writable(void);

object_impl_t *object_create_impl_bv_commandable(void);

extern int binary_value_init(cJSON *object);

#ifdef __cplusplus
}
#endif

#endif  /* _BV_H_ */

