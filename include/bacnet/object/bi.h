/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bi.h
 * Original Author:  linzhixian, 2015-2-2
 *
 * Binary Input Object
 *
 * History
 */

#ifndef _BI_H_
#define _BI_H_

#include <stdint.h>
#include <stdbool.h>

#include "bacnet/bacstr.h"
#include "bacnet/object/object.h"
#include "misc/cJSON.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct object_bi_t {
    object_seor_t        base;
    BACNET_BINARY_PV    present;
    BACNET_POLARITY     polarity;
} object_bi_t;

object_impl_t *object_create_impl_bi(void);

extern int binary_input_init(cJSON *object);

#ifdef __cplusplus
}
#endif

#endif  /* _BI_H_ */

