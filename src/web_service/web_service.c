/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * web_service.c
 * Original Author:  linzhixian, 2015-9-14
 *
 * Web Service
 *
 * History
 */

#include <stdlib.h>
#include <errno.h>

#include "web_request.h"
#include "web_ack.h"
#include "web_def.h"
#include "connect_mng.h"
#include "debug.h"
#include "bacnet/bactext.h"
#include "bacnet/app.h"
#include "bacnet/network.h"
#include "bacnet/datalink.h"

static web_service_table_t WEB_SERVICE_TABLE;

static bool web_service_status = false;

bool web_dbg_err = true;
bool web_dbg_warn = true;
bool web_dbg_verbos = true;

static uint32_t BKDRHash(const char *str, uint32_t len)
{
    uint32_t seed = 131;    /* 31 131 1313 13131 131313 etc.. */
    uint32_t hash = 0;
    uint32_t i;

    for (i = 0; i < len; str++, i++) {
        hash = (hash * seed) + (*str);
    }
    
    return hash;
}

static web_service_node_t *__web_service_table_find(char *choice)
{
    web_service_node_t *node;
    char str[64];

    (void)bactext_tolower(choice, str, sizeof(str));
    hash_for_each_possible(WEB_SERVICE_TABLE.name, node, h_node, BKDRHash(str, strlen(str))) {
        if (strcmp(node->choice, str) == 0) {
            return node;
        }
    }

    return NULL;
}

static web_service_node_t *web_service_table_find(char *choice)
{
    web_service_node_t *node;
    
    RWLOCK_RDLOCK(&WEB_SERVICE_TABLE.rwlock);

    node = __web_service_table_find(choice);
    
    RWLOCK_UNLOCK(&WEB_SERVICE_TABLE.rwlock);

    return node;
}

static int web_service_table_add(web_service_node_t *node)
{
    web_service_node_t *old;

    RWLOCK_WRLOCK(&WEB_SERVICE_TABLE.rwlock);

    old = __web_service_table_find((char *)node->choice);
    if (old) {
        WEB_WARN("%s: choice %s is already registered\r\n", __func__, node->choice);
        old->handler = node->handler;
    } else {
        hash_add(WEB_SERVICE_TABLE.name, &node->h_node, BKDRHash(node->choice, strlen(node->choice)));
        WEB_SERVICE_TABLE.count++;
    }

    RWLOCK_UNLOCK(&WEB_SERVICE_TABLE.rwlock);

    return OK;
}

static web_service_node_t *web_service_table_detach(char *choice)
{
    web_service_node_t *node;
    
    RWLOCK_WRLOCK(&WEB_SERVICE_TABLE.rwlock);

    node = __web_service_table_find(choice);
    if (node) {
        hash_del(&node->h_node);
        WEB_SERVICE_TABLE.count--;
    }
    
    RWLOCK_UNLOCK(&WEB_SERVICE_TABLE.rwlock);

    return node;
}

static void web_service_table_destroy(void)
{
    web_service_node_t *node;
    struct hlist_node *tmp;
    uint32_t bkt;

    RWLOCK_WRLOCK(&WEB_SERVICE_TABLE.rwlock);
    
    hash_for_each_safe(WEB_SERVICE_TABLE.name, bkt, node, tmp, h_node) {
        hash_del(&node->h_node);
        free(node);
    }

    WEB_SERVICE_TABLE.count = 0;

    RWLOCK_UNLOCK(&WEB_SERVICE_TABLE.rwlock);
    
    return;
}

static cJSON *web_list_method(connect_info_t *conn, cJSON *request)
{
    int i;
    cJSON *reply;
    web_service_node_t *node;

    reply = cJSON_CreateObject();
    if (reply == NULL) {
        WEB_ERROR("%s: create reply object failed\r\n", __func__);
        connect_mng_drop(conn);
        return NULL;
    }

    cJSON *result = cJSON_CreateArray();
    
    RWLOCK_RDLOCK(&WEB_SERVICE_TABLE.rwlock);
    hash_for_each (WEB_SERVICE_TABLE.name, i, node, h_node) {
        cJSON_AddItemToArray(result, cJSON_CreateString(node->choice));
    }
    RWLOCK_UNLOCK(&WEB_SERVICE_TABLE.rwlock);

    cJSON_AddItemToObject(reply, "result", result);
    
    return reply;
}

static int web_service_table_init(void)
{
    int rv;
    
    hash_init(WEB_SERVICE_TABLE.name);
    WEB_SERVICE_TABLE.count = 0;
    rv = pthread_rwlock_init(&(WEB_SERVICE_TABLE.rwlock), NULL);
    if (rv) {
        WEB_ERROR("%s: rwlock init failed(%d)\r\n", __func__, rv);
        return -EPERM;
    }

    return OK;
}

static void web_service_table_exit(void)
{
    web_service_table_destroy();
    (void)pthread_rwlock_destroy(&(WEB_SERVICE_TABLE.rwlock));
}

