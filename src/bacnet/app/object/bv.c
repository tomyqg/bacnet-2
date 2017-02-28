/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bv.c
 * Original Author:  linzhixian, 2015-1-12
 *
 * Binary Value Object
 *
 * History
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "bacnet/object/bv.h"
#include "bacnet/bacapp.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacdef.h"
#include "bacnet/app.h"

object_impl_t *object_create_impl_bv(void)
{
    object_impl_t *bv;

    bv = object_create_impl_bi();
    if (!bv) {
        APP_ERROR("%s: create bi impl failed\r\n", __func__);
        return NULL;
    }
    bv->type = OBJECT_BINARY_VALUE;
    
    return bv;
}

static int bv_writable_write_present_value(object_instance_t *object,
                BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    object_bv_writable_t *bv;
    uint32_t value;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    bv = container_of(object, object_bv_writable_t, base.base);

    if (decode_application_enumerated(wp_data->application_data, &value)
            != wp_data->application_data_len)
        return BACNET_STATUS_ERROR;

    if (value >= MAX_BINARY_PV) {
        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
        return BACNET_STATUS_ERROR;
    }

    BACNET_BINARY_PV prevalue = bv->present;
    bv->present = value;
    if (prevalue != value && !bv->base.Out_Of_Service) {
        if (bv->base.notify)
            bv->base.notify(&bv->base);
    }

    return 0;
}

object_impl_t *object_create_impl_bv_writable(void)
{
    object_impl_t *bv_writable;
    property_impl_t *p_impl;

    bv_writable = object_create_impl_bi();
    if (!bv_writable) {
        APP_ERROR("%s: create bi impl failed\r\n", __func__);
        return NULL;
    }
    bv_writable->type = OBJECT_BINARY_VALUE;

    p_impl = object_impl_extend(bv_writable,
            PROP_PRESENT_VALUE, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_PRESENT_VALUE failed\r\n", __func__);
        goto out;
    }
    p_impl->write_property = bv_writable_write_present_value;

    return bv_writable;

out:
    object_impl_destroy(bv_writable);
    
    return NULL;
}

object_impl_t *object_create_impl_bv_commandable(void)
{
    object_impl_t *bv_commandable;

    bv_commandable = object_create_impl_bo();
    if (!bv_commandable) {
        APP_ERROR("%s: clone bo impl failed\r\n", __func__);
        return NULL;
    }
    bv_commandable->type = OBJECT_BINARY_VALUE;
    
    return bv_commandable;
}

