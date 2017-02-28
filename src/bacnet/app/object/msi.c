/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * msi.c
 * Original Author:  linzhixian, 2015-1-12
 *
 * Multi-state Input Object
 *
 * History
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "bacnet/object/msi.h"
#include "bacnet/bacapp.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacdef.h"
#include "bacnet/app.h"

static int msi_read_present_value(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_msi_t *msi;
    
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    msi = container_of(object, object_msi_t, base.base);

    return encode_application_unsigned(rp_data->application_data, msi->present);
}

static int msi_write_present_value(object_instance_t *object, BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    object_msi_t *msi;
    uint32_t value;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    msi = container_of(object, object_msi_t, base.base);
    if (!msi->base.Out_Of_Service) {
        wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
        return BACNET_STATUS_ERROR;
    }

    if (decode_application_unsigned(wp_data->application_data, &value)
            != wp_data->application_data_len)
        return BACNET_STATUS_ERROR;

    if (value == 0 || value > msi->number_of_states) {
        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
        return BACNET_STATUS_ERROR;
    }

    msi->present = value;
    
    return 0;
}

static int msi_read_number_of_states(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_msi_t *msi;
    
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    msi = container_of(object, object_msi_t, base.base);

    return encode_application_unsigned(rp_data->application_data, msi->number_of_states);
}

object_impl_t *object_create_impl_msi(void)
{
    object_impl_t *msi;
    property_impl_t *p_impl;
    
    msi = object_create_impl_seor(true, true, true);
    if (!msi) {
        APP_ERROR("%s: create SEOR impl failed\r\n", __func__);
        return NULL;
    }
    msi->type = OBJECT_MULTI_STATE_INPUT;

    p_impl = object_impl_extend(msi, PROP_PRESENT_VALUE, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_PRESENT_VALUE failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = msi_read_present_value;
    p_impl->write_property = msi_write_present_value;

    p_impl = object_impl_extend(msi, PROP_NUMBER_OF_STATES, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_NUMBER_OF_STATES failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = msi_read_number_of_states;

    return msi;

out:
    object_impl_destroy(msi);
    
    return NULL;
}

int __attribute__((weak)) multistate_input_init(cJSON *object)
{
    cJSON *array, *instance, *tmp;
    object_impl_t *msi_type = NULL;
    object_msi_t *msi;
    object_instance_t *msi_instance;
    char *name;
    bool out_of_service;
    uint32_t number_of_states;
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

        tmp = cJSON_GetObjectItem(instance, "Number_Of_States");
        if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
            APP_ERROR("%s: get Instance_List[%d] Number_Of_States item failed\r\n", __func__, i);
            goto reclaim;
        }
        if (tmp->valueint <= 0) {
            APP_ERROR("%s: invalid Instance_List[%d] Number_Of_States item value(%d)\r\n", __func__,
                i, tmp->valueint);
            goto reclaim;
        }
        number_of_states = (uint32_t)tmp->valueint;

        if (!msi_type) {
            msi_type = (object_impl_t *)object_create_impl_msi();
            if (!msi_type) {
                APP_ERROR("%s: create msi type impl failed\r\n", __func__);
                goto reclaim;
            }
        }

        msi = (object_msi_t *)malloc(sizeof(object_msi_t));
        if (!msi) {
            APP_ERROR("%s: not enough memory\r\n", __func__);
            goto reclaim;
        }
        memset(msi, 0, sizeof(*msi));
        msi->base.base.instance = i;
        msi->base.base.type = msi_type;
        msi->base.Out_Of_Service = out_of_service;
        msi->present = 1;
        msi->number_of_states = number_of_states;

        if (!vbuf_fr_str(&msi->base.base.object_name.vbuf, name, OBJECT_NAME_MAX_LEN)) {
            APP_ERROR("%s: set object name overflow\r\n", __func__);
            free(msi);
            goto reclaim;
        }

        if (!object_add(&msi->base.base)) {
            APP_ERROR("%s: object add failed\r\n", __func__);
            free(msi);
            goto reclaim;
        }
        i++;
    }

end:
    return OK;

reclaim:
    for (i = i - 1; i >= 0; i--) {
        msi_instance = object_find(OBJECT_MULTI_STATE_INPUT, i);
        if (!msi_instance) {
            APP_ERROR("%s: reclaim failed\r\n", __func__);
        } else {
            object_detach(msi_instance);
            free(msi_instance);
        }
    }

    if (msi_type) {
        object_impl_destroy(msi_type);
    }
    
out:
    return -EPERM;
}

