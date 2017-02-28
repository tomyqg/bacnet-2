/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * ao.c
 * Original Author:  linzhixian, 2015-1-12
 *
 * Analog Output Object
 *
 * History
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "bacnet/object/ao.h"
#include "bacnet/bacapp.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacdef.h"
#include "bacnet/app.h"

bool analog_output_present_value_set(object_ao_t *ao, float value, uint8_t priority)
{
    int idx;
    
    if (!ao) {
        APP_ERROR("%s: null argument\r\n", __func__);
        return false;
    }

    if (priority < BACNET_MIN_PRIORITY || priority > BACNET_MAX_PRIORITY) {
        APP_ERROR("%s: invalid priority(%d)\r\n\r\n", __func__, priority);
        return false;
    }

    idx = priority - 1;
    ao->priority_array[idx] = value;
    ao->priority_bits |= 1 << idx;

    if (idx <= ao->active_bit) {
        ao->active_bit = idx;
        float prevalue = ao->base.present;
        ao->base.present = value;
        if (prevalue != value && !ao->base.base.Out_Of_Service) {
            if (ao->base.base.notify)
                ao->base.base.notify(&ao->base.base);
        }
    }

    return true;
}

bool analog_output_present_value_relinquish(object_ao_t *ao, uint8_t priority)
{
    int idx, i;
    
    if (!ao) {
        APP_ERROR("%s: null argument\r\n", __func__);
        return false;
    }

    if (priority < BACNET_MIN_PRIORITY || priority > BACNET_MAX_PRIORITY) {
        APP_ERROR("%s: invalid priority(%d)\r\n\r\n", __func__, priority);
        return false;
    }

    idx = priority - 1;
    ao->priority_bits &= ~(1 << idx);

    if (idx != ao->active_bit)
        return true;

    float prevalue = ao->base.present;
    for (i = idx + 1; i < BACNET_MAX_PRIORITY; ++i) {
        if (ao->priority_bits & (1 << i)) {
            ao->active_bit = i;
            ao->base.present = ao->priority_array[i];
            goto end;
        }
    }
    ao->active_bit = BACNET_MAX_PRIORITY;
    ao->base.present = ao->relinquish_default;

end:
    if (prevalue != ao->base.present && !ao->base.base.Out_Of_Service) {
        if (ao->base.base.notify)
            ao->base.base.notify(&ao->base.base);
    }

    return true;
}

static int ao_write_present_value(object_instance_t *object, BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    object_ao_t *ao;
    float value;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    /* Command priority 6 is reserved for use by Minimum On/Off algorithm 
        and may not be used for other purposes in any object. */
    if (wp_data->priority == 6) {
        wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
        return BACNET_STATUS_ERROR;
    }

    ao = container_of(object, object_ao_t, base.base.base);

    if (decode_application_real(wp_data->application_data, &value)
            != wp_data->application_data_len) {
        if (decode_application_null(wp_data->application_data) != wp_data->application_data_len) {
            return BACNET_STATUS_ERROR;
        } else {
            analog_output_present_value_relinquish(ao, wp_data->priority);
        }
    } else {
        analog_output_present_value_set(ao, value, wp_data->priority);
    }

    return 0;
}

static int ao_read_relinquish_default(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_ao_t *ao;
    
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    ao = container_of(object, object_ao_t, base.base.base);

    return encode_application_real(rp_data->application_data, ao->relinquish_default);
}

