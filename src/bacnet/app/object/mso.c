/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * mso.c
 * Original Author:  linzhixian, 2015-1-12
 *
 * Multi-state Output Object
 *
 * History
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "bacnet/object/mso.h"
#include "bacnet/bacapp.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacdef.h"
#include "bacnet/app.h"

static int mso_write_present_value(object_instance_t *object, BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    object_mso_t *mso;
    uint32_t value;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    if (wp_data->priority == 6) {
        wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
        return BACNET_STATUS_ERROR;
    }

    mso = container_of(object, object_mso_t, base.base.base);

    if (decode_application_unsigned(wp_data->application_data, &value)
            != wp_data->application_data_len) {
        if (decode_application_null(wp_data->application_data)
                != wp_data->application_data_len) {
            return BACNET_STATUS_ERROR;
        } else {
            (void)multistate_output_present_value_relinquish(mso, wp_data->priority);
        }
    } else {
        if ((value == 0) || (value > mso->base.number_of_states)) {
            wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
            return BACNET_STATUS_ERROR;
        }
        
        (void)multistate_output_present_value_set(mso, value, wp_data->priority);
    }

    return 0;
}

static int mso_read_relinquish_default(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_mso_t *mso;
    
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    mso = container_of(object, object_mso_t, base.base.base);

    return encode_application_unsigned(rp_data->application_data, mso->relinquish_default);
}

