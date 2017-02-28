/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * config_app.c
 * Original Author:  linzhixian, 2016-5-16
 *
 * Config Application
 *
 * History
 */

#include <stdint.h>
#include <errno.h>

#include "config_app.h"
#include "bacnet/bacnet.h"
#include "bacnet/bacdef.h"
#include "webui.h"
#include "translate.h"
#include "misc/utils.h"

static pthread_rwlock_t app_cfg_rwlock;

static cJSON *app_cfg = NULL;

static const char *conf_app_set_tsm(cJSON *new_cfg, cJSON *old_cfg, const char *locale)
{
    cJSON *tmp_1, *tmp_2;

    tmp_1 = cJSON_GetObjectItem(new_cfg, "Max_Peer");
    if (tmp_1) {
        if (tmp_1->type != cJSON_Number) {
            return "Invalid Max_Peer item type";
        }
        if (tmp_1->valueint < 0) {
            return TLT("Invalid Max_Peer item value");
        }

        tmp_2 = cJSON_GetObjectItem(old_cfg, "Max_Peer");
        if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Number)) {
            return "Invalid original Max_Peer item type";
        }

        tmp_2->valueint = tmp_1->valueint;
        tmp_2->valuedouble = tmp_1->valueint;
    }

    tmp_1 = cJSON_GetObjectItem(new_cfg, "Max_Invoker");
    if (tmp_1) {
        if (tmp_1->type != cJSON_Number) {
            return "Invalid Max_Invoker item type";
        }
        if (tmp_1->valueint < 0) {
            return TLT("Invalid Max_Invoker item value");
        }

        tmp_2 = cJSON_GetObjectItem(old_cfg, "Max_Invoker");
        if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Number)) {
            return "Invalid original Max_Invoker item type";
        }

        tmp_2->valueint = tmp_1->valueint;
        tmp_2->valuedouble = tmp_1->valueint;
    }

    tmp_1 = cJSON_GetObjectItem(new_cfg, "No_Ack_Recycle_Timeout");
    if (tmp_1) {
        if (tmp_1->type != cJSON_Number) {
            return "Invalid No_Ack_Recycle_Timeout item type";
        }
        if (tmp_1->valueint < 0) {
            return TLT("Invalid No_Ack_Recycle_Timeout item value");
        }

        tmp_2 = cJSON_GetObjectItem(old_cfg, "No_Ack_Recycle_Timeout");
        if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Number)) {
            return "Invalid original No_Ack_Recycle_Timeout item type";
        }

        tmp_2->valueint = tmp_1->valueint;
        tmp_2->valuedouble = tmp_1->valueint;
    }

    tmp_1 = cJSON_GetObjectItem(new_cfg, "APDU_Timeout");
    if (tmp_1) {
        if (tmp_1->type != cJSON_Number) {
            return "Invalid APDU_Timeout item type";
        }
        if (tmp_1->valueint < 0) {
            return TLT("Invalid APDU_Timeout item value");
        }

        tmp_2 = cJSON_GetObjectItem(old_cfg, "APDU_Timeout");
        if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Number)) {
            return "Invalid original APDU_Timeout item type";
        }

        tmp_2->valueint = tmp_1->valueint;
        tmp_2->valuedouble = tmp_1->valueint;
    }

    tmp_1 = cJSON_GetObjectItem(new_cfg, "APDU_Retries");
    if (tmp_1) {
        if (tmp_1->type != cJSON_Number) {
            return "Invalid APDU_Retries item type";
        }
        if (tmp_1->valueint < 0) {
            return TLT("Invalid APDU_Retries item value");
        }

        tmp_2 = cJSON_GetObjectItem(old_cfg, "APDU_Retries");
        if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Number)) {
            return "Invalid original APDU_Retries item type";
        }

        tmp_2->valueint = tmp_1->valueint;
        tmp_2->valuedouble = tmp_1->valueint;
    }

    tmp_1 = cJSON_GetObjectItem(new_cfg, "Max_APDU_Cache");
    if (tmp_1) {
        if (tmp_1->type != cJSON_Number) {
            return "Invalid Max_APDU_Cache item type";
        }
        if (tmp_1->valueint < 0) {
            return TLT("Invalid Max_APDU_Cache item value");
        }

        tmp_2 = cJSON_GetObjectItem(old_cfg, "Max_APDU_Cache");
        if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Number)) {
            return "Invalid original Max_APDU_Cache item type";
        }

        tmp_2->valueint = tmp_1->valueint;
        tmp_2->valuedouble = tmp_1->valueint;
    }

    return NULL;
}

static const char *__conf_app_set_cfg(cJSON *new_cfg, const char *locale)
{
    cJSON *tmp_1, *tmp_2;
    const char *msg;

    msg = NULL;

    RWLOCK_WRLOCK(&app_cfg_rwlock);
    
    tmp_1 = cJSON_GetObjectItem(new_cfg, "TSM");
    if (tmp_1) {
        if (tmp_1->type != cJSON_Object) {
            msg = "Invalid TSM item type";
            goto out0;
        }

        tmp_2 = cJSON_GetObjectItem(app_cfg, "TSM");
        if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Object)) {
            msg = "Invalid original TSM item type";
            goto out0;
        }

        msg = conf_app_set_tsm(tmp_1, tmp_2, locale);
        if (msg) {
            goto out0;
        }
    }

    msg = NULL;
    
