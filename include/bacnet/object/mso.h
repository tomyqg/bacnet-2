/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * mso.h
 * Original Author:  linzhixian, 2015-2-2
 *
 * Multi-state Output Object
 *
 * History
 */

#ifndef _MSO_H_
#define _MSO_H_

#include <stdint.h>
#include <stdbool.h>

#include "bacnet/bacstr.h"
#include "bacnet/object/msi.h"
#include "misc/cJSON.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct object_mso_s {
    object_msi_t    base;
    uint32_t        relinquish_default;
    uint32_t        priority_array[16];
    uint16_t        priority_bits;
    uint8_t         active_bit;
} object_mso_t;

object_impl_t *object_create_impl_mso(void);

extern bool multistate_output_present_value_set(object_mso_t *bo, uint32_t value, uint8_t priority);

extern bool multistate_output_present_value_relinquish(object_mso_t *bo, uint8_t priority);

extern int multistate_output_init(cJSON *object);

#ifdef __cplusplus
}
#endif

#endif  /* _MSO_H_ */

