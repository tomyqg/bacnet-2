/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * msv.h
 * Original Author:  linzhixian, 2015-2-2
 *
 * Multi-state Value Object
 *
 * History
 */

#ifndef _MSV_H_
#define _MSV_H_

#include <stdint.h>
#include <stdbool.h>

#include "bacnet/bacstr.h"
#include "bacnet/object/mso.h"
#include "misc/cJSON.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef object_msi_t object_msv_t;

typedef object_msi_t object_msv_writable_t;

typedef object_mso_t object_msv_commandable_t;

object_impl_t *object_create_impl_msv(void);

object_impl_t *object_create_impl_msv_writable(void);

object_impl_t *object_create_impl_msv_commandable(void);

extern int multistate_value_init(cJSON *object);

#ifdef __cplusplus
}
#endif

#endif  /* _MSV_H_ */

