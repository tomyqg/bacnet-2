/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bo.c
 * Original Author:  linzhixian, 2015-1-12
 *
 * Binary Output Object
 *
 * History
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "bacnet/object/bo.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacapp.h"
#include "bacnet/bacdef.h"
#include "bacnet/app.h"

static int bo_write_present_value(object_instance_t *object, BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    object_bo_t *bo;
    uint32_t value;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    if (wp_data->priority == 6) {
        wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
        return BACNET_STATUS_ERROR;
    }

    bo = container_of(object, object_bo_t, base.base.base);

    if (decode_application_enumerated(wp_data->application_data, &value)
            != wp_data->application_data_len) {
        if (decode_application_null(wp_data->application_data) != wp_data->application_data_len) {
            return BACNET_STATUS_ERROR;
        } else {
            binary_output_present_value_relinquish(bo, wp_data->priority);
        }
    } else {
        if (value >= MAX_BINARY_PV) {
            wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
            return BACNET_STATUS_ERROR;
        }
        binary_output_present_value_set(bo, value, wp_data->priority);
    }

    return 0;
}

static int bo_read_relinquish_default(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_bo_t *bo;
    
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    bo = container_of(object, object_bo_t, base.base.base);

    return encode_application_enumerated(rp_data->application_data, bo->relinquish_default);
}

