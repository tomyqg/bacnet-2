/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * module_mng.c
 * Original Author:  linzhixian, 2015-7-7
 *
 * MODULE MANAGER
 *
 * History
 */

#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include "module_mng.h"
#include "module_mng_def.h"

static struct list_head module_head = {0};

bool module_mng_dbg_err = true;
bool module_mng_dbg_warn = true;
bool module_mng_dbg_verbos = true;

int module_mng_register(module_handler_t *handler)
{
    if (handler == NULL) {
        MODULE_MNG_ERROR("%s: null argument\r\n", __func__);
        return -EINVAL;
    }

    if (module_head.next == NULL) {
        MODULE_MNG_ERROR("%s: module_mng is not initialized\r\n", __func__);
        return -EPERM;;
    }

    if (handler->node.next != NULL || handler->node.prev != NULL) {
        MODULE_MNG_ERROR("%s: module %s already register\r\n", __func__,
            handler->name);
        return -EINVAL;
    }

    list_add_tail(&handler->node, &module_head);

    return OK;
}

int module_mng_unregister(module_handler_t *handler)
{
    if (handler == NULL) {
        MODULE_MNG_ERROR("%s: null argument\r\n", __func__);
        return -EINVAL;
    }

    if (module_head.next == NULL) {
        MODULE_MNG_ERROR("%s: module_mng is not initialized\r\n", __func__);
        return -EPERM;;
    }

    if (handler->node.next == NULL) {
        MODULE_MNG_WARN("%s: module %s already unregister\r\n", __func__,
            handler->name);
        return OK;;
    }
    
    list_del(&handler->node);

    return OK;
}

int module_mng_startup(void)
{
    module_handler_t *handler;
    int rv;
    
    if (module_head.next == NULL) {
        MODULE_MNG_ERROR("%s: module_mng is not initialized\r\n", __func__);
        return -EPERM;
    }

    list_for_each_entry(handler, &module_head, node) {
        if (handler->startup) {
            rv = handler->startup();
            if (rv < 0) {
                MODULE_MNG_ERROR("%s: module %s startup failed(%d)\r\n", __func__,
                    handler->name, rv);
                return rv;
            }
        }
    }

    module_mng_dbg_err = false;
    module_mng_dbg_warn = false;
    module_mng_dbg_verbos = false;

    return OK;
}

void module_mng_stop(void)
{
    module_handler_t *handler;

    if (module_head.next == NULL) {
        MODULE_MNG_ERROR("%s: module_mng is not initialized\r\n", __func__);
        return;
    }
    
    list_for_each_entry(handler, &module_head, node) {
        if (handler->stop) {
            handler->stop();
        }
    }
}

int module_mng_init(void)
{
    if (module_head.next != NULL) {
        MODULE_MNG_ERROR("%s: module_mng has been already inited\r\n", __func__);
        return -EPERM;
    }

    INIT_LIST_HEAD(&module_head);

    MODULE_MNG_VERBOS("%s: OK\r\n", __func__);
    
    return OK;
}

void module_mng_exit(void)
{
    if (module_head.next == NULL) {
        MODULE_MNG_ERROR("%s: module_mng has not been inited\r\n", __func__);
        return;
    }

    module_mng_stop();

    module_head.next = NULL;
    module_head.prev = NULL;
}

