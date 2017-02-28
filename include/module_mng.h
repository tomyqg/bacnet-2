/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * module_mng.h
 * Original Author:  linzhixian, 2015-7-7
 *
 * Module Manager
 *
 * History
 */

#ifndef _MODULE_MNG_H_
#define _MODULE_MNG_H_

#include "misc/cJSON.h"
#include "misc/list.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct module_handler_s {
    struct list_head node;
    const char *name;
    int (*startup)(void);
    void (*stop)(void);
} module_handler_t;

extern int module_mng_register(module_handler_t *handler);

extern int module_mng_unregister(module_handler_t *handler);

extern int module_mng_init(void);

extern int module_mng_startup(void);

extern void module_mng_stop(void);

extern void module_mng_exit(void);

#ifdef __cplusplus
}
#endif

#endif  /*_MODULE_MNG_H_ */