static int mso_read_priority_array_RR(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_mso_t *mso;
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

    mso = container_of(object, object_mso_t, base.base.base);
    item_count = sizeof(mso->priority_array) / sizeof(mso->priority_array[0]);
    
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

        if (mso->priority_bits & (1 << index)) {
            item_data_len += encode_application_unsigned(&item_data[item_data_len],
                mso->priority_array[index]);
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

static int mso_read_priority_array(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_mso_t *mso;
    uint8_t *pdu;
    int pdu_len;
    int index;

    if (range != NULL) {
        return mso_read_priority_array_RR(object, rp_data, range);
    }

    mso = container_of(object, object_mso_t, base.base.base);
    pdu = rp_data->application_data;

    if (rp_data->array_index == 0) {
        return encode_application_unsigned(pdu,
                sizeof(mso->priority_array) / sizeof(mso->priority_array[0]));
    }
    
    if (rp_data->array_index == BACNET_ARRAY_ALL) {
        pdu_len = 0;
        for (index = 0; index < sizeof(mso->priority_array) / sizeof(mso->priority_array[0]); ++index) {
            if (mso->priority_bits & (1 << index)) {
                pdu_len += encode_application_unsigned(&pdu[pdu_len], mso->priority_array[index]);
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
            > sizeof(mso->priority_array) / sizeof(mso->priority_array[0])) {
        rp_data->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
        return BACNET_STATUS_ERROR;
    }

    index = rp_data->array_index - 1;
    if (mso->priority_bits & (1 << index)) {
        return encode_application_unsigned(pdu, mso->priority_array[index]);
    } else {
        return encode_application_null(pdu);
    }
}

object_impl_t *object_create_impl_mso(void)
{
    object_impl_t *mso;
    property_impl_t *p_impl;
    
    mso = object_create_impl_msi();
    if (!mso) {
        APP_ERROR("%s: create msi impl failed\r\n", __func__);
        return NULL;
    }
    mso->type = OBJECT_MULTI_STATE_OUTPUT;

    p_impl = object_impl_extend(mso, PROP_PRESENT_VALUE, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_PRESENT_VALUE failed\r\n", __func__);
        goto out;
    }
    p_impl->write_property = mso_write_present_value;

    p_impl = object_impl_extend(mso, PROP_RELINQUISH_DEFAULT, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_RELINQUISH_DEFAULT failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = mso_read_relinquish_default;

    p_impl = object_impl_extend(mso, PROP_PRIORITY_ARRAY, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_PRIORITY_ARRAY failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = mso_read_priority_array;

    return mso;

out:
    object_impl_destroy(mso);
    
    return NULL;
}

bool multistate_output_present_value_set(object_mso_t *mso, uint32_t value, uint8_t priority)
{
    if (!mso) {
        APP_ERROR("%s: null argument\r\n", __func__);
        return false;
    }

    if (priority < BACNET_MIN_PRIORITY || priority > BACNET_MAX_PRIORITY) {
        APP_ERROR("%s: invalid priority(%d)\r\n\r\n", __func__, priority);
        return false;
    }

    int idx = priority - 1;
    mso->priority_array[idx] = value;
    mso->priority_bits |= 1 << idx;

    if (idx <= mso->active_bit) {
        mso->active_bit = idx;
        uint32_t prevalue = mso->base.present;
        mso->base.present = value;
        if (prevalue != value && !mso->base.base.Out_Of_Service) {
            if (mso->base.base.notify) {
                mso->base.base.notify(&mso->base.base);
            }
        }
    }

    return true;
}

bool multistate_output_present_value_relinquish(object_mso_t *mso, uint8_t priority)
{
    if (!mso) {
        APP_ERROR("%s: null argument\r\n", __func__);
        return false;
    }

    if (priority < BACNET_MIN_PRIORITY || priority > BACNET_MAX_PRIORITY) {
        APP_ERROR("%s: invalid priority(%d)\r\n\r\n", __func__, priority);
        return false;
    }

    int idx = priority - 1;
    mso->priority_bits &= ~(1 << idx);

    if (idx != mso->active_bit) {
        return true;
    }
    
    uint32_t prevalue = mso->base.present;
    for (int i = idx + 1; i < BACNET_MAX_PRIORITY; ++i) {
        if (mso->priority_bits & (1 << i)) {
            mso->active_bit = i;
            mso->base.present = mso->priority_array[i];
            goto end;
        }
    }
    mso->active_bit = BACNET_MAX_PRIORITY;
    mso->base.present = mso->relinquish_default;

end:
    if (prevalue != mso->base.present && !mso->base.base.Out_Of_Service) {
        if (mso->base.base.notify) {
            mso->base.base.notify(&mso->base.base);
        }
    }

    return true;
}

int __attribute__((weak)) multistate_output_init(cJSON *object)
{
    cJSON *array, *instance, *tmp;
    object_impl_t *mso_type = NULL;
    object_instance_t *mso_instance;
    object_mso_t *mso;
    char *name;
    bool out_of_service;
    uint32_t relinquish_default;
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

        if (!mso_type) {
            mso_type = (object_impl_t *)object_create_impl_mso();
            if (!mso_type) {
                APP_ERROR("%s: create mso type impl failed\r\n", __func__);
                goto reclaim;
            }
        }

        mso = (object_mso_t *)malloc(sizeof(object_mso_t));
        if (!mso) {
            APP_ERROR("%s: not enough memory\r\n", __func__);
            goto reclaim;
        }
        memset(mso, 0, sizeof(*mso));
        mso->base.base.base.instance = i;
        mso->base.base.base.type = mso_type;
        mso->base.base.Out_Of_Service = out_of_service;
        mso->base.present = relinquish_default;
        mso->base.number_of_states = number_of_states;
        mso->active_bit = BACNET_MAX_PRIORITY;
        mso->relinquish_default = relinquish_default;

        if (!vbuf_fr_str(&mso->base.base.base.object_name.vbuf, name, OBJECT_NAME_MAX_LEN)) {
            APP_ERROR("%s: set object name overflow\r\n", __func__);
            free(mso);
            goto reclaim;
        }

        if (!object_add(&mso->base.base.base)) {
            APP_ERROR("%s: object add failed\r\n", __func__);
            free(mso);
            goto reclaim;
        }
        i++;
    }

end:
    return OK;

reclaim:
    for (i = i - 1; i >= 0; i--) {
        mso_instance = object_find(OBJECT_MULTI_STATE_OUTPUT, i);
        if (!mso_instance) {
            APP_ERROR("%s: reclaim failed\r\n", __func__);
        } else {
            object_detach(mso_instance);
            free(mso_instance);
        }
    }

    if (mso_type) {
        object_impl_destroy(mso_type);
    }
    
out:
    return -EPERM;
}

