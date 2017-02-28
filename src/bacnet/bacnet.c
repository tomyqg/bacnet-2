/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bacnet.c
 * Original Author:  linzhixian, 2014-10-21
 *
 *
 * History
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include "bacnet/bacnet.h"
#include "bacnet/network.h"
#include "bacnet/app.h"
#include "bacnet/bacdef.h"
#include "misc/eventloop.h"
#include "misc/utils.h"
#include "module_mng.h"

static bool bacnet_init_status = false;

cJSON *__attribute__((weak)) bacnet_get_app_cfg(void)
{
    cJSON *cfg;

    cfg = load_json_file(BACNET_APP_CONFIG_FILE);
    if (cfg == NULL) {
        cfg = load_json_file("/etc/"BACNET_APP_CONFIG_FILE);
        if (cfg == NULL) {
            APP_ERROR("%s: load %s failed\r\n", __func__, BACNET_APP_CONFIG_FILE);
            return NULL;
        }
    }
    
    if (cfg->type != cJSON_Object) {
        APP_ERROR("%s: invalid cJSON type\r\n", __func__);
        cJSON_Delete(cfg);
        return NULL;
    }

    return cfg;
}

cJSON *__attribute__((weak)) bacnet_get_network_cfg(void)
{
    cJSON *cfg;
    
    cfg = load_json_file(BACNET_NETWORK_CONFIG_FILE);
    if (cfg == NULL) {
        cfg = load_json_file("/etc/"BACNET_NETWORK_CONFIG_FILE);
        if (cfg == NULL) {
            APP_ERROR("%s: load %s failed\r\n", __func__, BACNET_NETWORK_CONFIG_FILE);
            return NULL;
        }
    }

    if (cfg->type != cJSON_Object) {
        APP_ERROR("%s: invalid cJSON type\r\n", __func__);
        cJSON_Delete(cfg);
        return NULL;
    }

    return cfg;
}

cJSON *__attribute__((weak)) bacnet_get_resource_cfg(void)
{
    cJSON *cfg;

    cfg = load_json_file(BACNET_RESOURCE_CONFIG_FILE);
    if (cfg == NULL) {
        cfg = load_json_file("/etc/"BACNET_RESOURCE_CONFIG_FILE);
        if (cfg == NULL) {
            APP_ERROR("%s: load %s failed\r\n", __func__, BACNET_RESOURCE_CONFIG_FILE);
            return NULL;
        }
    }

    if (cfg->type != cJSON_Object) {
        APP_ERROR("%s: invalid cJSON type\r\n", __func__);
        cJSON_Delete(cfg);
        return NULL;
    }
    
    return cfg;
}

int bacnet_init(void)
{
    int rv;

    signal(SIGPIPE, SIG_IGN);

    if (bacnet_init_status == true) {
        return OK;
    }

    rv = el_loop_init(&el_default_loop);
    if (rv < 0) {
        APP_ERROR("%s: run event loop failed(%d)\r\n", __func__, rv);
        goto out0;
    }
    
    rv = module_mng_init();
    if (rv < 0) {
        APP_ERROR("%s: module manager init failed(%d)\r\n", __func__, rv);
        goto out1;
    }
    
    rv = network_init();
    if (rv < 0) {
        APP_ERROR("%s: network init failed(%d)\r\n", __func__, rv);
        goto out2;
    }

    rv = app_init();
    if (rv < 0) {
        APP_ERROR("%s: app init failed(%d)\r\n", __func__, rv);
        goto out3;
    }

    rv = module_mng_startup();
    if (rv < 0) {
        APP_ERROR("%s: module manager run failed(%d)\r\n", __func__, rv);
        goto out4;
    }

    bacnet_init_status = true;
    APP_VERBOS("%s: OK\r\n", __func__);
    return OK;
    
out4:
    app_exit();

out3:
    network_exit();

out2:
    module_mng_exit();

out1:
    el_loop_exit(&el_default_loop);

out0:

    return rv;
}

void bacnet_exit(void)
{
    if (bacnet_init_status == false) {
        return;
    }

    module_mng_exit();

    el_loop_exit(&el_default_loop);

    bacnet_init_status = false;
}