static bool web_service_connect_handler(connect_info_t *conn)
{
    cJSON *request, *reply, *method;
    web_service_node_t *node;
    
    if (conn == NULL) {
        WEB_ERROR("%s: null argument\r\n", __func__);
        return true;
    }

    if (!conn->data_len || !conn->data) {
        WEB_ERROR("%s: invalid argument\r\n", __func__);
        connect_mng_drop(conn);
        return true;
    }

    if (conn->data[conn->data_len - 1]) {
        WEB_ERROR("%s: request should be null terminated\r\n", __func__);
        connect_mng_drop(conn);
        return true;
    }

    request = cJSON_Parse((char *)conn->data);
    if (request == NULL) {
        WEB_ERROR("%s: cJSON Parse failed\r\n", __func__);
        connect_mng_drop(conn);
        return true;
    }
    free(conn->data);
    conn->data = NULL;
    
    if (request->type != cJSON_Object) {
        WEB_ERROR("%s: invalid request type(%d)\r\n", __func__, request->type);
        reply = cJSON_CreateObject();
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "bad request");
        goto end;
    }

    method = cJSON_GetObjectItem(request, "method");
    if ((method == NULL) || (method->type != cJSON_String)) {
        WEB_ERROR("%s: get method item failed\r\n", __func__);
        reply = cJSON_CreateObject();
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "bad method");
        goto end;
    }

    node = web_service_table_find(method->valuestring);
    if (node == NULL) {
        WEB_ERROR("%s: invalid method(%s)\r\n", __func__, method->valuestring);
        reply = cJSON_CreateObject();
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "unknown method");
        goto end;
    } else if (node->handler == NULL) {
        WEB_ERROR("%s: method %s handler is null\r\n", __func__, method->valuestring);
        reply = cJSON_CreateObject();
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "no handler");
        goto end;
    } else {
        WEB_VERBOS("%s: request method(%s)\r\n", __func__, method->valuestring);
        reply = node->handler(conn, request);
        if (!reply) {
            cJSON_Delete(request);
            return false;   /* delay reply or dropped */
        }
    }

end:
    cJSON_Delete(request);

    conn->data = (uint8_t *)cJSON_Print(reply);
    cJSON_Delete(reply);
    if (conn->data == NULL) {
        WEB_ERROR("%s: render reply failed\r\n", __func__);
        connect_mng_drop(conn);
        return true;
    }
    
    conn->data_len = strlen((char*)conn->data) + 1;
    (void)connect_mng_echo(conn);
    
    return true;
}

int web_service_register(const char *choice, web_service_handler handler)
{
    web_service_node_t *node;

    if ((choice == NULL) || (handler == NULL)) {
        WEB_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    node = (web_service_node_t *)malloc(sizeof(web_service_node_t));
    if (node == NULL) {
        WEB_ERROR("%s: malloc failed\r\n", __func__);
        return -ENOMEM;
    }
    node->choice = choice;
    node->handler = handler;

    (void)web_service_table_add(node);
    
    return OK;
}

void web_service_unregister(char *choice)
{
    web_service_node_t *node;

    node = web_service_table_detach(choice);
    if (node) {
        free(node);
    }

    return;
}

int web_service_init(void)
{
    int rv;

    if (web_service_status == true) {
        return OK;
    }

    rv = web_service_table_init();
    if (rv < 0) {
        WEB_ERROR("%s: WEB_SERVICE_TABLE init failed\r\n", __func__);
        goto err0;
    }

    rv = connect_mng_register(BACNET_WEB_SERVICE, web_service_connect_handler);
    if (rv < 0) {
        WEB_ERROR("%s: connect_mng register failed(%d)\r\n", __func__, rv);
        goto err1;
    }

    rv = web_ack_init();
    if (rv < 0) {
        WEB_ERROR("%s: web ack init failed\r\n", __func__);
        goto err2;
    }

    (void)web_service_register(WEB_LIST_METHOD, web_list_method);
    (void)web_service_register(WEB_READ_DEVICE_OBJECT_LIST, web_read_device_object_list);
    (void)web_service_register(WEB_READ_DEVICE_OBJECT_PROPERTY_LIST, web_read_device_object_property_list);
    (void)web_service_register(WEB_READ_DEVICE_OBJECT_PROPERTY_VALUE, web_read_device_object_property_value);
    (void)web_service_register(WEB_WRITE_DEVICE_OBJECT_PROPERTY_VALUE, web_write_device_object_property_value);
    (void)web_service_register(WEB_READ_DEVICE_ADDRESS_BINDING, web_read_device_address_binding);
    (void)web_service_register(WEB_SEND_WHO_IS, web_send_who_is);
    (void)web_service_register(WEB_GET_APP_STATUS, app_get_status);
    (void)web_service_register(WEB_GET_NETWORK_STATUS, network_get_status);
    (void)web_service_register(WEB_GET_DATALINK_STATUS, datalink_get_status);
    (void)web_service_register(WEB_GET_PORT_MIB, network_get_port_mib);
    
    web_service_status = true;
    web_service_set_dbg_level(0);

    return OK;

err2:
    connect_mng_unregister(BACNET_WEB_SERVICE);

err1:
    web_service_table_exit();
    
err0:

    return rv;
}

void web_service_exit(void)
{
    if (web_service_status == false) {
        return;
    }

    web_ack_exit();
    
    (void)connect_mng_unregister(BACNET_WEB_SERVICE);

    web_service_table_exit();
    
    web_service_status = false;

    return;
}

void web_service_set_dbg_level(uint32_t level)
{
    web_dbg_verbos = level & DEBUG_LEVEL_VERBOS;
    web_dbg_warn = level & DEBUG_LEVEL_WARN;
    web_dbg_err = level & DEBUG_LEVEL_ERROR;
}

