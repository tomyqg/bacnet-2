/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bi.c
 * Original Author:  linzhixian, 2015-1-12
 *
 * Binary Input Object
 *
 * History
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "bacnet/object/bi.h"
#include "bacnet/bacapp.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacdef.h"
#include "bacnet/app.h"

static int bi_read_present_value(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_bi_t *bi_obj;
    
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    bi_obj = container_of(object, object_bi_t, base.base);

    return encode_application_enumerated(rp_data->application_data, bi_obj->present);
}

static int bi_write_present_value(object_instance_t *object, BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    object_bi_t *bi_obj;
    uint32_t value;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    bi_obj = container_of(object, object_bi_t, base.base);
    if (!bi_obj->base.Out_Of_Service) {
        wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
        return BACNET_STATUS_ERROR;
    }

    if (decode_application_enumerated(wp_data->application_data, &value)
            != wp_data->application_data_len)
        return BACNET_STATUS_ERROR;

    if (value >= MAX_BINARY_PV) {
        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
        return BACNET_STATUS_ERROR;
    }

    bi_obj->present = value;
    
    return 0;
}

static int bi_read_polarity(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_bi_t *bi_obj;
    
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    bi_obj = container_of(object, object_bi_t, base.base);

    return encode_application_enumerated(rp_data->application_data, bi_obj->polarity);
}

object_impl_t *object_create_impl_bi(void)
{
    property_impl_t *p_impl;
    object_impl_t *bi;
    
    bi = object_create_impl_seor(true, true, true);
    if (!bi) {
        APP_ERROR("%s: create SEOR impl failed\r\n", __func__);
        return NULL;
    }
    bi->type = OBJECT_BINARY_INPUT;

    p_impl = object_impl_extend(bi, PROP_PRESENT_VALUE, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_PRESENT_VALUE failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = bi_read_present_value;
    p_impl->write_property = bi_write_present_value;

    p_impl = object_impl_extend(bi, PROP_POLARITY, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_POLARITY failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = bi_read_polarity;
    
    return bi;

out:
    object_impl_destroy(bi);
    
    return NULL;
}

int __attribute__((weak)) binary_input_init(cJSON *object)
{
    cJSON *array, *instance, *tmp;
    int i;
    object_impl_t *bi_type = NULL;
    object_bi_t *bi;
    object_instance_t *bi_instance;
    char *name;
    bool out_of_service;
    BACNET_POLARITY polarity;
        
    if (object == NULL) {
        goto end;
    }

    array = cJSON_GetObjectItem(object, "Instance_List");
    if ((array == NULL) || (array->type != cJSON_Array)) {
        APP_ERROR("%s: get Instance_List item failed\r\n", __func__);
        goto out;
    }

    i = 0;
    cJSON_ArrayForEach(instance, array) {
        if (instance->type != cJSON_Object) {
            APP_ERROR("%s: invalid Instance_List[%d] item type\r\n", __func__, i);
            goto reclaim;
        }

        tmp = cJSON_GetObjectItem(instance, "Name");
        if ((tmp == NULL) || (tmp->type != cJSON_String)) {
            APP_ERROR("%s: get Instance_List[%d] Name item failed\r\n", __func__, i);
            goto reclaim;
        }
        name = tmp->valuestring;

        tmp = cJSON_GetObjectItem(instance, "Out_Of_Service");
        if ((tmp == NULL) || ((tmp->type != cJSON_False) && (tmp->type != cJSON_True))) {
            APP_ERROR("%s: get Instance_List[%d] Out_Of_Service item failed\r\n", __func__, i);
            goto reclaim;
        }
        out_of_service = (tmp->type == cJSON_True)? true: false;

        tmp = cJSON_GetObjectItem(instance, "Polarity");
        if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
            APP_ERROR("%s: get Instance_List[%d] Polarity item failed\r\n", __func__, i);
            goto reclaim;
        }
        if ((tmp->valueint < 0) || (tmp->valueint >= MAX_POLARITY)) {
            APP_ERROR("%s: invalid Instance_List[%d] Polarity item value(%d)\r\n", __func__, i,
                tmp->valueint);
            goto reclaim;
        }
        polarity = (BACNET_POLARITY)tmp->valueint;

        if (!bi_type) {
            bi_type = (object_impl_t *)object_create_impl_bi();
            if (!bi_type) {
                APP_ERROR("%s: create bi type impl failed\r\n", __func__);
                goto reclaim;
            }
        }

        bi = (object_bi_t *)malloc(sizeof(object_bi_t));
        if (!bi) {
            APP_ERROR("%s: not enough memory\r\n", __func__);
            goto reclaim;
        }
        memset(bi, 0, sizeof(*bi));
        bi->base.base.instance = i;
        bi->base.base.type = bi_type;
        bi->base.Out_Of_Service = out_of_service;
        bi->present = 0;
        bi->polarity = polarity;

        if (!vbuf_fr_str(&bi->base.base.object_name.vbuf, name, OBJECT_NAME_MAX_LEN)) {
            APP_ERROR("%s: set object name overflow\r\n", __func__);
            free(bi);
            goto reclaim;
        }

        if (!object_add(&bi->base.base)) {
            APP_ERROR("%s: object add failed\r\n", __func__);
            free(bi);
            goto reclaim;
        }
        i++;
    }

end:
    return OK;

reclaim:
    for (i = i - 1; i >= 0; i--) {
        bi_instance = object_find(OBJECT_BINARY_INPUT, i);
        if (!bi_instance) {
            APP_ERROR("%s: reclaim failed\r\n", __func__);
        } else {
            object_detach(bi_instance);
            free(bi_instance);
        }
    }

    if (bi_type) {
        object_impl_destroy(bi_type);
    }

out:
    return -EPERM;
}

