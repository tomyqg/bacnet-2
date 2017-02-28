/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * datalink_def.h
 * Original Author:  linzhixian, 2014-11-5
 *
 * BACnet数据链路层内部头文件
 *
 * History
 */

#ifndef _DATALINK_DEF_H_
#define _DATALINK_DEF_H_

#include <stdio.h>
#include <stdbool.h>
#include <net/if.h>

extern bool dl_dbg_err;
extern bool dl_dbg_warn;
extern bool dl_dbg_verbos;

#define DL_ERROR(fmt, args...)                      \
do {                                                \
    if (dl_dbg_err) {                               \
       printf(fmt, ##args);                         \
    }                                               \
} while (0)

#define DL_WARN(fmt, args...)                       \
do {                                                \
    if (dl_dbg_warn) {                              \
        printf(fmt, ##args);                        \
    }                                               \
} while (0)

#define DL_VERBOS(fmt, args...)                     \
do {                                                \
    if (dl_dbg_verbos) {                            \
       printf(fmt, ##args);                         \
    }                                               \
} while (0)

#endif  /* _DATALINK_DEF_H_ */

