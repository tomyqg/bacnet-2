/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * web_ack.c
 * Original Author:  linzhixian, 2015-9-14
 *
 * Web Service
 *
 * History
 */

#include <stdlib.h>
#include <errno.h>

#include "web_ack.h"
#include "web_def.h"
#include "web_service.h"
#include "misc/cJSON.h"
#include "bacnet/addressbind.h"
#include "bacnet/bacdcode.h"
#include "bacnet/service/error.h"
#include "bacnet/tsm.h"
#include "bacnet/service/rp.h"

static web_cache_entry_t *web_cache = NULL;

static int web_ack_read_device_object_list(BACNET_READ_PROPERTY_DATA *rp_data, cJSON *reply)
{
    cJSON *result, *tmp;
    BACNET_OBJECT_TYPE object_type;
    uint32_t object_instance;
    uint8_t *pdu;
    int dec_len, len;
    
    result = cJSON_CreateArray();
    if (result == NULL) {
        WEB_ERROR("%s: create result item failed\r\n", __func__);
        cJSON_AddStringToObject(reply, "reason", "create result item failed");
        return -EPERM;
    }

    len = 0;
    pdu = rp_data->application_data;
    while (len < rp_data->application_data_len) {
        dec_len = decode_application_object_id(&pdu[len], &object_type, &object_instance);
        if (dec_len < 0) {
            WEB_ERROR("%s: decode object_id failed(%d)\r\n", __func__, dec_len);
            cJSON_Delete(result);
            cJSON_AddStringToObject(reply, "reason", "decode object_id failed");
            return -EPERM;
        }
        len += dec_len;

        tmp = cJSON_CreateObject();
        if (tmp == NULL) {
            WEB_ERROR("%s: create object failed\r\n", __func__);
            cJSON_Delete(result);
            cJSON_AddStringToObject(reply, "reason", "create object failed");
            return -EPERM;
        }

        cJSON_AddItemToArray(result, tmp);
    }

    cJSON_AddItemToObject(reply, "result", result);

    return OK;
}

static int web_ack_read_device_object_property_list(BACNET_READ_PROPERTY_DATA *rp_data, cJSON *reply)
{
    cJSON *result, *tmp;
    uint32_t property_id;
    uint8_t *pdu;
    int dec_len, len;
    
    result = cJSON_CreateArray();
    if (result == NULL) {
        WEB_ERROR("%s: create result item failed\r\n", __func__);
        cJSON_AddStringToObject(reply, "reason", "create result item failed");
        return -EPERM;
    }

    len = 0;
    pdu = rp_data->application_data;
    while (len < rp_data->application_data_len) {
        dec_len = decode_application_enumerated(&pdu[len], &property_id);
        if (dec_len < 0) {
            WEB_ERROR("%s: decode property_id failed(%d)\r\n", __func__,
                dec_len);
            cJSON_Delete(result);
            cJSON_AddStringToObject(reply, "reason", "decode property_id failed");
            return -EPERM;
        }
        len += dec_len;
        
        tmp = cJSON_CreateObject();
        if (tmp == NULL) {
            WEB_ERROR("%s: create object failed\r\n", __func__);
            cJSON_Delete(result);
            cJSON_AddStringToObject(reply, "reason", "create object failed");
            return -EPERM;
        }

        cJSON_AddItemToArray(result, cJSON_CreateNumber(property_id));
    }
    
    cJSON_AddItemToObject(reply, "result", result);

    return OK;    
}

static int web_ack_read_device_object_property_value(BACNET_READ_PROPERTY_DATA *rp_data, cJSON *reply)
{
    char string[MAX_APDU];

    if (!bacapp_snprint_value(string, sizeof(string), rp_data->application_data, 
            rp_data->application_data_len)) {
        WEB_ERROR("%s: snprint value failed\r\n", __func__);
        cJSON_AddStringToObject(reply, "reason", "snprint value failed");
        return -EPERM;
    }
    
    cJSON_AddStringToObject(reply, "result", string);

    return OK;
}

