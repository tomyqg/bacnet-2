/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * ai.c
 * Original Author:  linzhixian, 2015-1-12
 *
 * Analog Input Object
 *
 * History
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "bacnet/object/ai.h"
#include "bacnet/bacapp.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacdef.h"
#include "bacnet/app.h"

static int ai_read_present_value(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data, 
            RR_RANGE *range)
{
    object_ai_t *ai_obj;

    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    ai_obj = container_of(object, object_ai_t, base.base);
    
    return encode_application_real(rp_data->application_data, ai_obj->present);
}

static int ai_write_present_value(object_instance_t *object, BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    object_ai_t *ai_obj;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    ai_obj = container_of(object, object_ai_t, base.base);
    if (!ai_obj->base.Out_Of_Service) {
        wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
        return BACNET_STATUS_ERROR;
    }

    if (decode_application_real(wp_data->application_data, &ai_obj->present)
            != wp_data->application_data_len) {
        return BACNET_STATUS_ERROR;
    }
    
    return 0;
}

static int ai_read_units(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_ai_t *ai_obj;
    
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    ai_obj = container_of(object, object_ai_t, base.base);

    return encode_application_enumerated(rp_data->application_data, ai_obj->units);
}

object_impl_t *object_create_impl_ai(void)
{
    object_impl_t *ai;
    property_impl_t *p_impl;
    
    ai = object_create_impl_seor(true, true, true);
    if (!ai) {
        APP_ERROR("%s: create SEOR impl failed\r\n", __func__);
        return NULL;
    }
    ai->type = OBJECT_ANALOG_INPUT;

    p_impl = object_impl_extend(ai, PROP_PRESENT_VALUE, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_PRESENT_VALUE failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = ai_read_present_value;
    p_impl->write_property = ai_write_present_value;

    p_impl = object_impl_extend(ai, PROP_UNITS, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_UNITS failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = ai_read_units;

    return ai;

out:
    object_impl_destroy(ai);
    
    return NULL;
}

int __attribute__((weak)) analog_input_init(cJSON *object)
{
    object_impl_t *ai_type = NULL;
    object_instance_t *ai_instance;
    object_ai_t *ai;
    cJSON *array, *instance, *tmp;
    char *name;
    bool out_of_service;
    BACNET_ENGINEERING_UNITS units;
    int i;
    
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

        tmp = cJSON_GetObjectItem(instance, "Units");
        if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
            APP_ERROR("%s: get Instance_List[%d] Units item failed\r\n", __func__, i);
            goto reclaim;
        }
        if ((tmp->valueint < 0) || (tmp->valueint > UNITS_PROPRIETARY_RANGE_MAX)) {
            APP_ERROR("%s: invalid Instance_List[%d] Units item value(%d)\r\n", __func__, i,
                tmp->valueint);
            goto reclaim;
        }
        units = (BACNET_ENGINEERING_UNITS)tmp->valueint;

        if (!ai_type) {
            ai_type = (object_impl_t *)object_create_impl_ai();
            if (!ai_type) {
                APP_ERROR("%s: create ai type impl failed\r\n", __func__);
                goto reclaim;
            }
        }

        ai = (object_ai_t *)malloc(sizeof(object_ai_t));
        if (!ai) {
            APP_ERROR("%s: not enough memory\r\n", __func__);
            goto reclaim;
        }
        memset(ai, 0, sizeof(*ai));
        ai->base.base.instance = i;
        ai->base.base.type = ai_type;
        ai->base.Out_Of_Service = out_of_service;
        ai->present = 0.0f;
        ai->units = units;

        if (!vbuf_fr_str(&ai->base.base.object_name.vbuf, name, OBJECT_NAME_MAX_LEN)) {
            APP_ERROR("%s: set object name overflow\r\n", __func__);
            free(ai);
            goto reclaim;
        }

        if (!object_add(&ai->base.base)) {
            APP_ERROR("%s: object add failed\r\n", __func__);
            free(ai);
            goto reclaim;
        }
        i++;
    }

end:
    return OK;

reclaim:
    for (i = i - 1; i >= 0; i--) {
        ai_instance = object_find(OBJECT_ANALOG_INPUT, i);
        if (!ai_instance) {
            APP_ERROR("%s: reclaim failed\r\n", __func__);
        } else {
            object_detach(ai_instance);
            free(ai_instance);
        }
    }

    if (ai_type) {
        object_impl_destroy(ai_type);
    }

out:

    return -EPERM;
}

