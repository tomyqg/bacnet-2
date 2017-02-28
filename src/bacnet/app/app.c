/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * app.c
 * Original Author:  linzhixian, 2015-4-3
 *
 * APP
 *
 * History
 */

#include <errno.h>

#include "bacnet/app.h"
#include "bacnet/addressbind.h"
#include "bacnet/object/device.h"
#include "bacnet/bacnet.h"
#include "bacnet/tsm.h"
#include "module_mng.h"
#include "debug.h"

bool is_app_exist = false;

bool app_dbg_verbos = true;
bool app_dbg_warn = true;
bool app_dbg_err = true;

static module_handler_t module = {
    .name = "app",
    .startup = app_startup,
    .stop = app_stop,
};

int app_init(void)
{
    int rv;
    
    rv = module_mng_register(&module);
    if (rv < 0) {
        APP_ERROR("%s: module register failed(%d)\r\n", __func__, rv);
        return rv;
    }

    APP_VERBOS("%s: OK\r\n", __func__);
    return OK;
}

int app_startup(void)
{
    cJSON *app_cfg, *tmp;
    int rv;

    app_cfg = bacnet_get_app_cfg();
    if (app_cfg == NULL) {
        APP_ERROR("%s: get app cfg failed\r\n", __func__);
        return -EPERM;
    }

    rv = OK;
    if (!cJSON_HasAnyItem(app_cfg)) {
        APP_WARN("%s: This device is in Routing Mode\r\n", __func__);
        is_app_exist = false;
        goto out0;
    }

    tmp = cJSON_GetObjectItem(app_cfg, "TSM");
    if ((tmp != NULL) && (tmp->type != cJSON_Object)) {
        APP_ERROR("%s: get TSM item failed\r\n", __func__);
        rv = -EPERM;
        goto out0;
    } else if (tmp == NULL) {
        tmp = cJSON_CreateObject();
        cJSON_AddItemToObject(app_cfg, "TSM", tmp);
    }

    rv = tsm_init(tmp);
    if (rv < 0) {
        APP_ERROR("%s: tsm inited failed(%d)\r\n", __func__, rv);
        goto out0;
    }

    tmp = cJSON_GetObjectItem(app_cfg, "Address_Binding");
    if ((tmp != NULL) && (tmp->type != cJSON_Object)) {
        APP_ERROR("%s: get Address_Binding item failed\r\n", __func__);
        rv = -EPERM;
        goto out1;
    } else if (tmp == NULL) {
        tmp = cJSON_CreateObject();
        cJSON_AddItemToObject(app_cfg, "Address_Binding", tmp);
    }

    rv = address_init(tmp);
    if (rv < 0) {
        APP_ERROR("%s: address startup failed(%d)\r\n", __func__, rv);
        goto out1;
    }

    rv = object_init(app_cfg);
    if (rv < 0) {
        APP_ERROR("%s: object init failed(%d)\r\n", __func__, rv);
        goto out2;
    }

    is_app_exist = true;
    app_set_dbg_level(0);
    goto out0;

out2:
    address_exit();

out1:
    tsm_exit();

out0:
    cJSON_Delete(app_cfg);

    return rv;
}

void app_stop(void)
{
    is_app_exist = false;
    
    return;
}

void app_exit(void)
{
    app_stop();

    return;
}

void app_set_dbg_level(uint32_t level)
{
    app_dbg_verbos = level & DEBUG_LEVEL_VERBOS;
    app_dbg_warn = level & DEBUG_LEVEL_WARN;
    app_dbg_err = level & DEBUG_LEVEL_ERROR;
}

cJSON *app_get_status(connect_info_t *conn, cJSON *request)
{
    /* TODO */
    return cJSON_CreateObject();
}

