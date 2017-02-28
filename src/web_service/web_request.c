/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * web_request.c
 * Original Author:  linzhixian, 2015-9-14
 *
 * Web Request
 *
 * History
 */

#include "web_request.h"
#include "web_service.h"
#include "web_ack.h"
#include "web_def.h"

#include "bacnet/object/device.h"
#include "bacnet/service/whois.h"
#include "bacnet/addressbind.h"
#include "bacnet/config.h"

cJSON *web_send_who_is(connect_info_t *conn, cJSON *request)
{
    cJSON *reply, *tmp;
    uint16_t dnet;
    int32_t low_limit;
    int32_t high_limit;

    reply = cJSON_CreateObject();
    if (reply == NULL) {
        WEB_ERROR("%s: create result object failed\r\n", __func__);
        connect_mng_drop(conn);
        return NULL;
    }

    tmp = cJSON_GetObjectItem(request, "dnet");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        WEB_ERROR("%s: get dnet item failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get dnet item failed");
        return reply;
    }

    if ((tmp->valuedouble < 0) || (tmp->valuedouble > BACNET_BROADCAST_NETWORK)) {
        WEB_ERROR("%s: invalid dnet(%lf)\r\n", __func__, tmp->valuedouble);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "invalid dnet");
        return reply;
    }
    dnet = (uint16_t)tmp->valuedouble;
    
    tmp = cJSON_GetObjectItem(request, "low_limit");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        WEB_ERROR("%s: get low_limit item failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get low_limit item failed");
        return reply;
    }

    if (tmp->valuedouble > BACNET_MAX_INSTANCE) {
        WEB_ERROR("%s: invalid low_limit(%lf)\r\n", __func__, tmp->valuedouble);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "invalid low_limit");
        return reply;
    }
    low_limit = (int32_t)tmp->valuedouble;

    tmp = cJSON_GetObjectItem(request, "high_limit");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        WEB_ERROR("%s: get high_limit item failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get high_limit item failed");
        return reply;
    }

    if ((tmp->valuedouble > BACNET_MAX_INSTANCE) || (tmp->valuedouble < low_limit)) {
        WEB_ERROR("%s: invalid high_limit(%lf)\r\n", __func__, tmp->valuedouble);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "invalid high_limit");
        return reply;
    }
    high_limit = (int32_t)tmp->valuedouble;

    cJSON_AddItemToObject(reply, "result", cJSON_CreateObject());
    Send_WhoIs_Remote(dnet, low_limit, high_limit);

    return reply;
}

cJSON *web_read_device_object_list(connect_info_t *conn, cJSON *request)
{
    BACNET_DEVICE_OBJECT_PROPERTY property;
    web_cache_entry_t *entry;
    cJSON *reply, *tmp;
    int rv;

    reply = cJSON_CreateObject();
    if (reply == NULL) {
        WEB_ERROR("%s: create result object failed\r\n", __func__);
        connect_mng_drop(conn);
        return NULL;
    }

    tmp = cJSON_GetObjectItem(request, "device_id");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        WEB_ERROR("%s: get device_id item failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get device_id item failed");
        return reply;
    }
    if ((tmp->valueint < 0) || (tmp->valueint > BACNET_MAX_INSTANCE)) {
        WEB_ERROR("%s: invalid device_id(%d)\r\n", __func__, tmp->valueint);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "invalid device_id");
        return reply;
    }
    property.device_id = (uint32_t)tmp->valueint;
    
    if (property.device_id == device_object_instance_number()) {
        tmp = object_get_object_list();
        if (tmp == NULL) {
            WEB_ERROR("%s: get object list failed\r\n", __func__);
            cJSON_AddNumberToObject(reply, "error_code", -1);
            cJSON_AddStringToObject(reply, "reason", "get object list failed");
        } else {
            cJSON_AddItemToObject(reply, "result", tmp);
        }

        return reply;
    }

    property.object_id.type = OBJECT_DEVICE;
    property.object_id.instance = property.device_id;
    property.property_id = PROP_OBJECT_LIST;
    property.array_index = BACNET_ARRAY_ALL;
    entry = web_cache_entry_add(conn, WEB_READ_DEVICE_OBJECT_LIST, &property);
    if (entry == NULL) {
        WEB_ERROR("%s: add web_cache entry failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "add web_cache entry failed");
        return reply;
    }

    /* send RP request */
    bacnet_addr_t dst_addr;
    if (!query_address_from_device(property.device_id, NULL, &dst_addr)) {
        WEB_ERROR("%s: get address from device(%d) failed\r\n", __func__, property.device_id);
        web_cache_entry_delete(entry);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get address from device failed");
        return reply;
    }

    tsm_invoker_t *invoker;
    invoker = tsm_alloc_invokeID(&dst_addr, SERVICE_CONFIRMED_READ_PROPERTY,
        web_confirmed_ack_handler, (void *)entry);
    if (invoker == NULL) {
        WEB_ERROR("%s: alloc invokeID failed\r\n", __func__);
        web_cache_entry_delete(entry);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "alloc invokeID failed");
        return reply;
    }

    DECLARE_BACNET_BUF(tx_apdu, MIN_APDU);
    (void)bacnet_buf_init(&tx_apdu.buf, MIN_APDU);
    rv = rp_encode_apdu(&tx_apdu.buf, invoker->invokeID, OBJECT_DEVICE, property.device_id,
        property.property_id, property.array_index);
    if ((rv < 0) || (rv > MIN_APDU)) {
        WEB_ERROR("%s: encode apdu failed(%d)\r\n", __func__, rv);
        tsm_free_invokeID(invoker);
        web_cache_entry_delete(entry);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "encode apdu failed");
        return reply;
    }

    rv = tsm_send_apdu(invoker, &tx_apdu.buf, PRIORITY_NORMAL, 0);
    if (rv < 0) {
        WEB_ERROR("%s: send RP request failed(%d)\r\n", __func__, rv);
        tsm_free_invokeID(invoker);
        web_cache_entry_delete(entry);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "send RP request failed");
        return reply;
    }

    cJSON_Delete(reply);

    return NULL;
}