static int web_ack_read_device_address_binding(BACNET_READ_PROPERTY_DATA *rp_data, cJSON *reply)
{
    cJSON *result, *tmp;
    BACNET_OCTET_STRING mac;
    BACNET_OBJECT_TYPE object_type;
    char mac_str[MAX_MAC_STR_LEN];
    uint32_t object_instance;
    uint32_t net_num;
    uint8_t *pdu;
    int dec_len, len;

    result = cJSON_CreateArray();
    if (result == NULL) {
        WEB_ERROR("%s: create result item failed\r\n", __func__);
        cJSON_AddStringToObject(reply, "reason", "create result item failed");
        return -EPERM;
    }

    len = 0;
    pdu = rp_data->application_data;
    while (len < rp_data->application_data_len) {
        tmp = cJSON_CreateObject();
        if (tmp == NULL) {
            WEB_ERROR("%s: create object failed\r\n", __func__);
            cJSON_Delete(result);
            cJSON_AddStringToObject(reply, "reason", "create object failed");
            return -EPERM;
        }
        cJSON_AddItemToArray(result, tmp);
        
        dec_len = decode_application_object_id(&pdu[len], &object_type, &object_instance);
        if (dec_len < 0) {
            WEB_ERROR("%s: decode device_id failed(%d)\r\n", __func__, dec_len);
            cJSON_Delete(result);
            cJSON_AddStringToObject(reply, "reason", "decode device_id failed");
            return -EPERM;
        }

        if (object_type != OBJECT_DEVICE) {
            WEB_ERROR("%s: invalid device object(%d)\r\n", __func__, object_type);
            cJSON_Delete(result);
            cJSON_AddStringToObject(reply, "reason", "invalid invalid device object");
            return -EPERM;
        }
        
        len += dec_len;
        cJSON_AddNumberToObject(tmp, "device_id", object_instance);

        dec_len = decode_application_unsigned(&pdu[len], &net_num);
        if (dec_len < 0) {
            WEB_ERROR("%s: decode net_num failed(%d)\r\n", __func__, dec_len);
            cJSON_Delete(result);
            cJSON_AddStringToObject(reply, "reason", "decode net_num failed");
            return -EPERM;
        }
        
        len += dec_len;
        cJSON_AddNumberToObject(tmp, "net_num", net_num);
        
        dec_len = decode_application_octet_string(&pdu[len], &mac);
        if (dec_len < 0) {
            WEB_ERROR("%s: decode mac failed(%d)\r\n", __func__, dec_len);
            cJSON_Delete(result);
            cJSON_AddStringToObject(reply, "reason", "decode mac failed");
            return -EPERM;
        }
        len += dec_len;
        
        if (mac.length > MAX_MAC_LEN) {
            WEB_ERROR("%s: invalid mac_len(%d)\r\n", __func__, mac.length);
            cJSON_Delete(result);
            cJSON_AddStringToObject(reply, "reason", "invalid mac_len");
            return -EPERM;
        }

        dec_len = bacnet_array_to_macstr(mac.value, mac.length, mac_str, sizeof(mac_str));
        if (dec_len < 0) {
            WEB_ERROR("%s: array to macstr failed(%d)\r\n", __func__, dec_len);
            cJSON_Delete(result);
            cJSON_AddStringToObject(reply, "reason", "array to macstr failed");
            return -EPERM;
        }

        cJSON_AddStringToObject(tmp, "mac", mac_str);
    }
    
    cJSON_AddItemToObject(reply, "result", result);

    return OK;
}

static void web_rp_confirmed_ack_handler(const char *web_choice,
                BACNET_DEVICE_OBJECT_PROPERTY *property, BACNET_CONFIRMED_SERVICE_ACK_DATA *ack_data,
                cJSON *reply)
{
    BACNET_READ_PROPERTY_DATA rp_data;
    int rv;
    
    rv = rp_ack_decode(ack_data->service_data, ack_data->service_data_len, &rp_data);
    if (rv < 0) {
        WEB_ERROR("%s: decode rp ack failed(%d)\r\n", __func__, rv);
        cJSON_AddStringToObject(reply, "reason", "decode rp ack failed");
        return;
    }

    if ((property->object_id.type != rp_data.object_type)
            || (property->object_id.instance != rp_data.object_instance)
            || (property->property_id != rp_data.property_id)
            || (property->array_index != rp_data.array_index)) {
        WEB_ERROR("%s: match entry_property failed\r\n", __func__);
        cJSON_AddStringToObject(reply, "reason", "match entry_property failed");
        return;
    }

    if (web_choice == (char *)WEB_READ_DEVICE_OBJECT_LIST) {
        (void)web_ack_read_device_object_list(&rp_data, reply);
    } else if (web_choice == (char *)WEB_READ_DEVICE_OBJECT_PROPERTY_LIST) {
        (void)web_ack_read_device_object_property_list(&rp_data, reply);
    } else if (web_choice == (char *)WEB_READ_DEVICE_OBJECT_PROPERTY_VALUE) {
        (void)web_ack_read_device_object_property_value(&rp_data, reply);
    } else if (web_choice == (char *)WEB_READ_DEVICE_ADDRESS_BINDING) {
        (void)web_ack_read_device_address_binding(&rp_data, reply);
    } else {
        WEB_ERROR("%s: invalid web_service choice(%s)\r\n", __func__, web_choice);
        cJSON_AddStringToObject(reply, "reason", "invalid web_service choice");
    }

    return;
}

