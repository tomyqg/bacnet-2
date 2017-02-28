/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * datalink.c
 * Original Author:  linzhixian, 2014-7-9
 *
 * BACnet数据链路层驱动
 *
 * History
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "datalink_def.h"
#include "bacnet/mstp.h"
#include "bacnet/bip.h"
#include "bacnet/etherdl.h"
#include "debug.h"
#include "bacnet/bacnet.h"
#include "bacnet/bactext.h"

bool dl_dbg_err = true;
bool dl_dbg_warn = true;
bool dl_dbg_verbos = true;

/**
 * datalink_set_dbg_level - 设置调试状态
 *
 * @level: 调试开关
 *
 * @return: void
 *
 */
void datalink_set_dbg_level(uint32_t level)
{
    dl_dbg_verbos = level & DEBUG_LEVEL_VERBOS;
    dl_dbg_warn = level & DEBUG_LEVEL_WARN;
    dl_dbg_err = level & DEBUG_LEVEL_ERROR;
}

/**
 * datalink_show_dbg_status - 查看调试状态信息
 *
 * @return: void
 *
 */
void datalink_show_dbg_status(void)
{
    printf("datalink_dbg_verbos: %d\r\n", dl_dbg_verbos);
    printf("datalink_dbg_warn: %d\r\n", dl_dbg_warn);
    printf("datalink_dbg_err: %d\r\n", dl_dbg_err);
}

/**
 * datalink_port_create - 创建链路层对象
 *
 * @cfg: 配置信息
 *
 * @return: 成功返回链路层对象，失败返回NULL
 *
 */
datalink_base_t *datalink_port_create(cJSON *cfg, cJSON *res)
{
    datalink_base_t *dl;
    cJSON *dl_cfg;

    if (cfg == NULL || res == NULL) {
        DL_ERROR("%s: null argument\r\n", __func__);
        return NULL;
    }

    dl_cfg = cJSON_DetachItemFromObject(cfg, "dl_type");
    if (!dl_cfg) {
        DL_ERROR("%s: get dl_type item failed\r\n", __func__);
        return NULL;
    }

    if (dl_cfg->type != cJSON_String) {
        DL_ERROR("%s: dl_type item value is not string\r\n", __func__);
        cJSON_Delete(dl_cfg);
        return NULL;
    }

    dl = NULL;
    if (strcmp(dl_cfg->valuestring, "MSTP") == 0) {
	    dl = (datalink_base_t *)mstp_port_create(cfg, res);
	    if (!dl) {
	        DL_ERROR("%s: create mstp failed\r\n", __func__);
	    } else {
	        dl->type = DL_MSTP;
	    }
    } else if (strcmp(dl_cfg->valuestring, "BIP") == 0) {
	    dl = (datalink_base_t *)bip_port_create(cfg, res);
	    if (!dl) {
	        DL_ERROR("%s: create bip failed\r\n", __func__);
	    } else {
	        dl->type = DL_BIP;
	    }
    } else if (strcmp(dl_cfg->valuestring, "ETH") == 0) {
        dl = (datalink_base_t *)ether_port_create(cfg, res);
        if (!dl) {
            DL_ERROR("%s: create ether failed\r\n", __func__);
        } else {
            dl->type = DL_ETHERNET;
        }
    } else {
        DL_ERROR("%s: unsupported dl_type:(%s)\r\n", __func__, dl_cfg->valuestring);
    }
    
    cJSON_Delete(dl_cfg);
    return dl;
}