cJSON *web_read_device_object_property_list(connect_info_t *conn, cJSON *request)
{
    BACNET_DEVICE_OBJECT_PROPERTY property;
    web_cache_entry_t *entry;
    cJSON *reply, *tmp;
    int rv;

    reply = cJSON_CreateObject();
    if (reply == NULL) {
        WEB_ERROR("%s: create result object failed\r\n", __func__);
        connect_mng_drop(conn);
        return NULL;
    }
    
    tmp = cJSON_GetObjectItem(request, "device_id");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        WEB_ERROR("%s: get device_id item failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get device_id item failed");
        return reply;
    }
    if ((tmp->valueint < 0) || (tmp->valueint > BACNET_MAX_INSTANCE)) {
        WEB_ERROR("%s: invalid device_id(%d)\r\n", __func__, tmp->valueint);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "invalid device_id");
        return reply;
    }
    property.device_id = (uint32_t)tmp->valueint;

    tmp = cJSON_GetObjectItem(request, "object_type");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        WEB_ERROR("%s: get object_type item failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get object_type item failed");
        return reply;
    }
    if ((tmp->valueint < 0) || (tmp->valueint > MAX_BACNET_OBJECT_TYPE)) {
        WEB_ERROR("%s: invalid object type(%d)\r\n", __func__,
            tmp->valueint);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "invalid object type");
        return reply;
    }
    property.object_id.type = (BACNET_OBJECT_TYPE)tmp->valueint;

    tmp = cJSON_GetObjectItem(request, "object_instance");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        WEB_ERROR("%s: get object_instance item failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get object_instance item failed");
        return reply;
    }
    if ((tmp->valueint < 0) || (tmp->valueint > BACNET_MAX_INSTANCE)) {
        WEB_ERROR("%s: invalid object_instance(%d)\r\n", __func__,
            tmp->valueint);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "invalid object_instance");
        return reply;
    }
    property.object_id.instance = (uint32_t)tmp->valueint;

    if (property.device_id == device_object_instance_number()) {
        tmp = object_get_property_list(property.object_id.type, property.object_id.instance);
        if (tmp == NULL) {
            WEB_ERROR("%s: get property list failed\r\n", __func__);
            cJSON_AddNumberToObject(reply, "error_code", -1);
            cJSON_AddStringToObject(reply, "reason", "get property list failed");
        } else {
            cJSON_AddItemToObject(reply, "result", tmp);
        }

        return reply;
    }

    property.property_id = PROP_PROPERTY_LIST;
    property.array_index = BACNET_ARRAY_ALL;
    entry = web_cache_entry_add(conn, WEB_READ_DEVICE_OBJECT_PROPERTY_LIST, &property);
    if (entry == NULL) {
        WEB_ERROR("%s: add web_cache entry failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "add web_cache entry failed");
        return reply;
    }

    /* send RP request */
    bacnet_addr_t dst_addr;
    if (!query_address_from_device(property.device_id, NULL, &dst_addr)) {
        WEB_ERROR("%s: get address from device(%d) failed\r\n", __func__, property.device_id);
        web_cache_entry_delete(entry);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get address from device failed");
        return reply;
    }

    tsm_invoker_t *invoker;
    invoker = tsm_alloc_invokeID(&dst_addr, SERVICE_CONFIRMED_READ_PROPERTY,
        web_confirmed_ack_handler, (void *)entry);
    if (invoker == NULL) {
        WEB_ERROR("%s: alloc invokeID failed\r\n", __func__);
        web_cache_entry_delete(entry);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "alloc invokeID failed");
        return reply;
    }

    DECLARE_BACNET_BUF(tx_apdu, MIN_APDU);
    (void)bacnet_buf_init(&tx_apdu.buf, MIN_APDU);
    rv = rp_encode_apdu(&tx_apdu.buf, invoker->invokeID, property.object_id.type,
        property.object_id.instance, property.property_id, property.array_index);
    if ((rv < 0) || (rv > MIN_APDU)) {
        WEB_ERROR("%s: encode apdu failed(%d)\r\n", __func__, rv);
        tsm_free_invokeID(invoker);
        web_cache_entry_delete(entry);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "encode apdu failed");
        return reply;
    }

    rv = tsm_send_apdu(invoker, &tx_apdu.buf, PRIORITY_NORMAL, 0);
    if (rv < 0) {
        WEB_ERROR("%s: send RP request failed(%d)\r\n", __func__, rv);
        tsm_free_invokeID(invoker);
        web_cache_entry_delete(entry);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "send RP request failed");
        return reply;
    }

    cJSON_Delete(reply);

    return NULL;
}

