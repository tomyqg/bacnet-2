/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * module_mng_def.h
 * Original Author:  linzhixian, 2015-7-7
 *
 * Module Manager
 *
 * History
 */

#ifndef _MODULE_MNG_DEF_H_
#define _MODULE_MNG_DEF_H_

#include <stdio.h>
#include <stdbool.h>

extern bool module_mng_dbg_err;
extern bool module_mng_dbg_warn;
extern bool module_mng_dbg_verbos;

#define MODULE_MNG_ERROR(fmt, args...)              \
do {                                                \
    if (module_mng_dbg_err) {                       \
        printf(fmt, ##args);                        \
    }                                               \
} while (0)

#define MODULE_MNG_WARN(fmt, args...)               \
do {                                                \
    if (module_mng_dbg_warn) {                      \
        printf(fmt, ##args);                        \
    }                                               \
} while (0)

#define MODULE_MNG_VERBOS(fmt, args...)             \
do {                                                \
    if (module_mng_dbg_verbos) {                    \
        printf(fmt, ##args);                        \
    }                                               \
} while (0)

#ifndef OK
#define OK                  (0)
#endif

#endif /* _MODULE_MNG_DEF_H_ */

