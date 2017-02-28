/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * msi.h
 * Original Author:  linzhixian, 2015-2-2
 *
 * Multi-state Input Object
 *
 * History
 */

#ifndef _MSI_H_
#define _MSI_H_

#include <stdint.h>
#include <stdbool.h>

#include "bacnet/bacstr.h"
#include "bacnet/object/object.h"
#include "misc/cJSON.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct object_msi_s {
    object_seor_t        base;
    uint32_t            present;
    uint32_t            number_of_states;
} object_msi_t;

object_impl_t *object_create_impl_msi(void);

extern int multistate_input_init(cJSON *object);

#ifdef __cplusplus
}
#endif

#endif  /* _MSI_H_ */