cJSON *web_read_device_object_property_value(connect_info_t *conn, cJSON *request)
{
    BACNET_DEVICE_OBJECT_PROPERTY property;
    web_cache_entry_t *entry;
    cJSON *reply, *tmp;
    int rv;

    reply = cJSON_CreateObject();
    if (reply == NULL) {
        WEB_ERROR("%s: create result object failed\r\n", __func__);
        connect_mng_drop(conn);
        return NULL;
    }

    tmp = cJSON_GetObjectItem(request, "device_id");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        WEB_ERROR("%s: get device_id item failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get device_id item failed");
        return reply;
    }
    if ((tmp->valueint < 0) || (tmp->valueint > BACNET_MAX_INSTANCE)) {
        WEB_ERROR("%s: invalid device_id(%d)\r\n", __func__,
            tmp->valueint);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "invalid device_id");
        return reply;
    }
    property.device_id = (uint32_t)tmp->valueint;

    tmp = cJSON_GetObjectItem(request, "object_type");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        WEB_ERROR("%s: get object_type item failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get object_type item failed");
        return reply;
    }
    if ((tmp->valueint < 0) || (tmp->valueint > MAX_BACNET_OBJECT_TYPE)) {
        WEB_ERROR("%s: invalid object type(%d)\r\n", __func__,
            tmp->valueint);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "invalid object type");
        return reply;
    }
    property.object_id.type = (BACNET_OBJECT_TYPE)tmp->valueint;

    tmp = cJSON_GetObjectItem(request, "object_instance");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        WEB_ERROR("%s: get object_instance item failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get object_instance item failed");
        return reply;
    }
    if ((tmp->valueint < 0) || (tmp->valueint > BACNET_MAX_INSTANCE)) {
        WEB_ERROR("%s: invalid object_instance(%d)\r\n", __func__,
            tmp->valueint);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "invalid object_instance");
        return reply;
    }
    property.object_id.instance = (uint32_t)tmp->valueint;

    tmp = cJSON_GetObjectItem(request, "property_id");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        WEB_ERROR("%s: get property_id item failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get property_id item failed");
        return reply;
    }
    if ((tmp->valueint < 0) || (tmp->valueint >= MAX_BACNET_PROPERTY_ID)) {
        WEB_ERROR("%s: invalid property_id(%d)\r\n", __func__,
            tmp->valueint);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "invalid property_id");
        return reply;
    }
    property.property_id = (BACNET_PROPERTY_ID)tmp->valueint;

    tmp = cJSON_GetObjectItem(request, "array_index");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        WEB_ERROR("%s: get array_index item failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get array_index item failed");
        return reply;
    }
    if (tmp->valueint < -1) {
        WEB_ERROR("%s: invalid array_index(%d)\r\n", __func__, 
            tmp->valueint);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "invalid array_index");
        return reply;
    }
    property.array_index = (uint32_t)tmp->valueint;
    
    if (property.device_id == device_object_instance_number()) {
        tmp = object_get_property_value(&property);
        if (tmp == NULL) {
            WEB_ERROR("%s: get property value failed\r\n", __func__);
            cJSON_AddNumberToObject(reply, "error_code", -1);
            cJSON_AddStringToObject(reply, "reason", "get property value failed");
        } else {
            cJSON_AddItemToObject(reply, "result", tmp);
        }
        
        return reply;
    }

    entry = web_cache_entry_add(conn, WEB_READ_DEVICE_OBJECT_PROPERTY_VALUE, &property);
    if (entry == NULL) {
        WEB_ERROR("%s: add web_cache entry failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "add web_cache entry failed");
        return reply;
    }

    /* send RP request */
    bacnet_addr_t dst_addr;
    if (!query_address_from_device(property.device_id, NULL, &dst_addr)) {
        WEB_ERROR("%s: get address from device(%d) failed\r\n", __func__, property.device_id);
        web_cache_entry_delete(entry);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get address from device failed");
        return reply;
    }

    tsm_invoker_t *invoker;
    invoker = tsm_alloc_invokeID(&dst_addr, SERVICE_CONFIRMED_READ_PROPERTY,
        web_confirmed_ack_handler, (void *)entry);
    if (invoker == NULL) {
        WEB_ERROR("%s: alloc invokeID failed\r\n", __func__);
        web_cache_entry_delete(entry);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "alloc invokeID failed");
        return reply;
    }

    DECLARE_BACNET_BUF(tx_apdu, MIN_APDU);
    (void)bacnet_buf_init(&tx_apdu.buf, MIN_APDU);
    rv = rp_encode_apdu(&tx_apdu.buf, invoker->invokeID, property.object_id.type,
        property.object_id.instance, property.property_id, property.array_index);
    if ((rv < 0) || (rv > MIN_APDU)) {
        WEB_ERROR("%s: encode apdu failed(%d)\r\n", __func__, rv);
        tsm_free_invokeID(invoker);
        web_cache_entry_delete(entry);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "encode apdu failed");
        return reply;
    }

    rv = tsm_send_apdu(invoker, &tx_apdu.buf, PRIORITY_NORMAL, 0);
    if (rv < 0) {
        WEB_ERROR("%s: send RP request failed(%d)\r\n", __func__, rv);
        tsm_free_invokeID(invoker);
        web_cache_entry_delete(entry);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "send RP request failed");
        return reply;
    }

    cJSON_Delete(reply);

    return NULL;
}