static int bo_read_priority_array_RR(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_bo_t *bo;
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

    bo = container_of(object, object_bo_t, base.base.base);
    item_count = sizeof(bo->priority_array) / sizeof(bo->priority_array[0]);
    
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

        if (bo->priority_bits & (1 << index)) {
            item_data_len += encode_application_enumerated(&item_data[item_data_len],
                bo->priority_array[index]);
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

static int bo_read_priority_array(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_bo_t *bo;
    uint8_t *pdu;
    int pdu_len;
    int index;

    if (range != NULL) {
        return bo_read_priority_array_RR(object, rp_data, range);
    }
    
    bo = container_of(object, object_bo_t, base.base.base);
    pdu = rp_data->application_data;

    if (rp_data->array_index == 0) {
        return encode_application_unsigned(pdu,
                sizeof(bo->priority_array) / sizeof(bo->priority_array[0]));
    }
    
    if (rp_data->array_index == BACNET_ARRAY_ALL) {
        pdu_len = 0;
        for (index = 0; index < sizeof(bo->priority_array) / sizeof(bo->priority_array[0]); ++index) {
            if (bo->priority_bits & (1 << index)) {
                pdu_len += encode_application_enumerated(&pdu[pdu_len], bo->priority_array[index]);
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
            > sizeof(bo->priority_array) / sizeof(bo->priority_array[0])) {
        rp_data->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
        return BACNET_STATUS_ERROR;
    }

    index = rp_data->array_index - 1;
    if (bo->priority_bits & (1 << index)) {
        return encode_application_enumerated(pdu, bo->priority_array[index]);
    } else {
        return encode_application_null(pdu);
    }
}

object_impl_t *object_create_impl_bo(void)
{
    object_impl_t *bo;
    property_impl_t *p_impl;
    
    bo = object_create_impl_bi();
    if (!bo) {
        APP_ERROR("%s: create bi impl failed\r\n", __func__);
        return NULL;
    }
    bo->type = OBJECT_BINARY_OUTPUT;

    p_impl = object_impl_extend(bo, PROP_PRESENT_VALUE, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_PRESENT_VALUE failed\r\n", __func__);
        goto out;
    }
    p_impl->write_property = bo_write_present_value;

    p_impl = object_impl_extend(bo, PROP_RELINQUISH_DEFAULT, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_RELINQUISH_DEFAULT failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = bo_read_relinquish_default;

    p_impl = object_impl_extend(bo, PROP_PRIORITY_ARRAY, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_PRIORITY_ARRAY failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = bo_read_priority_array;
    
    return bo;

out:
    object_impl_destroy(bo);
    
    return NULL;
}

bool binary_output_present_value_set(object_bo_t *bo, BACNET_BINARY_PV value, uint8_t priority)
{
    int idx;
    
    if (!bo) {
        APP_ERROR("%s: null argument\r\n", __func__);
        return false;
    }

    if (priority < BACNET_MIN_PRIORITY || priority > BACNET_MAX_PRIORITY) {
        APP_ERROR("%s: invalid priority(%d)\r\n\r\n", __func__, priority);
        return false;
    }

    idx = priority - 1;
    bo->priority_array[idx] = value;
    bo->priority_bits |= 1 << idx;

    if (idx <= bo->active_bit) {
        bo->active_bit = idx;
        BACNET_BINARY_PV prevalue = bo->base.present;
        bo->base.present = value;
        if (prevalue != value && !bo->base.base.Out_Of_Service) {
            if (bo->base.base.notify)
                bo->base.base.notify(&bo->base.base);
        }
    }

    return true;
}

bool binary_output_present_value_relinquish(object_bo_t *bo, uint8_t priority)
{
    int idx;
    int i;
    
    if (!bo) {
        APP_ERROR("%s: null argument\r\n", __func__);
        return false;
    }

    if (priority < BACNET_MIN_PRIORITY || priority > BACNET_MAX_PRIORITY) {
        APP_ERROR("%s: invalid priority(%d)\r\n\r\n", __func__, priority);
        return false;
    }

    idx = priority - 1;
    bo->priority_bits &= ~(1 << idx);

    if (idx != bo->active_bit)
        return true;

    BACNET_BINARY_PV prevalue = bo->base.present;
    for (i = idx + 1; i < BACNET_MAX_PRIORITY; ++i) {
        if (bo->priority_bits & (1 << i)) {
            bo->active_bit = i;
            bo->base.present = bo->priority_array[i];
            goto end;
        }
    }
    bo->active_bit = BACNET_MAX_PRIORITY;
    bo->base.present = bo->relinquish_default;

end:
    if (prevalue != bo->base.present && !bo->base.base.Out_Of_Service) {
        if (bo->base.base.notify) {
            bo->base.base.notify(&bo->base.base);
        }
    }

    return true;
}

int __attribute__((weak)) binary_output_init(cJSON *object)
{
    cJSON *array, *instance, *tmp;
    object_impl_t *bo_type = NULL;
    object_bo_t *bo;
    object_instance_t *bo_instance;
    char *name;
    bool out_of_service;
    BACNET_BINARY_PV relinquish_default;
    BACNET_POLARITY polarity;
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
        if ((tmp->valueint < MIN_BINARY_PV) || (tmp->valueint >= MAX_BINARY_PV)) {
            APP_ERROR("%s: invalid Instance_List[%d] Relinquish_Default item value(%d)\r\n", __func__,
                i, tmp->valueint);
            goto reclaim;
        }
        relinquish_default = (BACNET_BINARY_PV)tmp->valueint;

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

        if (!bo_type) {
            bo_type = (object_impl_t *)object_create_impl_bo();
            if (!bo_type) {
                APP_ERROR("%s: create bo type impl failed\r\n", __func__);
                goto reclaim;
            }
        }

        bo = (object_bo_t *)malloc(sizeof(object_bo_t));
        if (!bo) {
            APP_ERROR("%s: not enough memory\r\n", __func__);
            goto reclaim;
        }
        memset(bo, 0, sizeof(*bo));
        bo->base.base.base.instance = i;
        bo->base.base.base.type = bo_type;
        bo->base.base.Out_Of_Service = out_of_service;
        bo->base.present = relinquish_default;
        bo->base.polarity = polarity;
        bo->active_bit = BACNET_MAX_PRIORITY;
        bo->relinquish_default = relinquish_default;

        if (!vbuf_fr_str(&bo->base.base.base.object_name.vbuf, name, OBJECT_NAME_MAX_LEN)) {
            APP_ERROR("%s: set object name overflow\r\n", __func__);
            free(bo);
            goto reclaim;
        }

        if (!object_add(&bo->base.base.base)) {
            APP_ERROR("%s: object add failed\r\n", __func__);
            free(bo);
            goto reclaim;
        }
        i++;
    }

end:
    return OK;

reclaim:
    for (i = i - 1; i >= 0; i--) {
        bo_instance = object_find(OBJECT_BINARY_OUTPUT, i);
        if (!bo_instance) {
            APP_ERROR("%s: reclaim failed\r\n", __func__);
        } else {
            object_detach(bo_instance);
            free(bo_instance);
        }
    }

    if (bo_type) {
        object_impl_destroy(bo_type);
    }
    
out:
    return -EPERM;
}

