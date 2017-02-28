/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * app.h
 * Original Author:  linzhixian, 2015-4-3
 *
 * app.h
 *
 * History
 */

#ifndef _APP_H_
#define _APP_H_

#include <stdint.h>
#include <stdbool.h>

#include "misc/cJSON.h"
#include "connect_mng.h"

#ifdef __cplusplus
extern "C"
{
#endif

extern bool app_dbg_err;
extern bool app_dbg_warn;
extern bool app_dbg_verbos;

#define APP_ERROR(fmt, args...)                     \
do {                                                \
    if (app_dbg_err) {                              \
        printf(fmt, ##args);                        \
    }                                               \
} while (0)

#define APP_WARN(fmt, args...)                      \
do {                                                \
    if (app_dbg_warn) {                             \
        printf(fmt, ##args);                        \
    }                                               \
} while (0)

#define APP_VERBOS(fmt, args...)                    \
do {                                                \
    if (app_dbg_verbos) {                           \
        printf(fmt, ##args);                        \
    }                                               \
} while (0)


extern int app_startup(void);

extern void app_stop(void);

extern int app_init(void);

extern void app_exit(void);

extern void app_set_dbg_level(uint32_t level);

extern cJSON *app_get_status(connect_info_t *conn, cJSON *request);

#ifdef __cplusplus
}
#endif

#endif /* _APP_H_ */