static int ao_read_priority_array_RR(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_ao_t *ao;
    BACNET_BIT_STRING ResultFlags;
    uint8_t *pdu;
    int index;
    int item_count;
    int pdu_len, item_data_len;
    uint8_t item_data[MAX_APDU];
    uint8_t value;

    if ((range->RequestType != RR_BY_POSITION) && (range->RequestType != RR_READ_ALL)) {
        APP_ERROR("%s: invalid RR_RequestType(%d)\r\n", __func__, range->RequestType);
        rp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
        return BACNET_STATUS_ERROR;
    }

    ao = container_of(object, object_ao_t, base.base.base);
    item_count = sizeof(ao->priority_array) / sizeof(ao->priority_array[0]);
    
    if (range->RequestType == RR_BY_POSITION) {
        if (range->Count < 0) {
            index = range->Range.RefIndex;
            index += range->Count + 1;
            if (index < 1) {
                range->Count = range->Range.RefIndex;
                range->Range.RefIndex = 1;
            } else {
                range->Count = -range->Count;
                range->Range.RefIndex = index;
            }
        }
    } else {
        range->Range.RefIndex = 1;
        range->Count = item_count;
    }

    if (range->Range.RefIndex > item_count) {
        APP_ERROR("%s: invalid RR RefIndex(%d)\r\n", __func__, range->Range.RefIndex);
        rp_data->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
        return BACNET_STATUS_ERROR;
    }

    if (range->Range.RefIndex == 0) {
        range->Range.RefIndex = 1;
        range->Count = range->Count - 1;
    }

    if (range->Count == 0) {
        APP_ERROR("%s: invalid RR Count(%d)\r\n", __func__, range->Count);
        rp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
        return BACNET_STATUS_ERROR;
    }
    
    if ((range->Range.RefIndex + range->Count - 1) > item_count) {
        range->Count = item_count + 1 - range->Range.RefIndex;
    }
    
    item_count = 0;
    item_data_len = 0;
    value = 0;
    (void)bitstring_init(&ResultFlags, &value, 3);

    index = range->Range.RefIndex - 1;
    while (1) {
        if ((item_data_len + 5) > sizeof(item_data)) {
            bitstring_set_bit(&ResultFlags, RESULT_FLAG_MORE_ITEMS, true);
            break;
        }

        if (ao->priority_bits & (1 << index)) {
            item_data_len += encode_application_real(&item_data[item_data_len],
                ao->priority_array[index]);
        } else {
            item_data_len += encode_application_null(&item_data[item_data_len]);
        }
        
        item_count++;
        if (item_count >= range->Count) {
            bitstring_set_bit(&ResultFlags, RESULT_FLAG_LAST_ITEM, true);
            break;
        }
        index++;
    }

    pdu = rp_data->application_data;
    if (item_count != 0) {
        bitstring_set_bit(&ResultFlags, RESULT_FLAG_FIRST_ITEM, true);
    }

    /* Context 3 BACnet Result Flags */
    pdu_len = encode_context_bitstring(pdu, 3, &ResultFlags);
    
    /* Context 4 Item Count */
    pdu_len += encode_context_unsigned(&pdu[pdu_len], 4, item_count);

    /* Context 5 Property list */
    pdu_len += encode_opening_tag(&pdu[pdu_len], 5);
    if (item_count != 0) {
        if (pdu_len + item_data_len >= rp_data->application_data_len) {
            rp_data->abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
            return BACNET_STATUS_ABORT;
        }
        
        memcpy(&pdu[pdu_len], item_data, item_data_len);
        pdu_len += item_data_len;
    }
    pdu_len += encode_closing_tag(&pdu[pdu_len], 5);

    return pdu_len;
}

static int ao_read_priority_array(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_ao_t *ao;
    uint8_t *pdu;
    int pdu_len;
    int index;

    if (range != NULL) {
        return ao_read_priority_array_RR(object, rp_data, range);
    }
    
    ao = container_of(object, object_ao_t, base.base.base);
    pdu = rp_data->application_data;

    if (rp_data->array_index == 0) {
        return encode_application_unsigned(pdu,
                sizeof(ao->priority_array) / sizeof(ao->priority_array[0]));
    }
    
    if (rp_data->array_index == BACNET_ARRAY_ALL) {
        pdu_len = 0;
        for (index = 0; index < sizeof(ao->priority_array) / sizeof(ao->priority_array[0]); ++index) {
            if (ao->priority_bits & (1 << index)) {
                pdu_len += encode_application_real(&pdu[pdu_len], ao->priority_array[index]);
            } else {
                pdu_len += encode_application_null(&pdu[pdu_len]);
            }

            if (pdu_len >= rp_data->application_data_len) {
                rp_data->abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
                return BACNET_STATUS_ABORT;
            }
        }
        
        return pdu_len;
    }

    if (rp_data->array_index
            > sizeof(ao->priority_array) / sizeof(ao->priority_array[0])) {
        rp_data->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
        return BACNET_STATUS_ERROR;
    }

    index = rp_data->array_index - 1;
    if (ao->priority_bits & (1 << index)) {
        return encode_application_real(pdu, ao->priority_array[index]);
    } else {
        return encode_application_null(pdu);
    }
}