cJSON *web_write_device_object_property_value(connect_info_t *conn, cJSON *request)
{
    DECLARE_BACNET_BUF(apdu, MAX_APDU);
    BACNET_DEVICE_OBJECT_PROPERTY property;
    BACNET_WRITE_PROPERTY_DATA wp_data;
    web_cache_entry_t *entry;
    uint8_t priority;
    cJSON *reply, *tmp;
    int rv;

    reply = cJSON_CreateObject();
    if (reply == NULL) {
        WEB_ERROR("%s: create result object failed\r\n", __func__);
        connect_mng_drop(conn);
        return NULL;
    }

    tmp = cJSON_GetObjectItem(request, "device_id");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        WEB_ERROR("%s: get device_id item failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get device_id item failed");
        return reply;
    }
    if ((tmp->valueint < 0) || (tmp->valueint > BACNET_MAX_INSTANCE)) {
        WEB_ERROR("%s: invalid device_id(%d)\r\n", __func__,
            tmp->valueint);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "invalid device_id");
        return reply;
    }
    property.device_id = (uint32_t)tmp->valueint;

    tmp = cJSON_GetObjectItem(request, "object_type");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        WEB_ERROR("%s: get object_type item failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get object_type item failed");
        return reply;
    }
    if ((tmp->valueint < 0) || (tmp->valueint > MAX_BACNET_OBJECT_TYPE)) {
        WEB_ERROR("%s: invalid object type(%d)\r\n", __func__,
            tmp->valueint);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "invalid object type");
        return reply;
    }
    property.object_id.type = (BACNET_OBJECT_TYPE)tmp->valueint;

    tmp = cJSON_GetObjectItem(request, "object_instance");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        WEB_ERROR("%s: get object_instance item failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get object_instance item failed");
        return reply;
    }
    if ((tmp->valueint < 0) || (tmp->valueint > BACNET_MAX_INSTANCE)) {
        WEB_ERROR("%s: invalid object_instance(%d)\r\n", __func__,
            tmp->valueint);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "invalid object_instance");
        return reply;
    }
    property.object_id.instance = (uint32_t)tmp->valueint;

    tmp = cJSON_GetObjectItem(request, "property_id");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        WEB_ERROR("%s: get property_id item failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get property_id item failed");
        return reply;
    }
    if ((tmp->valueint < 0) || (tmp->valueint >= MAX_BACNET_PROPERTY_ID)) {
        WEB_ERROR("%s: invalid property_id(%d)\r\n", __func__,
            tmp->valueint);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "invalid property_id");
        return reply;
    }
    property.property_id = (BACNET_PROPERTY_ID)tmp->valueint;

    tmp = cJSON_GetObjectItem(request, "array_index");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        WEB_ERROR("%s: get array_index item failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get array_index item failed");
        return reply;
    }
    if (tmp->valueint < -1) {
        WEB_ERROR("%s: invalid array_index(%d)\r\n", __func__, 
            tmp->valueint);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "invalid array_index");
        return reply;
    }
    property.array_index = (uint32_t)tmp->valueint;

    tmp = cJSON_GetObjectItem(request, "priority");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        WEB_ERROR("%s: get priority item failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get priority item failed");
        return reply;
    }
    if ((tmp->valueint < 1) || (tmp->valueint > BACNET_MAX_PRIORITY)) {
        WEB_ERROR("%s: invalid priority(%d)\r\n", __func__, tmp->valueint);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "invalid priority");
        return reply;
    }
    priority = (uint8_t)tmp->valueint;

    tmp = cJSON_GetObjectItem(request, "value");
    if ((tmp == NULL) || (tmp->type != cJSON_String)) {
        WEB_ERROR("%s: get value item failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get value item failed");
        return reply;
    }

    (void)bacnet_buf_init(&apdu.buf, MAX_APDU);
    
    if (property.device_id == device_object_instance_number()) {
        if (!bacapp_parse_application_data(&apdu.buf, tmp->valuestring)) {
            WEB_ERROR("%s: parse value failed\r\n", __func__);
            cJSON_AddNumberToObject(reply, "error_code", -1);
            cJSON_AddStringToObject(reply, "reason", "parse value failed");
            return reply;
        }

        wp_data.application_data = apdu.buf.data;
        wp_data.application_data_len = apdu.buf.data_len;
        wp_data.priority = priority;
        wp_data.object_type = property.object_id.type;
        wp_data.object_instance = property.object_id.instance;
        wp_data.property_id = property.property_id;
        wp_data.array_index = property.array_index;
        
        if (!object_write_property(&wp_data)) {
            WEB_ERROR("%s: write property failed\r\n", __func__);
            cJSON_AddNumberToObject(reply, "error_code", -1);
            cJSON_AddStringToObject(reply, "reason", "write property failed");
            return reply;
        }

        cJSON_AddStringToObject(reply, "result", "success");
        
        return reply;
    }

    entry = web_cache_entry_add(conn, WEB_WRITE_DEVICE_OBJECT_PROPERTY_VALUE, &property);
    if (entry == NULL) {
        WEB_ERROR("%s: add web_cache entry failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "add web_cache entry failed");
        return reply;
    }

    /* send WP request */
    bacnet_addr_t dst_addr;
    if (!query_address_from_device(property.device_id, NULL, &dst_addr)) {
        WEB_ERROR("%s: get address from device(%d) failed\r\n", __func__, property.device_id);
        web_cache_entry_delete(entry);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get address from device failed");
        return reply;
    }

    tsm_invoker_t *invoker;
    invoker = tsm_alloc_invokeID(&dst_addr, SERVICE_CONFIRMED_WRITE_PROPERTY,
        web_confirmed_ack_handler, (void *)entry);
    if (invoker == NULL) {
        WEB_ERROR("%s: alloc invokeID failed\r\n", __func__);
        web_cache_entry_delete(entry);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "alloc invokeID failed");
        return reply;
    }
    
    wp_req_encode(&apdu.buf, property.object_id.type, property.object_id.instance,
        property.property_id, property.array_index);
    if (!bacapp_parse_application_data(&apdu.buf, tmp->valuestring)) {
        WEB_ERROR("%s: parse application data failed\r\n", __func__);
        tsm_free_invokeID(invoker);
        web_cache_entry_delete(entry);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "parse application data failed");
        return reply;
    }
    
    if (!wp_req_encode_end(&apdu.buf, invoker->invokeID, priority)) {
        WEB_ERROR("%s: wp req encode end failed\r\n", __func__);
        tsm_free_invokeID(invoker);
        web_cache_entry_delete(entry);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "wp req encode end failed");
        return reply;
    }

    rv = tsm_send_apdu(invoker, &apdu.buf, PRIORITY_NORMAL, 0);
    if (rv < 0) {
        WEB_ERROR("%s: send WP request failed(%d)\r\n", __func__, rv);
        tsm_free_invokeID(invoker);
        web_cache_entry_delete(entry);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "send WP request failed");
        return reply;
    }

    cJSON_Delete(reply);

    return NULL;
}