out0:
    RWLOCK_UNLOCK(&app_cfg_rwlock);
    
    return msg;
}

static void conf_app_set_cfg(const struct mg_request_info *ri, struct mg_connection *conn,
                const char *locale)
{
    cJSON *cfg;
    char post_data[8192];
    int post_data_len;
    const char *msg;
    
    if (strcmp(ri->request_method, "POST")) {
        errResp(conn, "This entity only supports POST method");
        return;
    }
    
    post_data_len = mg_read(conn, post_data, sizeof(post_data));
    if (post_data_len == sizeof(post_data)) {
        errResp(conn, "Too long post");
        return;
    }
    post_data[post_data_len] = 0;

    cfg = cJSON_Parse(post_data);
    if (cfg == NULL) {
        errResp(conn, "Invalid json string");
        return;
    }

    msg = __conf_app_set_cfg(cfg, locale);

    jsonErrResp(conn, msg);
    
    cJSON_Delete(cfg);

    return;
}

static void conf_app_get_cfg(const struct mg_request_info *ri, struct mg_connection *conn,
                const char *locale)
{
    cJSON *new_cfg;
    
    if (strcmp(ri->request_method, "GET")) {
        errResp(conn, "This entity only supports GET method");
        return;
    }

    RWLOCK_RDLOCK(&app_cfg_rwlock);
    new_cfg = cJSON_Duplicate(app_cfg, 1);
    RWLOCK_UNLOCK(&app_cfg_rwlock);

    if (new_cfg == NULL) {
        errResp(conn, "Duplicate app_cfg failed");
        return;
    }

    jsonResp(conn, new_cfg);
    cJSON_Delete(new_cfg);
    
    return;
}

static cJSON *conf_app_get_default_cfg(void)
{
    cJSON *default_cfg, *tmp;

    default_cfg = cJSON_CreateObject();
    if (default_cfg == NULL) {
        printf("%s: create default app_conf json failed\r\n", __func__);
        return NULL;
    }

    cJSON_AddTrueToObject(default_cfg, "Client_Device");

    tmp = cJSON_CreateObject();
    if (tmp == NULL) {
        printf("%s: create TSM json failed\r\n", __func__);
        goto out0;
    }
    
    cJSON_AddNumberToObject(tmp, "Max_Peer", 1024);
    cJSON_AddNumberToObject(tmp, "Max_Invoker", 2048);
    cJSON_AddNumberToObject(tmp, "No_Ack_Recycle_Timeout", 60000);
    cJSON_AddNumberToObject(tmp, "APDU_Timeout", 10000);
    cJSON_AddNumberToObject(tmp, "APDU_Retries", 3);
    cJSON_AddNumberToObject(tmp, "Max_APDU_Cache", 2048);
    cJSON_AddItemToObject(default_cfg, "TSM", tmp);

    return default_cfg;

out0:
    cJSON_Delete(default_cfg);

    return NULL;
}

int saveAppCfg(void)
{
    cJSON *new_cfg;
    int rv;
    
    RWLOCK_RDLOCK(&app_cfg_rwlock);
    new_cfg = cJSON_Duplicate(app_cfg, 1);
    RWLOCK_UNLOCK(&app_cfg_rwlock);

    if (new_cfg == NULL) {
        printf("%s: Duplicate app_cfg failed\r\n", __func__);
        return -EPERM;
    }

    rv = save_json_file(new_cfg, BACNET_APP_CONFIG_FILE);
    cJSON_Delete(new_cfg);
    if (rv < 0) {
        printf("%s: save json file failed(%d)\r\n", __func__, rv);
    }

    return rv;
}

int initAppCfg(void)
{
    cJSON *cfg;
    
    if (pthread_rwlock_init(&app_cfg_rwlock, NULL)) {
        printf("%s: app_cfg_rwlock init failed\r\n", __func__);
        return -EPERM;
    }

    app_cfg = conf_app_get_default_cfg();
    if (app_cfg == NULL) {
        printf("%s: get default app_cfg failed\r\n", __func__);
        return -EPERM;
    }

    cfg = bacnet_get_app_cfg();
    if (cfg) {
        if (__conf_app_set_cfg(cfg, NULL)) {
            printf("%s: set app_cfg failed\r\n", __func__);
            cJSON_Delete(cfg);
            goto out0;
        }
    }

    (void)reg_handler("app_set_cfg", conf_app_set_cfg);
    (void)reg_handler("app_get_cfg", conf_app_get_cfg);

    return OK;

out0:
    cJSON_Delete(app_cfg);

    (void)pthread_rwlock_destroy(&app_cfg_rwlock);

    return -EPERM;
}