int datalink_port_delete(datalink_base_t *dl_port)
{
    int rv;
    
    if (dl_port == NULL) {
        DL_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    rv = -EPERM;
    
    switch (dl_port->type) {
    case DL_MSTP:
        rv = mstp_port_delete((datalink_mstp_t *)dl_port);
        break;

    case DL_BIP:
        rv = bip_port_delete((datalink_bip_t *)dl_port);
        break;

    case DL_ETHERNET:
        rv = ether_port_delete((datalink_ether_t *)dl_port);
        break;
    
    default:
        DL_ERROR("%s: unsupported dl_type(%d)\r\n", __func__, dl_port->type);
        break;
    }

    return rv;
}

const char *datalink_get_type_by_resource_name(cJSON *res, char *name)
{
    cJSON *resource;
    cJSON *value;
    
    if ((res == NULL) || (name == NULL)) {
        DL_ERROR("%s: null argument\r\n", __func__);
        return NULL;
    }

    resource = cJSON_GetObjectItem(res, name);
    if (resource == NULL) {
        return NULL;
    }
    
    if (resource->type != cJSON_Object) {
        DL_ERROR("%s: resource %s is not object\r\n", __func__, name);
        return NULL;
    }

    value = cJSON_GetObjectItem(resource, "type");
    if ((value == NULL) || (value->type != cJSON_String)) {
        DL_ERROR("%s: resource %s invalid type\r\n", __func__, name);
        return NULL;
    }

    return value->valuestring;
}

const char *datalink_get_ifname_by_resource_name(cJSON *res, char *name)
{
    cJSON *resource;
    cJSON *value;
    
    if ((res == NULL) || (name == NULL)) {
        DL_ERROR("%s: null argument\r\n", __func__);
        return NULL;
    }

    resource = cJSON_GetObjectItem(res, name);
    if (resource == NULL) {
        return NULL;
    }
    
    if (resource->type != cJSON_Object) {
        DL_ERROR("%s: resource %s is not object\r\n", __func__, name);
        return NULL;
    }

    value = cJSON_GetObjectItem(resource, "ifname");
    if ((value == NULL) || (value->type != cJSON_String)) {
        DL_ERROR("%s: resource %s invalid ifname\r\n", __func__, name);
        return NULL;
    }

    return value->valuestring;
}

int datalink_init(void)
{
    int rv;

    rv = mstp_init();
    if (rv < 0) {
        DL_ERROR("%s: mstp init failed(%d)\r\n", __func__, rv);
        goto err1;
    }

    rv = bip_init();
    if (rv < 0) {
        DL_ERROR("%s: bip init failed(%d)\r\n", __func__, rv);
        goto err2;
    }

    rv = ether_init();
    if (rv < 0) {
        DL_ERROR("%s: ether init failed(%d)\r\n", __func__, rv);
        goto err3;
    }

    DL_VERBOS("%s: OK\r\n", __func__);
    return OK;

err3:
    bip_exit();

err2:
    mstp_exit();

err1:
    return rv;
}

int datalink_startup(void)
{
    int rv;

    rv = mstp_startup();
    if (rv < 0) {
        DL_ERROR("%s: mstp startup failed(%d)\r\n", __func__, rv);
        goto err0;
    }

    rv = bip_startup();
    if (rv < 0) {
        DL_ERROR("%s: bip startup failed(%d)\r\n", __func__, rv);
        goto err1;
    }

    rv = ether_startup();
    if (rv < 0) {
        DL_ERROR("%s: ether startup failed(%d)\r\n", __func__, rv);
        goto err2;
    }
    
    datalink_set_dbg_level(0);

    return OK;

err2:
    bip_stop();

err1:
    mstp_stop();

err0:
    return rv;
}

void datalink_stop(void)
{
    ether_stop();
    bip_stop();
    mstp_stop();
}

void datalink_clean(void)
{
    ether_clean();
    bip_clean();
    mstp_clean();
}

void datalink_exit(void)
{
    datalink_stop();
    datalink_clean();
}

cJSON *datalink_get_status(connect_info_t *conn, cJSON *request)
{
    cJSON *reply, *tmp;
    char str[64];
    const char *reason;
    int error_code;
    
    if (request == NULL) {
        DL_ERROR("%s: invalid request\r\n", __func__);
        return NULL;
    }
    
    tmp = cJSON_GetObjectItem(request, "dl_type");
    if ((tmp == NULL) || (tmp->type != cJSON_String)) {
        DL_ERROR("%s: get dl_type item failed\r\n", __func__);
        error_code = -1;
        reason = "get dl_type item failed";
        goto err;
    }

    (void)bactext_tolower(tmp->valuestring, str, sizeof(str));
    if (strcmp(str, "bip") == 0) {
        reply = bip_get_status(request);
    } else if (strcmp(str, "mstp") == 0) {
        reply = mstp_get_status(request);
    } else if (strcmp(str, "ethernet") == 0) {
        reply = ether_get_status(request);
    } else {
        DL_ERROR("%s: invalid dl_type(%s)\r\n", __func__, tmp->valuestring);
        error_code = -1;
        reason = "invalid dl_type, dl_type should be:\r\n"
            "bip\r\n"
            "mstp\r\n"
            "ethernet\r\n";
        goto err;
    }

    if (!reply) {
        DL_ERROR("%s: unknown error\r\n", __func__);
        connect_mng_drop(conn);
        return NULL;
    }

    return reply;

err:
    reply = cJSON_CreateObject();
    if (reply == NULL) {
        DL_ERROR("%s: create reply object failed\r\n", __func__);
        connect_mng_drop(conn);
        return NULL;
    }

    cJSON_AddNumberToObject(reply, "error_code", error_code);
    cJSON_AddStringToObject(reply, "reason", reason);
    
    return reply;
}

cJSON *datalink_get_mib(datalink_base_t *dl_port)
{
    cJSON *result;

    if (dl_port == NULL) {
        DL_ERROR("%s: invalid argument\r\n", __func__);
        return NULL;
    }

    result = cJSON_CreateObject();
    if (result == NULL) {
        DL_ERROR("%s: create result object failed\r\n", __func__);
        return NULL;
    }

    cJSON_AddNumberToObject(result, "tx_all", dl_port->tx_all);
    cJSON_AddNumberToObject(result, "tx_ok", dl_port->tx_ok);
    cJSON_AddNumberToObject(result, "rx_all", dl_port->rx_all);
    cJSON_AddNumberToObject(result, "rx_ok", dl_port->rx_ok);

    return result;
}