object_impl_t *object_create_impl_ao(void)
{
    object_impl_t *ao;
    property_impl_t *p_impl;
    
    ao = object_create_impl_ai();
    if (!ao) {
        APP_ERROR("%s: create ai impl failed\r\n", __func__);
        return NULL;
    }
    ao->type = OBJECT_ANALOG_OUTPUT;

    p_impl = object_impl_extend(ao, PROP_PRESENT_VALUE, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_PRESENT_VALUE failed\r\n", __func__);
        goto out;
    }
    p_impl->write_property = ao_write_present_value;

    p_impl = object_impl_extend(ao, PROP_RELINQUISH_DEFAULT, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_RELINQUISH_DEFAULT failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = ao_read_relinquish_default;

    p_impl = object_impl_extend(ao, PROP_PRIORITY_ARRAY, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_PRIORITY_ARRAY failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = ao_read_priority_array;
    
    return ao;

out:
    object_impl_destroy(ao);
    
    return NULL;
}

int __attribute__((weak)) analog_output_init(cJSON *object)
{
    object_impl_t *ao_type = NULL;
    object_ao_t *ao;
    object_instance_t *ao_instance;
    cJSON *array, *instance, *tmp;
    char *name;
    bool out_of_service;
    float relinquish_default;
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

        tmp = cJSON_GetObjectItem(instance, "Relinquish_Default");
        if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
            APP_ERROR("%s: get Instance_List[%d] Relinquish_Default item failed\r\n", __func__, i);
            goto reclaim;
        }
        relinquish_default = tmp->valuedouble;

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

        if (!ao_type) {
            ao_type = (object_impl_t *)object_create_impl_ao();
            if (!ao_type) {
                APP_ERROR("%s: create ao type impl failed\r\n", __func__);
                goto reclaim;
            }
        }

        ao = malloc(sizeof(object_ao_t));
        if (!ao) {
            APP_ERROR("%s: not enough memory\r\n", __func__);
            goto reclaim;
        }
        memset(ao, 0, sizeof(*ao));
        ao->base.base.base.instance = i;
        ao->base.base.base.type = ao_type;
        ao->base.base.Out_Of_Service = out_of_service;
        ao->base.present = relinquish_default;
        ao->base.units = units;
        ao->active_bit = BACNET_MAX_PRIORITY;
        ao->relinquish_default = relinquish_default;

        if (!vbuf_fr_str(&ao->base.base.base.object_name.vbuf, name, OBJECT_NAME_MAX_LEN)) {
            APP_ERROR("%s: set object name overflow\r\n", __func__);
            free(ao);
            goto reclaim;
        }

        if (!object_add(&ao->base.base.base)) {
            APP_ERROR("%s: object add failed\r\n", __func__);
            free(ao);
            goto reclaim;
        }
        i++;
    }

end:
    return OK;

reclaim:
    for (i = i - 1; i >= 0; i--) {
        ao_instance = object_find(OBJECT_ANALOG_OUTPUT, i);
        if (!ao_instance) {
            APP_ERROR("%s: reclaim failed\r\n", __func__);
        } else {
            object_detach(ao_instance);
            free(ao_instance);
        }
    }

    if (ao_type) {
        object_impl_destroy(ao_type);
    }

out:
    return -EPERM;
}

