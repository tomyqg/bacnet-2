/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * debug_def.h
 * Original Author:  linzhixian, 2015-12-7
 *
 * Debug Service
 *
 * History
 */

#ifndef _DEBUG_DEF_H_
#define _DEBUG_DEF_H_

extern bool debug_dbg_err;
extern bool debug_dbg_warn;
extern bool debug_dbg_verbos;

#define DEBUG_ERROR(fmt, args...)                   \
do {                                                \
    if (debug_dbg_err) {                            \
        printf(fmt, ##args);                        \
    }                                               \
} while (0)
 
#define DEBUG_WARN(fmt, args...)                    \
do {                                                \
    if (debug_dbg_warn) {                           \
        printf(fmt, ##args);                        \
    }                                               \
} while (0)
 
#define DEBUG_VERBOS(fmt, args...)                  \
do {                                                \
    if (debug_dbg_verbos) {                         \
        printf(fmt, ##args);                        \
    }                                               \
} while (0)

#ifndef OK
#define OK                                          (0)
#endif

#endif /* _DEBUG_DEF_H_ */

