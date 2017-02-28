/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * msv.c
 * Original Author:  linzhixian, 2015-1-12
 *
 * Multi-state Value Object
 *
 * History
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "bacnet/object/msv.h"
#include "bacnet/bacapp.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacdef.h"
#include "bacnet/app.h"

object_impl_t *object_create_impl_msv(void)
{
    object_impl_t *msv;
    
    msv = object_create_impl_msi();
    if (!msv) {
        APP_ERROR("%s: create msi impl failed\r\n", __func__);
        return NULL;
    }
    msv->type = OBJECT_MULTI_STATE_VALUE;

    return msv;
}

static int msv_writable_write_present_value(object_instance_t *object,
            BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    object_msv_writable_t *msv;
    uint32_t value;
    uint32_t prevalue;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    msv = container_of(object, object_msv_writable_t, base.base);

    if (decode_application_unsigned(wp_data->application_data, &value) 
            != wp_data->application_data_len) {
        return BACNET_STATUS_ERROR;
    }
    
    if (value == 0 || value >= msv->number_of_states) {
        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
        return BACNET_STATUS_ERROR;
    }

    prevalue = msv->present;
    msv->present = value;
    if (prevalue != value && !msv->base.Out_Of_Service) {
        if (msv->base.notify) {
            msv->base.notify(&msv->base);
        }
    }

    return 0;
}

object_impl_t *object_create_impl_msv_writable(void)
{
    object_impl_t *msv_writable;
    property_impl_t *p_impl;
    
    msv_writable = object_create_impl_msi();
    if (!msv_writable) {
        APP_ERROR("%s: create msi impl failed\r\n", __func__);
        return NULL;
    }
    msv_writable->type = OBJECT_MULTI_STATE_VALUE;

    p_impl = object_impl_extend(msv_writable, PROP_PRESENT_VALUE, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_PRESENT_VALUE failed\r\n", __func__);
        goto out;
    }
    p_impl->write_property = msv_writable_write_present_value;

    return msv_writable;

out:
    object_impl_destroy(msv_writable);
    
    return NULL;
}

object_impl_t *object_create_impl_msv_commandable(void)
{
    object_impl_t *msv_commandable;
    
    msv_commandable = object_create_impl_mso();
    if (!msv_commandable) {
        APP_ERROR("%s: create mso impl failed\r\n", __func__);
        return NULL;
    }
    msv_commandable->type = OBJECT_MULTI_STATE_VALUE;

    return msv_commandable;
}

int __attribute__((weak)) multistate_value_init(cJSON *object)
{
    cJSON *array, *instance, *tmp;
    object_msv_t *msv;
    object_impl_t *msv_type = NULL;
    object_impl_t *msv_writable_type = NULL;
    object_impl_t *msv_commandable_type = NULL;
    object_msv_writable_t *msv_writable;
    object_msv_commandable_t *msv_commandable;
    object_instance_t *msv_instance;
    char *name;
    bool out_of_service;
    bool writable = false;
    bool commandable = false;
    uint32_t number_of_states;
    uint32_t relinquish_default;
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
            APP_ERROR("%s: Instance_List[%d] Writable/Commandable both true\r\n", __func__, i);
            goto reclaim;
        }

        if (writable) {
            if (!msv_writable_type) {
                msv_writable_type = (object_impl_t *)object_create_impl_msv_writable();
                if (!msv_writable_type) {
                    APP_ERROR("%s: create msv writable type impl failed\r\n", __func__);
                    goto reclaim;
                }
            }

            msv_writable = (object_msv_writable_t *)malloc(sizeof(object_msv_writable_t));
            if (!msv_writable) {
                APP_ERROR("%s: not enough memory\r\n", __func__);
                goto reclaim;
            }
            msv = msv_writable;
            memset(msv_writable, 0, sizeof(*msv_writable));
            msv->present = 1;
            msv->base.base.type = msv_writable_type;
        } else if (commandable) {
            tmp = cJSON_GetObjectItem(instance, "Relinquish_Default");
            if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
                APP_ERROR("%s: get Instance_List[%d] Relinquish_Default item failed\r\n", __func__, i);
                goto reclaim;
            }
            if ((tmp->valueint <= 0) || (tmp->valueint > number_of_states)) {
                APP_ERROR("%s: invalid Instance_List[%d] Relinquish_Default item value(%d)\r\n",
                    __func__, i, tmp->valueint);
                goto reclaim;
            }
            relinquish_default = (uint32_t)tmp->valueint;

            if (!msv_commandable_type) {
                msv_commandable_type = (object_impl_t *)object_create_impl_msv_commandable();
                if (!msv_commandable_type) {
                    APP_ERROR("%s: create msv commandable type impl failed\r\n", __func__);
                    goto reclaim;
                }
            }

            msv_commandable = (object_msv_commandable_t *)malloc(sizeof(object_msv_commandable_t));
            if (!msv_commandable) {
                APP_ERROR("%s: not enough memory\r\n", __func__);
                goto reclaim;
            }
            msv = &msv_commandable->base;
            memset(msv_commandable, 0, sizeof(*msv_commandable));
            msv_commandable->relinquish_default = relinquish_default;
            msv_commandable->active_bit = BACNET_MAX_PRIORITY;
            msv->present = relinquish_default;
            msv->base.base.type = msv_commandable_type;
        } else {
            if (!msv_type) {
                msv_type = (object_impl_t *)object_create_impl_msv();
                if (!msv_type) {
                    APP_ERROR("%s: create msv type impl failed\r\n", __func__);
                    goto reclaim;
                }
            }

            msv = (object_msv_t *)malloc(sizeof(object_msv_t));
            if (!msv) {
                APP_ERROR("%s: not enough memory\r\n", __func__);
                goto reclaim;
            }
            memset(msv, 0, sizeof(*msv));
            msv->present = 1;
            msv->base.base.type = msv_type;
        }

        msv->base.base.instance = i;
        msv->number_of_states = number_of_states;
        msv->base.Out_Of_Service = out_of_service;

        if (!vbuf_fr_str(&msv->base.base.object_name.vbuf, name, OBJECT_NAME_MAX_LEN)) {
            APP_ERROR("%s: set object name overflow\r\n", __func__);
            free(msv);
            goto reclaim;
        }

        if (!object_add(&msv->base.base)) {
            APP_ERROR("%s: object add failed\r\n", __func__);
            free(msv);
            goto reclaim;
        }
        i++;
    }

end:
    return OK;

reclaim:
    for (i = i - 1; i >= 0; i--) {
        msv_instance = object_find(OBJECT_MULTI_STATE_VALUE, i);
        if (!msv_instance) {
            APP_ERROR("%s: reclaim failed\r\n", __func__);
        } else {
            object_detach(msv_instance);
            free(msv_instance);
        }
    }

    if (msv_type) {
        object_impl_destroy(msv_type);
    }
    
    if (msv_writable_type) {
        object_impl_destroy(msv_writable_type);
    }
    
    if (msv_commandable_type) {
        object_impl_destroy(msv_commandable_type);
    }
    
out:
    return -EPERM;
}