cJSON *web_read_device_address_binding(connect_info_t *conn, cJSON *request)
{
    BACNET_DEVICE_OBJECT_PROPERTY property;
    web_cache_entry_t *entry;
    cJSON *reply, *tmp;
    int rv;

    reply = cJSON_CreateObject();
    if (reply == NULL) {
        WEB_ERROR("%s: create result object failed\r\n", __func__);
        connect_mng_drop(conn);
        return NULL;
    }
    
    tmp = cJSON_GetObjectItem(request, "device_id");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        WEB_ERROR("%s: get device_id item failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get device_id item failed");
        return reply;
    }
    if ((tmp->valueint < 0) || (tmp->valueint > BACNET_MAX_INSTANCE)) {
        WEB_ERROR("%s: invalid device_id(%d)\r\n", __func__, tmp->valueint);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "invalid device_id");
        return reply;
    }
    property.device_id = (uint32_t)tmp->valueint;

    if (property.device_id == device_object_instance_number()) {
        tmp = get_address_binding();
        if (tmp == NULL) {
            WEB_ERROR("%s: get address binding failed\r\n", __func__);
            cJSON_AddNumberToObject(reply, "error_code", -1);
            cJSON_AddStringToObject(reply, "reason", "get address binding failed");
        } else {
            cJSON_AddItemToObject(reply, "result", tmp);
        }
        
        return reply;
    }

    property.object_id.type = OBJECT_DEVICE;
    property.object_id.instance = property.device_id;
    property.property_id = PROP_DEVICE_ADDRESS_BINDING;
    property.array_index = BACNET_ARRAY_ALL;
    entry = web_cache_entry_add(conn, WEB_READ_DEVICE_ADDRESS_BINDING, &property);
    if (entry == NULL) {
        WEB_ERROR("%s: add web_cache entry failed\r\n", __func__);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "add web_cache entry failed");
        return reply;
    }

    /* send RP request */
    bacnet_addr_t dst_addr;
    if (!query_address_from_device(property.device_id, NULL, &dst_addr)) {
        WEB_ERROR("%s: get address from device(%d) failed\r\n", __func__, property.device_id);
        web_cache_entry_delete(entry);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "get address from device failed");
        return reply;
    }

    tsm_invoker_t *invoker;
    invoker = tsm_alloc_invokeID(&dst_addr, SERVICE_CONFIRMED_READ_PROPERTY,
        web_confirmed_ack_handler, (void *)entry);
    if (invoker == NULL) {
        WEB_ERROR("%s: alloc invokeID failed\r\n", __func__);
        web_cache_entry_delete(entry);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "alloc invokeID failed");
        return reply;
    }

    DECLARE_BACNET_BUF(tx_apdu, MIN_APDU);
    (void)bacnet_buf_init(&tx_apdu.buf, MIN_APDU);
    rv = rp_encode_apdu(&tx_apdu.buf, invoker->invokeID, property.object_id.type,
        property.object_id.instance, property.property_id, property.array_index);
    if ((rv < 0) || (rv > MIN_APDU)) {
        WEB_ERROR("%s: encode apdu failed(%d)\r\n", __func__, rv);
        tsm_free_invokeID(invoker);
        web_cache_entry_delete(entry);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "encode apdu failed");
        return reply;
    }

    rv = tsm_send_apdu(invoker, &tx_apdu.buf, PRIORITY_NORMAL, 0);
    if (rv < 0) {
        WEB_ERROR("%s: send RP request failed(%d)\r\n", __func__, rv);
        tsm_free_invokeID(invoker);
        web_cache_entry_delete(entry);
        cJSON_AddNumberToObject(reply, "error_code", -1);
        cJSON_AddStringToObject(reply, "reason", "send RP request failed");
        return reply;
    }

    cJSON_Delete(reply);
    
    return NULL;
}

