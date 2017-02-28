/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * debug.c
 * Original Author:  linzhixian, 2015-11-20
 *
 * Debug
 *
 * History
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "debug.h"
#include "debug_def.h"
#include "connect_mng.h"
#include "web_service.h"
#include "bacnet/app.h"
#include "bacnet/network.h"
#include "bacnet/datalink.h"
#include "bacnet/mstp.h"
#include "bacnet/bip.h"
#include "bacnet/etherdl.h"
#include "misc/cJSON.h"

static bool debug_service_status = false;

bool debug_dbg_err = true;
bool debug_dbg_warn = true;
bool debug_dbg_verbos = true;

void debug_print(void *data, uint32_t len)
{
    uint8_t *p;
    uint32_t ch;
    int i, j;
    
    if ((data == NULL) || (len == 0)) {
        return;
    }

    printf("\r\n-----------len[%d]-------------\r\n", len);
    
    j = 0;
    p = (uint8_t *)data;
    for (i = 0; i < len; i++, p++) {
        ch = (*p & 0xff);
        printf("[%02x] ", ch);
        
        j++;
        if (j % 16 == 0) {
            printf("\r\n");
        }
    }
    
    printf("\r\n------------------------------\r\n");

    return;
}

static bool debug_get_dbg_level(cJSON *cfg, uint32_t *level)
{
    cJSON *tmp;

    tmp = cJSON_GetObjectItem(cfg, "dbg_level");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        DEBUG_ERROR("%s: get dbg_level item failed\r\n", __func__);
        return false;
    }
    
    if ((tmp->valueint < MIN_DBG_LEVEL) || (tmp->valueint > MAX_DBG_LEVEL)) {
        DEBUG_ERROR("%s: invalid dbg_level(%d) failed\r\n", __func__, tmp->valueint);
        return false;
    }
    
    *level = (uint32_t)tmp->valueint;

    return true;
}

static bool debug_set_debug_dbg_level(cJSON *cfg)
{
    uint32_t level;

    level = 0;
    if (debug_get_dbg_level(cfg, &level)) {
        debug_dbg_verbos = level & DEBUG_LEVEL_VERBOS;
        debug_dbg_warn = level & DEBUG_LEVEL_WARN;
        debug_dbg_err = level & DEBUG_LEVEL_ERROR;
    }

    return false;
}

static bool debug_set_event_loop_status(cJSON *cfg)
{
    uint32_t level;

    level = 0;
    if (debug_get_dbg_level(cfg, &level)) {
        el_set_dbg_level(level);
    }

    return false;
}

static bool debug_set_connect_mng_dbg_level(cJSON *cfg)
{
    uint32_t level;

    level = 0;
    if (debug_get_dbg_level(cfg, &level)) {
        connect_mng_set_dbg_level(level);
    }

    return false;
}

static bool debug_set_web_dbg_level(cJSON *cfg)
{
    uint32_t level;

    level = 0;
    if (debug_get_dbg_level(cfg, &level)) {
        web_service_set_dbg_level(level);
    }

    return false;
}

static bool debug_set_app_dbg_level(cJSON *cfg)
{
    uint32_t level;

    level = 0;
    if (debug_get_dbg_level(cfg, &level)) {
        app_set_dbg_level(level);
    }

    return false;
}

static bool debug_set_network_dbg_level(cJSON *cfg)
{
    uint32_t level;

    level = 0;
    if (debug_get_dbg_level(cfg, &level)) {
        network_set_dbg_level(level);
    }

    return false;
}

static bool debug_set_datalink_dbg_level(cJSON *cfg)
{
    uint32_t level;

    level = 0;
    if (debug_get_dbg_level(cfg, &level)) {
        datalink_set_dbg_level(level);
    }

    return false;
}

static bool debug_set_mpsp_dbg_level(cJSON *cfg)
{
    uint32_t level;

    level = 0;
    if (debug_get_dbg_level(cfg, &level)) {
        mstp_set_dbg_level(level);
    }

    return false;
}

static bool debug_set_bip_dbg_level(cJSON *cfg)
{
    uint32_t level;

    level = 0;
    if (debug_get_dbg_level(cfg, &level)) {
        bip_set_dbg_level(level);
    }

    return false;
}

