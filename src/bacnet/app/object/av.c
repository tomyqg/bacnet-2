/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * av.c
 * Original Author:  linzhixian, 2015-1-12
 *
 * Analog Value Object
 *
 * History
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "bacnet/object/av.h"
#include "bacnet/bacenum.h"
#include "bacnet/bacapp.h"
#include "bacnet/bacdcode.h"
#include "bacnet/app.h"

object_impl_t *object_create_impl_av(void)
{
    object_impl_t *av;
    
    av = object_create_impl_ai();
    if (!av) {
        APP_ERROR("%s: create ai impl failed\r\n", __func__);
        return NULL;
    }
    av->type = OBJECT_ANALOG_VALUE;

    return av;
}

static int av_writable_write_present_value(object_instance_t *object,
                BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    object_av_writable_t *av;
    float value;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    av = container_of(object, object_av_writable_t, base.base);

    if (decode_application_real(wp_data->application_data, &value)
            != wp_data->application_data_len) {
        return BACNET_STATUS_ERROR;
    }
    
    float prevalue = av->present;
    av->present = value;

    if (prevalue != value && !av->base.Out_Of_Service) {
        if (av->base.notify)
            av->base.notify(&av->base);
    }

    return 0;
}

object_impl_t *object_create_impl_av_writable(void)
{
    object_impl_t *av_writable;
    property_impl_t *p_impl;

    av_writable = object_create_impl_ai();
    if (!av_writable) {
        APP_ERROR("%s: create ai impl failed\r\n", __func__);
        return NULL;
    }
    av_writable->type = OBJECT_ANALOG_VALUE;

    p_impl = object_impl_extend(av_writable, PROP_PRESENT_VALUE, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_PRESENT_VALUE failed\r\n", __func__);
        goto out;
    }
    p_impl->write_property = av_writable_write_present_value;
    
    return av_writable;

out:
    object_impl_destroy(av_writable);
    
    return NULL;
}

object_impl_t *object_create_impl_av_commandable(void)
{
    object_impl_t *av_commandable;

    av_commandable = object_create_impl_ao();
    if (!av_commandable) {
        APP_ERROR("%s: create ao impl failed\r\n", __func__);
        return NULL;
    }
    av_commandable->type = OBJECT_ANALOG_VALUE;
    
    return av_commandable;
}

int __attribute__((weak)) analog_value_init(cJSON *object)
{
    object_impl_t *av_type = NULL;
    object_impl_t *av_writable_type = NULL;
    object_impl_t *av_commandable_type = NULL;
    object_av_writable_t *av_writable;
    object_av_commandable_t *av_commandable;
    object_av_t *av;
    object_instance_t *av_instance;
    cJSON *array, *instance, *tmp;
    int i;
    char *name;
    bool out_of_service;
    bool writable = false;
    bool commandable = false;
    BACNET_ENGINEERING_UNITS units;
    float relinquish_default;
    
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

        tmp = cJSON_GetObjectItem(instance, "Writable");
        if (tmp) {
            if ((tmp->type != cJSON_False) && (tmp->type != cJSON_True)) {
                APP_ERROR("%s: Instance_List[%d] Writable not boolean\r\n", __func__, i);
                goto reclaim;
            }
            writable = (tmp->type == cJSON_True)? true: false;
        }

        tmp = cJSON_GetObjectItem(instance, "Commandable");
        if (tmp) {
            if ((tmp->type != cJSON_False) && (tmp->type != cJSON_True)) {
                APP_ERROR("%s: Instance_List[%d] Commandable not boolean\r\n", __func__, i);
                goto reclaim;
            }
            commandable = (tmp->type == cJSON_True)? true: false;
        }

        if (writable && commandable) {
            APP_ERROR("%s: Instance_List[%d] Writable/Commandable item both true\r\n", __func__, i);
            goto reclaim;
        }

        if (writable) {
            if (!av_writable_type) {
                av_writable_type = (object_impl_t *)object_create_impl_av_writable();
                if (!av_writable_type) {
                    APP_ERROR("%s: create av writable type impl failed\r\n", __func__);
                    goto reclaim;
                }
            }
            av_writable = (object_av_writable_t *)malloc(sizeof(object_av_writable_t));
            if (!av_writable) {
                APP_ERROR("%s: not enough memory\r\n", __func__);
                goto reclaim;
            }
            av = av_writable;
            memset(av_writable, 0, sizeof(*av_writable));
            av->present = 0.0;
            av->base.base.type = av_writable_type;
        } else if (commandable) {
            tmp = cJSON_GetObjectItem(instance, "Relinquish_Default");
            if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
                APP_ERROR("%s: get Instance_List[%d] Relinquish_Default item failed\r\n", __func__, i);
                goto reclaim;
            }
            relinquish_default = tmp->valuedouble;

            if (!av_commandable_type) {
                av_commandable_type = (object_impl_t *)object_create_impl_av_commandable();
                if (!av_commandable_type) {
                    APP_ERROR("%s: create av commandable type impl failed\r\n", __func__);
                    goto reclaim;
                }
            }

            av_commandable = (object_av_commandable_t *)malloc(sizeof(object_av_commandable_t));
            if (!av_commandable) {
                APP_ERROR("%s: not enough memory\r\n", __func__);
                goto reclaim;
            }
            av = &av_commandable->base;
            memset(av_commandable, 0, sizeof(*av_commandable));
            av_commandable->relinquish_default = relinquish_default;
            av_commandable->active_bit = BACNET_MAX_PRIORITY;
            av->present = relinquish_default;
            av->base.base.type = av_commandable_type;
        } else {
            if (!av_type) {
                av_type = (object_impl_t *)object_create_impl_av();
                if (!av_type) {
                    APP_ERROR("%s: create av type impl failed\r\n", __func__);
                    goto reclaim;
                }
            }
            av = (object_av_t *)malloc(sizeof(object_av_t));
            if (!av) {
                APP_ERROR("%s: not enough memory\r\n", __func__);
                goto reclaim;
            }
            memset(av, 0, sizeof(*av));
            av->present = 0.0;
            av->base.base.type = av_type;
        }

        av->base.base.instance = i;
        av->units = units;
        av->base.Out_Of_Service = out_of_service;

        if (!vbuf_fr_str(&av->base.base.object_name.vbuf, name, OBJECT_NAME_MAX_LEN)) {
            APP_ERROR("%s: set object name overflow\r\n", __func__);
            free(av);
            goto reclaim;
        }

        if (!object_add(&av->base.base)) {
            APP_ERROR("%s: object add failed\r\n", __func__);
            free(av);
            goto reclaim;
        }
        i++;
    }

end:
    return OK;

reclaim:
    for (i = i - 1; i >= 0; i--) {
        av_instance = object_find(OBJECT_ANALOG_VALUE, i);
        if (!av_instance) {
            APP_ERROR("%s: reclaim failed\r\n", __func__);
        } else {
            object_detach(av_instance);
            free(av_instance);
        }
    }

    if (av_type) {
        object_impl_destroy(av_type);
    }
    
    if (av_writable_type) {
        object_impl_destroy(av_writable_type);
    }
    
    if (av_commandable_type) {
        object_impl_destroy(av_commandable_type);
    }
    
out:

    return -EPERM;
}