static void web_complex_ack_handler(const char *web_choice, BACNET_DEVICE_OBJECT_PROPERTY *property,
                bacnet_buf_t *apdu, cJSON *reply)
{
    BACNET_CONFIRMED_SERVICE_ACK_DATA service_ack_data;
    
    if (apdu_decode_complex_ack(apdu, &service_ack_data) < 0) {
        WEB_ERROR("%s: decode ack header failed\r\n", __func__);
        cJSON_AddStringToObject(reply, "reason", "decode complex_ack header failed");
        return;
    }

    switch (service_ack_data.service_choice) {
    case SERVICE_CONFIRMED_READ_PROPERTY:
        web_rp_confirmed_ack_handler(web_choice, property, &service_ack_data, reply);
        break;
    
    default:
        WEB_ERROR("%s: invalid service choice(%d)\r\n", __func__,
            service_ack_data.service_choice);
        cJSON_AddStringToObject(reply, "reason", "invalid complex_ack choice");
        break;
    }

    return;
}

static void web_simple_ack_handler(BACNET_DEVICE_OBJECT_PROPERTY *property, bacnet_buf_t *apdu,
                cJSON *reply)
{
    uint8_t service_choice;

    cJSON_AddNumberToObject(reply, "object_type", property->object_id.type);
    cJSON_AddNumberToObject(reply, "object_instance", property->object_id.instance);
    cJSON_AddNumberToObject(reply, "property_id", property->property_id);
    cJSON_AddNumberToObject(reply, "array_index", (double)((int)(property->array_index)));
    
    if (apdu->data_len != 3) {
        WEB_ERROR("%s: invalid pdu_len(%d)\r\n", __func__, apdu->data_len);
        cJSON_AddStringToObject(reply, "reason", "invalid simple_ack_pdu len");
        return;
    }

    service_choice = apdu->data[2];
    switch (service_choice) {
    case SERVICE_CONFIRMED_WRITE_PROPERTY:
        WEB_VERBOS("%s: receive service choice(%d) ack ok\r\n", __func__, service_choice);
        cJSON_AddStringToObject(reply, "result", "success");
        break;
    
    default:
        WEB_ERROR("%s: invalid service_choice(%d)\r\n", __func__, service_choice);
        cJSON_AddStringToObject(reply, "reason", "invalid simple_ack choice");
        break;
    }

    return;
}

static void web_error_handler(BACNET_DEVICE_OBJECT_PROPERTY *property, bacnet_buf_t *apdu,
                cJSON *reply)
{
    BACNET_CONFIRMED_SERVICE service_choice;
    BACNET_ERROR_CLASS error_class;
    BACNET_ERROR_CODE error_code;
    int len;

    cJSON_AddNumberToObject(reply, "object_type", property->object_id.type);
    cJSON_AddNumberToObject(reply, "object_instance", property->object_id.instance);
    cJSON_AddNumberToObject(reply, "property_id", property->property_id);
    cJSON_AddNumberToObject(reply, "array_index", (double)((int)(property->array_index)));
    
    if (apdu->data_len < 4) {
        WEB_ERROR("%s: invalid error_pdu len(%d)\r\n", __func__, apdu->data_len);
        cJSON_AddStringToObject(reply, "reason", "invalid error_pdu len");
        return;
    }

    len = bacerror_decode_apdu(apdu, NULL, &service_choice, &error_class, &error_code);
    if (len < 0) {
        WEB_ERROR("%s: decode apdu failed(%d)\r\n", __func__, len);
        cJSON_AddStringToObject(reply, "reason", "decode error_pdu failed");
        return;
    }

    cJSON_AddStringToObject(reply, "reason", "bacnet error");

    WEB_VERBOS("%s: service_choice(%d)\r\n", __func__, service_choice);
    WEB_VERBOS("%s: error_class(%d)\r\n", __func__, error_class);
    WEB_VERBOS("%s: error_code(%d)\r\n", __func__, error_code);

    return;
}