static bool debug_set_ethernet_dbg_level(cJSON *cfg)
{
    uint32_t level;

    level = 0;
    if (debug_get_dbg_level(cfg, &level)) {
        ether_set_dbg_level(level);
    }

    return false;
}

static bool debug_show_network_route_table(void)
{
    network_show_route_table();

    return false;
}

static bool debug_connect_service_handler(connect_info_t *conn)
{
    cJSON *cfg, *request;
    
    if (conn == NULL) {
        DEBUG_ERROR("%s: null argument\r\n", __func__);
        return true;
    }

    if ((conn->data == NULL) || (conn->data_len == 0)) {
        DEBUG_ERROR("%s: invalid argument\r\n", __func__);
        connect_mng_drop(conn);
        return true;
    }

    if (conn->data[conn->data_len - 1]) {
        DEBUG_ERROR("%s: request not null terminated\r\n", __func__);
        connect_mng_drop(conn);
        return true;
    }

    cfg = cJSON_Parse((char *)conn->data);
    if (cfg == NULL) {
        DEBUG_ERROR("%s: cJSON Parse failed\r\n", __func__);
        connect_mng_drop(conn);
        return true;
    }
    free(conn->data);
    conn->data = NULL;
    
    if (cfg->type != cJSON_Object) {
        DEBUG_ERROR("%s: invalid cfg type(%d)\r\n", __func__, cfg->type);
        goto out;
    }

    request = cJSON_GetObjectItem(cfg, "request");
    if ((request == NULL) || (request->type != cJSON_Number)) {
        DEBUG_ERROR("%s: get request item failed\r\n", __func__);
        goto out;
    }

    if ((request->valueint < 0) || (request->valueint >= MAX_DEBUG_SERVICE_CHOICE)) {
        DEBUG_ERROR("%s: invalid request(%lf)\r\n", __func__, request->valuedouble);
        goto out;
    }

    switch (request->valueint) {
    case DEBUG_SET_DEBUG_DBG_STATUS:
        debug_set_debug_dbg_level(cfg);
        break;

    case DEBUG_SET_EVENT_LOOP_STATUS:
        debug_set_event_loop_status(cfg);
        break;
        
    case DEBUG_SET_CONNECT_MNG_DBG_STATUS:
        debug_set_connect_mng_dbg_level(cfg);
        break;
        
    case DEBUG_SET_WEB_DBG_STATUS:
        debug_set_web_dbg_level(cfg);
        break;
    
    case DEBUG_SET_APP_DBG_STATUS:
        debug_set_app_dbg_level(cfg);
        break;
        
    case DEBUG_SET_NETWORK_DBG_STATUS:
        debug_set_network_dbg_level(cfg);
        break;
    
    case DEBUG_SET_DATALINK_DBG_STATUS:
        debug_set_datalink_dbg_level(cfg);
        break;
        
    case DEBUG_SET_MSTP_DBG_STATUS:
        debug_set_mpsp_dbg_level(cfg);
        break;
        
    case DEBUG_SET_BIP_DBG_STATUS:
        debug_set_bip_dbg_level(cfg);
        break;
        
    case DEBUG_SET_ETHERNET_DBG_STATUS:
        debug_set_ethernet_dbg_level(cfg);
        break;
        
    case DEBUG_SHOW_NETWORK_ROUTE_TABLE:
        debug_show_network_route_table();
        break;

    default:
        DEBUG_ERROR("%s: unknown request(%lf)\r\n", __func__, request->valuedouble);
        goto out;
    }

    cJSON_Delete(cfg);
    conn->data_len = 0;
    (void)connect_mng_echo(conn);
    return true;

out:
    cJSON_Delete(cfg);

    (void)connect_mng_drop(conn);
    return true;
}

int debug_service_init(void)
{
    int rv;

    if (debug_service_status == true) {
        return OK;
    }
    
    rv = connect_mng_register(BACNET_DEBUG_SERVICE, debug_connect_service_handler);
    if (rv < 0) {
        DEBUG_ERROR("%s: connect_mng register failed(%d)\r\n", __func__, rv);
        return rv;
    }

    debug_service_status = true;
    debug_dbg_err = false;
    debug_dbg_warn = false;
    debug_dbg_verbos = false;

    return OK;
}

void debug_service_exit(void)
{
    if (debug_service_status == false) {
        return;
    }

    (void)connect_mng_unregister(BACNET_DEBUG_SERVICE);
    
    debug_service_status = false;
}

