/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bo.h
 * Original Author:  linzhixian, 2015-2-2
 *
 * Binary Output Object
 *
 * History
 */

#ifndef _BO_H_
#define _BO_H_

#include <stdint.h>
#include <stdbool.h>

#include "bacnet/bacenum.h"
#include "bacnet/bacstr.h"
#include "bacnet/object/bi.h"
#include "misc/cJSON.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct object_bo_s {
    object_bi_t     base;
    BACNET_BINARY_PV    relinquish_default;
    BACNET_BINARY_PV    priority_array[16];
    uint16_t        priority_bits;
    uint8_t         active_bit;
} object_bo_t;

object_impl_t *object_create_impl_bo(void);

extern bool binary_output_present_value_set(object_bo_t *bo, BACNET_BINARY_PV value, uint8_t priority);

extern bool binary_output_present_value_relinquish(object_bo_t *bo, uint8_t priority);

extern int binary_output_init(cJSON *object);

#ifdef __cplusplus
}
#endif

#endif  /* _BO_H_ */