int __attribute__((weak)) binary_value_init(cJSON *object)
{
    cJSON *array, *instance, *tmp;
    int i;
    char *name;
    bool out_of_service;
    bool writable = false;
    bool commandable = false;
    BACNET_POLARITY polarity;
    object_instance_t *bv_instance;
    object_impl_t *bv_type = NULL;
    object_impl_t *bv_writable_type = NULL;
    object_impl_t *bv_commandable_type = NULL;
    object_bv_t *bv;
    object_bv_writable_t *bv_writable;
    object_bv_commandable_t *bv_commandable;
    BACNET_BINARY_PV relinquish_default;
    
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
        if ((tmp->valueint < MIN_POLARITY) || (tmp->valueint >= MAX_POLARITY)) {
            APP_ERROR("%s: invalid Instance_List[%d] Polarity item value(%d)\r\n", __func__, i,
                tmp->valueint);
            goto reclaim;
        }
        polarity = (BACNET_POLARITY)tmp->valueint;

        tmp = cJSON_GetObjectItem(instance, "Writable");
        if (tmp != NULL) {
            if ((tmp->type != cJSON_False) && (tmp->type != cJSON_True)) {
                APP_ERROR("%s: Instance_List[%d] Writable not boolean\r\n", __func__, i);
                goto reclaim;
            }
            writable = (tmp->type == cJSON_True)? true: false;
        }

        tmp = cJSON_GetObjectItem(instance, "Commandable");
        if (tmp != NULL) {
            if ((tmp->type != cJSON_False) && (tmp->type != cJSON_True)) {
                APP_ERROR("%s: Instance_List[%d] Commandable not boolean\r\n", __func__, i);
                goto reclaim;
            }
            commandable = (tmp->type == cJSON_True)? true: false;
        }

        if (writable && commandable) {
            APP_ERROR("%s: Instance_List[%d] Writable/Commandable both true\r\n", __func__, i);
            goto reclaim;
        }

        if (writable) {
            if (!bv_writable_type) {
                bv_writable_type = (object_impl_t *)object_create_impl_bv_writable();
                if (!bv_writable_type) {
                    APP_ERROR("%s: create bv writable type impl failed\r\n", __func__);
                    goto reclaim;
                }
            }

            bv_writable = (object_bv_writable_t *)malloc(sizeof(object_bv_writable_t));
            if (!bv_writable) {
                APP_ERROR("%s: not enough memory\r\n", __func__);
                goto reclaim;
            }
            bv = bv_writable;
            memset(bv_writable, 0, sizeof(*bv_writable));
            bv->present = 0;
            bv->base.base.type = bv_writable_type;
        } else if (commandable) {
            tmp = cJSON_GetObjectItem(instance, "Relinquish_Default");
            if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
                APP_ERROR("%s: get Instance_List[%d] Relinquish_Default item failed\r\n", __func__, i);
                goto reclaim;
            }
            if ((tmp->valueint < MIN_BINARY_PV) || (tmp->valueint >= MAX_BINARY_PV)) {
                APP_ERROR("%s: invalid Instance_List[%d] Relinquish_Default item value(%d)\r\n",
                    __func__, i, tmp->valueint);
                goto reclaim;
            }
            relinquish_default = (BACNET_BINARY_PV)tmp->valueint;

            if (!bv_commandable_type) {
                bv_commandable_type = (object_impl_t *)object_create_impl_bv_commandable();
                if (!bv_commandable_type) {
                    APP_ERROR("%s: create bv commandable type impl failed\r\n", __func__);
                    goto reclaim;
                }
            }

            bv_commandable = (object_bv_commandable_t *)malloc(sizeof(object_bv_commandable_t));
            if (!bv_commandable) {
                APP_ERROR("%s: not enough memory\r\n", __func__);
                goto reclaim;
            }
            bv = &bv_commandable->base;
            memset(bv_commandable, 0, sizeof(*bv_commandable));
            bv_commandable->relinquish_default = relinquish_default;
            bv_commandable->active_bit = BACNET_MAX_PRIORITY;
            bv->present = relinquish_default;
            bv->base.base.type = bv_commandable_type;
        } else {
            if (!bv_type) {
                bv_type = (object_impl_t *)object_create_impl_bv();
                if (!bv_type) {
                    APP_ERROR("%s: create bv type impl failed\r\n", __func__);
                    goto reclaim;
                }
            }

            bv = (object_bv_t *)malloc(sizeof(object_bv_t));
            if (!bv) {
                APP_ERROR("%s: not enough memory\r\n", __func__);
                goto reclaim;
            }
            memset(bv, 0, sizeof(*bv));
            bv->present = 0;
            bv->base.base.type = bv_type;
        }

        bv->base.base.instance = i;
        bv->polarity = polarity;
        bv->base.Out_Of_Service = out_of_service;

        if (!vbuf_fr_str(&bv->base.base.object_name.vbuf, name, OBJECT_NAME_MAX_LEN)) {
            APP_ERROR("%s: set object name overflow\r\n", __func__);
            free(bv);
            goto reclaim;
        }

        if (!object_add(&bv->base.base)) {
            APP_ERROR("%s: object add failed\r\n", __func__);
            free(bv);
            goto reclaim;
        }
        i++;
    }

end:
    return OK;

reclaim:
    for (i = i - 1; i >= 0; i--) {
        bv_instance = object_find(OBJECT_BINARY_VALUE, i);
        if (!bv_instance) {
            APP_ERROR("%s: reclaim failed\r\n", __func__);
        } else {
            object_detach(bv_instance);
            free(bv_instance);
        }
    }

    if (bv_type) {
        object_impl_destroy(bv_type);
    }
    
    if (bv_writable_type) {
        object_impl_destroy(bv_writable_type);
    }
    
    if (bv_commandable_type) {
        object_impl_destroy(bv_commandable_type);
    }
    
out:
    return -EPERM;
}