static void web_reject_handler(BACNET_DEVICE_OBJECT_PROPERTY *property, bacnet_buf_t *apdu,
                cJSON *reply)
{
    cJSON_AddNumberToObject(reply, "object_type", property->object_id.type);
    cJSON_AddNumberToObject(reply, "object_instance", property->object_id.instance);
    cJSON_AddNumberToObject(reply, "property_id", property->property_id);
    cJSON_AddNumberToObject(reply, "array_index", (double)((int)(property->array_index)));
    
    if (apdu->data_len != 3) {
        WEB_ERROR("%s: invalid reject_pdu len(%d)\r\n", __func__, apdu->data_len);
        cJSON_AddStringToObject(reply, "reason", "invalid reject_pdu len");
        return;
    }

    cJSON_AddStringToObject(reply, "reason", "bacnet reject");

    WEB_VERBOS("%s: reject reason(%d)\r\n", __func__, apdu->data[2]);

    return;
}

static void web_abort_handler(BACNET_DEVICE_OBJECT_PROPERTY *property, bacnet_buf_t *apdu,
                cJSON *reply)
{
    cJSON_AddNumberToObject(reply, "object_type", property->object_id.type);
    cJSON_AddNumberToObject(reply, "object_instance", property->object_id.instance);
    cJSON_AddNumberToObject(reply, "property_id", property->property_id);
    cJSON_AddNumberToObject(reply, "array_index", (double)((int)(property->array_index)));
    
    if (apdu->data_len != 3) {
        WEB_ERROR("%s: invalid abort_pdu len(%d)\r\n", __func__, apdu->data_len);
        cJSON_AddStringToObject(reply, "reason", "invalid abort_pdu len");
        return;
    }

    cJSON_AddStringToObject(reply, "reason", "bacnet abort");

    WEB_VERBOS("%s: reason(%d)\r\n", __func__, apdu->data[2]);
    
    return;
}

void web_confirmed_ack_handler(tsm_invoker_t *invoker, bacnet_buf_t *apdu, BACNET_PDU_TYPE apdu_type)
{
    web_cache_entry_t *entry;
    connect_info_t *conn;
    cJSON *reply;
    uint32_t src_device_id;
    
    if (invoker == NULL) {
        WEB_ERROR("%s: null invoker\r\n", __func__);
        return;
    }
    
    if ((apdu == NULL) || (apdu->data == NULL)) {
        WEB_ERROR("%s: invalid argument\r\n", __func__);
        goto out0;
    }

    entry = (web_cache_entry_t *)invoker->data;
    if (entry == NULL) {
        WEB_ERROR("%s: null entry\r\n", __func__);
        goto out0;
    }

    conn = entry->conn;
    if (conn == NULL) {
        WEB_ERROR("%s: null connect info\r\n", __func__);
        goto out1;
    }

    if (!query_device_from_address(&invoker->addr, NULL, &src_device_id)) {
        WEB_ERROR("%s: get src_deviceid from src address failed\r\n", __func__);
        goto out2;
    }

    if (src_device_id != entry->property.device_id) {
        WEB_ERROR("%s: invalid entry device_id(%d)\r\n", __func__, entry->property.device_id);
        goto out2;
    }
    
    reply = cJSON_CreateObject();
    if (reply == NULL) {
        WEB_ERROR("%s: create reply object failed\r\n", __func__);
        goto out2;
    }

    switch (apdu_type) {
    case PDU_TYPE_SIMPLE_ACK:
        web_simple_ack_handler(&(entry->property), apdu, reply);
        break;
    
    case PDU_TYPE_COMPLEX_ACK:
        web_complex_ack_handler(entry->choice, &(entry->property), apdu, reply);
        break;
        
    case PDU_TYPE_ERROR:
        web_error_handler(&(entry->property), apdu, reply);
        break;
    
    case PDU_TYPE_REJECT:
        web_reject_handler(&(entry->property), apdu, reply);
        break;
    
    case PDU_TYPE_ABORT:
        web_abort_handler(&(entry->property), apdu, reply);
        break;
    
    default:
        WEB_ERROR("%s: unknown apdu type(%d)\r\n", __func__, apdu_type);
        cJSON_AddStringToObject(reply, "reason", "unknown apdu type");
        break;
    }
    
    conn->data = (uint8_t *)cJSON_Print(reply);
    cJSON_Delete(reply);
    if (conn->data == NULL) {
        WEB_ERROR("%s: render reply cfg failed\r\n", __func__);
        goto out2;
    }

    conn->data_len = strlen((char *)conn->data) + 1;
    (void)connect_mng_echo(conn);
    goto out1;
    
out2:
    connect_mng_drop(conn);

out1:
    web_cache_entry_delete(entry);

out0:
    tsm_free_invokeID(invoker);
}

static void web_cache_entry_timer(el_timer_t *timer)
{
    web_cache_entry_t *entry;
    connect_info_t *conn;
    cJSON *reply;

    if (timer == NULL) {
        WEB_ERROR("%s: null timer\r\n", __func__);
        return;
    }
    
    entry = (web_cache_entry_t *)timer->data;
    if (entry == NULL) {
        WEB_ERROR("%s: null timer data\r\n", __func__);
        return;
    }

    if (entry->valid == false) {
        WEB_WARN("%s: invalid timer entry\r\n", __func__);
        web_cache_entry_delete(entry);
        return;
    }

    conn = entry->conn;

    reply = cJSON_CreateObject();
    if (reply == NULL) {
        WEB_ERROR("%s: create cJSON object failed\r\n", __func__);
        web_cache_entry_delete(entry);
        connect_mng_drop(conn);
        return;
    }

    cJSON_AddStringToObject(reply, "ack", entry->choice);
    cJSON_AddNumberToObject(reply, "device_id", entry->property.device_id);
    cJSON_AddNumberToObject(reply, "object_type", entry->property.object_id.type);
    cJSON_AddNumberToObject(reply, "object_instance", entry->property.object_id.instance);
    cJSON_AddNumberToObject(reply, "property_id", entry->property.property_id);
    cJSON_AddNumberToObject(reply, "array_index", (double)((int)(entry->property.array_index)));
    cJSON_AddStringToObject(reply, "reason", "operation timeout");
    
    web_cache_entry_delete(entry);
    
    conn->data = (uint8_t*)cJSON_Print(reply);
    cJSON_Delete(reply);
    if (conn->data == NULL) {
        WEB_ERROR("%s: render reply cfg failed\r\n", __func__);
        connect_mng_drop(conn);
        return;
    }
    
    conn->data_len = strlen((char*)conn->data) + 1;
    (void)connect_mng_echo(conn);
}

web_cache_entry_t *web_cache_entry_add(connect_info_t *conn, const char *choice,
                    BACNET_DEVICE_OBJECT_PROPERTY *property)
{
    web_cache_entry_t *entry;
    int i;

    entry = NULL;
    for (i = 0; i < MAX_WEB_CACHE_ENTRY_NUM; i++) {
        entry = &web_cache[i];
        if (entry->valid == false) {
            entry->timer = el_timer_create(&el_default_loop, 5000);
            if (entry->timer == NULL) {
                WEB_ERROR("%s: create timer failed\r\n", __func__);
                return NULL;
            }
            entry->timer->handler = web_cache_entry_timer;
            entry->timer->data = entry;
            
            entry->conn = conn;
            entry->choice = choice;
            memcpy(&entry->property, property, sizeof(BACNET_DEVICE_OBJECT_PROPERTY));
            entry->valid = true;
            return entry;
        }
    }

    WEB_ERROR("%s: web cache is full\r\n", __func__);

    return NULL;
}

void web_cache_entry_delete(web_cache_entry_t *entry)
{
    (void)el_timer_destroy(&el_default_loop, entry->timer);
    entry->timer = NULL;
    entry->conn = NULL;
    entry->valid = false;
}

int web_ack_init(void)
{    
    web_cache = (web_cache_entry_t *)malloc(MAX_WEB_CACHE_ENTRY_NUM * sizeof(web_cache_entry_t));
    if (web_cache == NULL) {
        WEB_ERROR("%s: malloc web_cache failed\r\n", __func__);
        return -ENOMEM;
    }
    memset(web_cache, 0, MAX_WEB_CACHE_ENTRY_NUM * sizeof(web_cache_entry_t));

    return OK;
}

void web_ack_exit(void)
{
    int i;

    for (i = 0; i < MAX_WEB_CACHE_ENTRY_NUM; i++) {
        (void)el_timer_destroy(&el_default_loop, web_cache[i].timer);
        web_cache[i].timer = NULL;
    }

    free(web_cache);
    web_cache = NULL;
}

