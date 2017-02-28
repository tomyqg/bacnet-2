/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * device.c
 * Original Author:  linzhixian, 2015-1-15
 *
 * BACnet Device Object
 *
 * History
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "bacnet/object/device.h"
#include "bacnet/service/dcc.h"
#include "bacnet/bacapp.h"
#include "bacnet/bacdcode.h"
#include "bacnet/addressbind.h"
#include "bacnet/datetime.h"
#include "bacnet/slaveproxy.h"
#include "bacnet/app.h"

static object_instance_t* device_instance = NULL;

static object_impl_t* device_impl = NULL;

static int32_t UTC_Offset = 8 * 60;

static VBUF_INIT(Location, MAX_DEV_LOC_LEN, "USA");

static uint32_t Database_Revision = 0;

static BACNET_DEVICE_STATUS System_Status = STATUS_OPERATIONAL;

uint32_t device_object_instance_number(void)
{
    if (device_instance) {
        return device_instance->instance;
    }

    return BACNET_MAX_INSTANCE;
}

object_instance_t *device_get_instance(void)
{
    return device_instance;
}

void device_database_revision_increasee(void)
{
    Database_Revision++;
}

/* return value - 0 = ok, -1 = bad value, -2 = not allowed */
static int Device_Set_System_Status(BACNET_DEVICE_STATUS status, bool local)
{
    int result;

    result = 0;

    /* We limit the options available depending on whether the source is internal or external. */
    if (local) {
        switch (status) {
        case STATUS_OPERATIONAL:
        case STATUS_OPERATIONAL_READ_ONLY:
        case STATUS_DOWNLOAD_REQUIRED:
        case STATUS_DOWNLOAD_IN_PROGRESS:
        case STATUS_NON_OPERATIONAL:
            System_Status = status;
            break;

        /* Don't support backup at present so don't allow setting */
        case STATUS_BACKUP_IN_PROGRESS:
            result = -2;
            break;

        default:
            result = -1;
            break;
        }
    } else {
        switch (status) {
        /* Allow these for the moment as a way to easily alter overall device operation.
           The lack of password protection or other authentication makes allowing writes
           to this property a risky facility to provide. */
        case STATUS_OPERATIONAL:
        case STATUS_OPERATIONAL_READ_ONLY:
        case STATUS_NON_OPERATIONAL:
            System_Status = status;
            break;

        /* Don't allow outsider set this - it should probably be set if the device config
           is incomplete or corrupted or perhaps after some sort of operator wipe operation. */
        case STATUS_DOWNLOAD_REQUIRED:
        /* Don't allow outsider set this - it should be set internally at the start of a multi
           packet download perhaps indirectly via PT or WF to a config file. */
        case STATUS_DOWNLOAD_IN_PROGRESS:
        /* Don't support backup at present so don't allow setting */
        case STATUS_BACKUP_IN_PROGRESS:
            result = -2;
            break;

        default:
            result = -1;
            break;
        }
    }

    return result;
}

static int device_write_object_name(object_instance_t *object, BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    BACNET_CHARACTER_STRING str;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    if (decode_application_character_string(wp_data->application_data, &str)
            != wp_data->application_data_len) {
        return BACNET_STATUS_ERROR;
    }

    wp_data->error_code = object_rename(device_instance, &str);
    
    return wp_data->error_code == MAX_BACNET_ERROR_CODE ? 0 : BACNET_STATUS_ERROR;
}

static int device_read_system_status(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    return encode_application_enumerated(rp_data->application_data, System_Status);
}

static int device_write_system_status(object_instance_t *object, BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    uint32_t value;
    int rv;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    if (decode_application_enumerated(wp_data->application_data, &value)
            != wp_data->application_data_len)
        return BACNET_STATUS_ERROR;

    rv = Device_Set_System_Status((BACNET_DEVICE_STATUS)value, false);
    if (rv == 0) {
        return 0;
    }
    
    if (rv == -1) {
        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
    } else {
        wp_data->error_code = ERROR_CODE_OPTIONAL_FUNCTIONALITY_NOT_SUPPORTED;
    }
    
    return BACNET_STATUS_ERROR;
}

static int device_read_vendor_name(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    if (sizeof(BACNET_VENDOR_NAME) >= rp_data->application_data_len) {
        rp_data->abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
        return BACNET_STATUS_ABORT;
    }

    return encode_application_ansi_character_string(rp_data->application_data, BACNET_VENDOR_NAME,
            sizeof(BACNET_VENDOR_NAME));
}

static int device_read_vendor_id(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    return encode_application_unsigned(rp_data->application_data, BACNET_VENDOR_ID);
}

static int device_read_model_name(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    if (sizeof(DEVICE_MODEL_NAME) >= rp_data->application_data_len) {
        rp_data->error_code = ERROR_CODE_ABORT_SEGMENTATION_NOT_SUPPORTED;
        return BACNET_STATUS_ABORT;
    }

    return encode_application_ansi_character_string(rp_data->application_data, DEVICE_MODEL_NAME,
            sizeof(DEVICE_MODEL_NAME) - 1);
}

static int device_read_firmware_revision(object_instance_t *object,
            BACNET_READ_PROPERTY_DATA *rp_data, RR_RANGE *range)
{
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    if (sizeof(DEVICE_FIRMWARE_REVISION) >= rp_data->application_data_len) {
        rp_data->abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
        return BACNET_STATUS_ABORT;
    }

    return encode_application_ansi_character_string(rp_data->application_data,
            DEVICE_FIRMWARE_REVISION, sizeof(DEVICE_FIRMWARE_REVISION) - 1);
}

static int device_read_application_software_version(object_instance_t *object,
        BACNET_READ_PROPERTY_DATA *rp_data, RR_RANGE *range)
{
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    if (sizeof(DEVICE_APPLICATION_SOFTWARE_VERSION)
            >= rp_data->application_data_len) {
        rp_data->abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
        return BACNET_STATUS_ABORT;
    }

    return encode_application_ansi_character_string(rp_data->application_data,
            DEVICE_APPLICATION_SOFTWARE_VERSION, sizeof(DEVICE_APPLICATION_SOFTWARE_VERSION) - 1);
}

static int device_read_protocol_version(object_instance_t *object,
            BACNET_READ_PROPERTY_DATA *rp_data, RR_RANGE *range)
{
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    return encode_application_unsigned(rp_data->application_data, BACNET_PROTOCOL_VERSION);
}

static int device_read_protocol_revision(object_instance_t *object,
            BACNET_READ_PROPERTY_DATA *rp_data, RR_RANGE *range)
{
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    return encode_application_unsigned(rp_data->application_data, BACNET_PROTOCOL_REVISION);
}

static int device_read_protocol_services_supported(object_instance_t *object,
            BACNET_READ_PROPERTY_DATA *rp_data, RR_RANGE *range)
{
    BACNET_BIT_STRING services;
    
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    apdu_get_service_supported(&services);

    return encode_application_bitstring(rp_data->application_data, &services);
}

static int device_read_protocol_object_types_supported(object_instance_t *object,
            BACNET_READ_PROPERTY_DATA *rp_data, RR_RANGE *range)
{
    BACNET_BIT_STRING types;
    
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }
    
    if (!object_get_types_supported(&types)) {
        APP_ERROR("%s: get object types supported failed\r\n",  __func__);
        rp_data->error_code = ERROR_CODE_VALUE_NOT_INITIALIZED;
        return BACNET_STATUS_ERROR;
    }
    
    return encode_application_bitstring(rp_data->application_data, &types);
}

static int device_read_object_list_RR(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    BACNET_BIT_STRING ResultFlags;
    object_store_t *store;
    object_instance_t *obj;
    uint8_t *pdu;
    bool found;
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

    item_count = object_list_count();
    
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
    
    item_data_len = 0;
    item_count = 0;
    value = 0;
    (void)bitstring_init(&ResultFlags, &value, 3);

    found = object_find_index(range->Range.RefIndex - 1, &store, &obj);
    while (found) {
        if ((item_data_len + 5) > sizeof(item_data)) {
            bitstring_set_bit(&ResultFlags, RESULT_FLAG_MORE_ITEMS, true);
            break;
        }

        item_data_len += encode_application_object_id(&item_data[item_data_len], store->object_type,
            obj->instance);
        item_count++;
        found = object_find_next(&store, &obj);
        if ((item_count >= range->Count) || (!found)) {
            bitstring_set_bit(&ResultFlags, RESULT_FLAG_LAST_ITEM, true);
            break;
        }
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

static int device_read_object_list(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_store_t *store;
    object_instance_t *obj;
    bool found;
    uint8_t *pdu;

    if (range != NULL) {
        return device_read_object_list_RR(object, rp_data, range);
    }

    pdu = rp_data->application_data;
    if (rp_data->array_index == 0) {
        return encode_application_unsigned(pdu, object_list_count());
    }

    if (rp_data->array_index == BACNET_ARRAY_ALL) {
        int pdu_len, len;

        pdu_len = 0;
        found = object_find_index(0, &store, &obj);
        while (found) {
            len = encode_application_object_id(&pdu[pdu_len], store->object_type, obj->instance);
            pdu_len += len;
            if (pdu_len >= rp_data->application_data_len) {
                rp_data->abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
                return BACNET_STATUS_ABORT;
            }
            
            found = object_find_next(&store, &obj);
        }
        
        return pdu_len;
    }

    found = object_find_index(rp_data->array_index - 1, &store, &obj);
    if (!found) {
        rp_data->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
        return BACNET_STATUS_ERROR;
    }
    
    return encode_application_object_id(pdu, store->object_type, obj->instance);
}

static int device_read_max_apdu_length_accepted(object_instance_t *object,
            BACNET_READ_PROPERTY_DATA *rp_data, RR_RANGE *range)
{
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    return encode_application_unsigned(rp_data->application_data, MAX_APDU);
}

static int device_read_segmentation_support(object_instance_t *object,
            BACNET_READ_PROPERTY_DATA *rp_data, RR_RANGE *range)
{
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    return encode_application_enumerated(rp_data->application_data, SEGMENTATION_NONE);
}

static int device_read_apdu_timeout_and_retry(object_instance_t *object,
            BACNET_READ_PROPERTY_DATA *rp_data, RR_RANGE *range)
{
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    return encode_application_unsigned(rp_data->application_data, 0);
}

static int device_read_device_address_binding(object_instance_t *object,
            BACNET_READ_PROPERTY_DATA *rp_data, RR_RANGE *range)
{
    if (rp_data->array_index != BACNET_ARRAY_ALL) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    return read_address_binding(rp_data, range);
}

static int device_read_database_revision(object_instance_t *object,
            BACNET_READ_PROPERTY_DATA *rp_data, RR_RANGE *range)
{
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    return encode_application_unsigned(rp_data->application_data, Database_Revision);
}

static int device_read_location(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    if (Location.vbuf.length >= rp_data->application_data_len) {
        rp_data->abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
        return BACNET_STATUS_ABORT;
    }

    return encode_application_ansi_character_string(rp_data->application_data,
            (char*)Location.vbuf.value, Location.vbuf.length);
}

static int device_write_location(object_instance_t *object, BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    BACNET_CHARACTER_STRING str;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    if (decode_application_character_string(wp_data->application_data, &str)
            != wp_data->application_data_len) {
        return BACNET_STATUS_ERROR;
    } else if (str.length > MAX_DEV_LOC_LEN) {
        wp_data->error_code = ERROR_CODE_NO_SPACE_TO_WRITE_PROPERTY;
        return BACNET_STATUS_ERROR;
    } else if (!characterstring_printable(&str)) {
        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
        return BACNET_STATUS_ERROR;
    } else {
        (void)vbuf_fr_buf(&Location.vbuf, (uint8_t*)str.value, str.length);
        return 0;
    }
}

static int device_read_utc_offset(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    return encode_application_signed(rp_data->application_data, UTC_Offset);
}

static int device_write_utc_offset(object_instance_t *object, BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    int32_t value;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    if (decode_application_signed(wp_data->application_data, &value)
            != wp_data->application_data_len) {
        return BACNET_STATUS_ERROR;
    } else if (value > 780 || value < -780) {
        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
        return BACNET_STATUS_ERROR;
    } else {
        UTC_Offset = value;
        return 0;
    }
}

static void device_impl_init(void)
{
    object_impl_t *device;
    property_impl_t *p_impl;
    
    if (device_impl) {
        return;
    }
    
    device = object_impl_clone(object_create_impl_base());
    if (!device) {
        APP_ERROR("%s: clone base impl failed\r\n", __func__);
        return;
    }
    device->type = OBJECT_DEVICE;

    p_impl = object_impl_extend(device, PROP_OBJECT_NAME, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_OBJECT_NAME failed\r\n", __func__);
        goto out;
    }
    p_impl->write_property = device_write_object_name;

    p_impl = object_impl_extend(device, PROP_SYSTEM_STATUS, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_SYSTEM_STATUS failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = device_read_system_status;
    p_impl->write_property = device_write_system_status;

    p_impl = object_impl_extend(device, PROP_VENDOR_NAME, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_VENDOR_NAME failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = device_read_vendor_name;

    p_impl = object_impl_extend(device, PROP_VENDOR_IDENTIFIER, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_VENDOR_IDENTIFIER failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = device_read_vendor_id;

    p_impl = object_impl_extend(device, PROP_MODEL_NAME, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_MODEL_NAME failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = device_read_model_name;

    p_impl = object_impl_extend(device, PROP_FIRMWARE_REVISION, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_FIRMWARE_REVISION failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = device_read_firmware_revision;

    p_impl = object_impl_extend(device, PROP_APPLICATION_SOFTWARE_VERSION, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_APPLICATION_SOFTWARE_VERSION failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = device_read_application_software_version;

    p_impl = object_impl_extend(device, PROP_PROTOCOL_VERSION, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_PROTOCOL_VERSION failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = device_read_protocol_version;

    p_impl = object_impl_extend(device, PROP_PROTOCOL_REVISION, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_PROTOCOL_REVISION failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = device_read_protocol_revision;

    p_impl = object_impl_extend(device, PROP_PROTOCOL_SERVICES_SUPPORTED, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_PROTOCOL_SERVICES_SUPPORTED failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = device_read_protocol_services_supported;

    p_impl = object_impl_extend(device, PROP_PROTOCOL_OBJECT_TYPES_SUPPORTED, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_PROTOCOL_OBJECT_TYPES_SUPPORTED failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = device_read_protocol_object_types_supported;

    p_impl = object_impl_extend(device, PROP_OBJECT_LIST, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_OBJECT_LIST failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = device_read_object_list;

    p_impl = object_impl_extend(device, PROP_MAX_APDU_LENGTH_ACCEPTED, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_MAX_APDU_LENGTH_ACCEPTED failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = device_read_max_apdu_length_accepted;

    p_impl = object_impl_extend(device, PROP_SEGMENTATION_SUPPORTED, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_SEGMENTATION_SUPPORTED failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = device_read_segmentation_support;

    p_impl = object_impl_extend(device, PROP_APDU_TIMEOUT, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_APDU_TIMEOUT failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = device_read_apdu_timeout_and_retry;

    p_impl = object_impl_extend(device, PROP_NUMBER_OF_APDU_RETRIES, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_NUMBER_OF_APDU_RETRIES failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = device_read_apdu_timeout_and_retry;

    p_impl = object_impl_extend(device, PROP_DEVICE_ADDRESS_BINDING, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_DEVICE_ADDRESS_BINDING failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = device_read_device_address_binding;

    p_impl = object_impl_extend(device, PROP_DATABASE_REVISION, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_DATABASE_REVISION failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = device_read_database_revision;

    p_impl = object_impl_extend(device, PROP_LOCATION, PROPERTY_TYPE_OPTIONAL);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_LOCATION failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = device_read_location;
    p_impl->write_property = device_write_location;

    p_impl = object_impl_extend(device, PROP_UTC_OFFSET, PROPERTY_TYPE_OPTIONAL);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_UTC_OFFSET failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = device_read_utc_offset;
    p_impl->write_property = device_write_utc_offset;

    device_impl = device;
    
    return;

out:
    object_impl_destroy(device);
    
    return;
}

static int device_read_auto_slave_discovery_RR(object_instance_t *object,
            BACNET_READ_PROPERTY_DATA *rp_data, RR_RANGE *range)
{
    bool auto_discovery;
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

    item_count = slave_proxy_port_number();
    
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
        if ((item_data_len + 1) > sizeof(item_data)) {
            bitstring_set_bit(&ResultFlags, RESULT_FLAG_MORE_ITEMS, true);
            break;
        }
        
        if (slave_proxy_get(index, NULL, &auto_discovery) < 0) {
            break;
        }
        
        item_data_len += encode_application_boolean(&item_data[item_data_len], auto_discovery);
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

static int device_read_auto_slave_discovery(object_instance_t *object,
            BACNET_READ_PROPERTY_DATA *rp_data, RR_RANGE *range)
{
    int port_number;
    bool auto_discovery;
    
    if (range != NULL) {
        return device_read_auto_slave_discovery_RR(object, rp_data, range);
    }
    
    if (rp_data->array_index == 0){
        return encode_application_unsigned(rp_data->application_data, slave_proxy_port_number());
    }
    
    if (rp_data->array_index == BACNET_ARRAY_ALL) {
        uint8_t *pdu;
        int pdu_len, i;

        pdu_len = 0;
        pdu = rp_data->application_data;
        port_number = slave_proxy_port_number();
        for (i = 0; i < port_number; ++i) {
            (void)slave_proxy_get(i, NULL, &auto_discovery);
            pdu_len += encode_application_boolean(&pdu[pdu_len], auto_discovery);
            if (pdu_len >= rp_data->application_data_len) {
                rp_data->abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
                return BACNET_STATUS_ABORT;
            }
        }
        
        return pdu_len;
    }

    if (slave_proxy_get(rp_data->array_index - 1, NULL, &auto_discovery) < 0) {
        rp_data->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
        return BACNET_STATUS_ERROR;
    }
    
    return encode_application_boolean(rp_data->application_data, auto_discovery);
}

static int device_write_auto_slave_discovery(object_instance_t *object,
            BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    bool auto_disc;
    bool enable;
    
    if (wp_data->array_index == 0) {
        wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
        return BACNET_STATUS_ERROR;
    }
    
    if (wp_data->array_index == BACNET_ARRAY_ALL) {
        int len, idx, dec_len;
        for (len = 0, idx = 0; len < wp_data->application_data_len; ++idx) {
            dec_len = decode_application_boolean(&wp_data->application_data[len], &auto_disc);
            if ((dec_len < 0) || (len + dec_len > wp_data->application_data_len)) {
                return BACNET_STATUS_ERROR;
            }
            len += dec_len;

            if ((slave_proxy_get(idx, &enable, NULL) < 0)
                    || (slave_proxy_enable(idx, enable, auto_disc) < 0)) {
                break;
            }
        }
        
        return 0;
    }

    if (decode_application_boolean(wp_data->application_data, &auto_disc)
            != wp_data->application_data_len) {
        return BACNET_STATUS_ERROR;
    }

    if ((slave_proxy_get(wp_data->array_index - 1, &enable, NULL) < 0)
            || (slave_proxy_enable(wp_data->array_index - 1, enable, auto_disc) < 0)) {
        wp_data->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
        return BACNET_STATUS_ERROR;
    }

    return 0;
}

static int device_read_manual_slave_address_binding(object_instance_t *object,
            BACNET_READ_PROPERTY_DATA *rp_data, RR_RANGE *range)
{
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    return read_manual_binding(rp_data);
}

static int device_write_manual_slave_address_binding(object_instance_t *object,
            BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    if (manual_binding_set(wp_data->application_data, wp_data->application_data_len)
            != wp_data->application_data_len) {
        return BACNET_STATUS_ERROR;
    }

    return 0;
}

static int device_read_slave_proxy_enable_RR(object_instance_t *object,
            BACNET_READ_PROPERTY_DATA *rp_data, RR_RANGE *range)
{
    bool enable;
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

    item_count = slave_proxy_port_number();
    
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
        if ((item_data_len + 1) > sizeof(item_data)) {
            bitstring_set_bit(&ResultFlags, RESULT_FLAG_MORE_ITEMS, true);
            break;
        }
        
        if (slave_proxy_get(index, &enable, NULL) < 0) {
            break;
        }
        
        item_data_len += encode_application_boolean(&item_data[item_data_len], enable);
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

static int device_read_slave_proxy_enable(object_instance_t *object,
            BACNET_READ_PROPERTY_DATA *rp_data, RR_RANGE *range)
{
    uint8_t *pdu;
    bool enable;

    if (range != NULL) {
        return device_read_slave_proxy_enable_RR(object, rp_data, range);
    }
    
    pdu = rp_data->application_data;
    
    if (rp_data->array_index == 0){
        return encode_application_unsigned(pdu, slave_proxy_port_number());
    }
    
    if (rp_data->array_index == BACNET_ARRAY_ALL) {
        int pdu_len, i;
        int port_number;
        
        i = 0;
        pdu_len = 0;
        port_number = slave_proxy_port_number();
        for (i = 0; i < port_number; ++i) {
            (void)slave_proxy_get(i, &enable, NULL);
            pdu_len += encode_application_boolean(&pdu[pdu_len], enable);
            if (pdu_len > rp_data->application_data_len) {
                rp_data->abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
                return BACNET_STATUS_ABORT;
            }
        }
        
        return pdu_len;
    }

    if (slave_proxy_get(rp_data->array_index - 1, &enable, NULL) < 0) {
        rp_data->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
        return BACNET_STATUS_ERROR;
    }

    return encode_application_boolean(pdu, enable);
}

static int device_write_slave_proxy_enable(object_instance_t *object,
            BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    bool auto_discovery;
    bool enable;

    if (wp_data->array_index == 0) {
        wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
        return BACNET_STATUS_ERROR;
    }
    
    if (wp_data->array_index == BACNET_ARRAY_ALL) {
        int len, idx, dec_len;
        for (len = 0, idx = 0; len < wp_data->application_data_len; ++idx) {
            dec_len = decode_application_boolean(&(wp_data->application_data[len]), &enable);
            if ((dec_len < 0) || (len + dec_len > wp_data->application_data_len)) {
                return BACNET_STATUS_ERROR;
            }
            len += dec_len;

            if ((slave_proxy_get(idx, NULL, &auto_discovery) < 0)
                    || (slave_proxy_enable(idx, enable, auto_discovery) < 0)) {
                break;
            }
        }
        
        return 0;
    }
    
    if (decode_application_boolean(wp_data->application_data, &enable)
            != wp_data->application_data_len) {
        return BACNET_STATUS_ERROR;
    }

    if ((slave_proxy_get(wp_data->array_index - 1, NULL, &auto_discovery) < 0)
            || (slave_proxy_enable(wp_data->array_index - 1, enable, auto_discovery) < 0)) {
        wp_data->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
        return BACNET_STATUS_ERROR;
    }

    return 0;
}

static int device_read_slave_address_binding(object_instance_t *object,
            BACNET_READ_PROPERTY_DATA *rp_data, RR_RANGE *range)
{
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    return read_slave_binding(rp_data);
}

static int device_write_slave_address_binding(object_instance_t *object,
            BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    if (slave_binding_set(wp_data->application_data, wp_data->application_data_len)
            != wp_data->application_data_len) {
        return BACNET_STATUS_ERROR;
    }

    return 0;
}

bool device_enable_mstp_slave_proxy_support(void)
{
    property_impl_t *p_impl;
    
    device_impl_init();
    
    if (!device_impl) {
        APP_ERROR("%s: device impl init failed\r\n", __func__);
        return false;
    }

    p_impl = object_impl_extend(device_impl, PROP_AUTO_SLAVE_DISCOVERY, PROPERTY_TYPE_OPTIONAL);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_AUTO_SLAVE_DISCOVERY failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = device_read_auto_slave_discovery;
    p_impl->write_property = device_write_auto_slave_discovery;

    p_impl = object_impl_extend(device_impl, PROP_MANUAL_SLAVE_ADDRESS_BINDING, PROPERTY_TYPE_OPTIONAL);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_MANUAL_SLAVE_ADDRESS_BINDING failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = device_read_manual_slave_address_binding;
    p_impl->write_property = device_write_manual_slave_address_binding;

    p_impl = object_impl_extend(device_impl, PROP_SLAVE_PROXY_ENABLE, PROPERTY_TYPE_OPTIONAL);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_SLAVE_PROXY_ENABLE failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = device_read_slave_proxy_enable;
    p_impl->write_property = device_write_slave_proxy_enable;

    p_impl = object_impl_extend(device_impl, PROP_SLAVE_ADDRESS_BINDING, PROPERTY_TYPE_OPTIONAL);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_SLAVE_ADDRESS_BINDING failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = device_read_slave_address_binding;
    p_impl->write_property = device_write_slave_address_binding;

    return true;

out:

    return false;
}

property_impl_t *device_impl_extend(BACNET_PROPERTY_ID object_property,
                    property_type_t property_type)
{
    return object_impl_extend(device_impl, object_property, property_type);
}

uint16_t device_vendor_identifier(void)
{
    return BACNET_VENDOR_ID;
}

bool device_reinitialize(BACNET_REINITIALIZE_DEVICE_DATA *rd_data)
{
    if (rd_data == NULL) {
        APP_ERROR("%s: null argument\r\n", __func__);
        return false;
    }

    if (!characterstring_ansi_same(&rd_data->password, DEVICE_REINITIALIZATION_PASSWORD,
            sizeof(DEVICE_REINITIALIZATION_PASSWORD) - 1)) {
        APP_WARN("%s: password error\r\n", __func__);
        rd_data->error_class = ERROR_CLASS_SECURITY;
        rd_data->error_code = ERROR_CODE_PASSWORD_FAILURE;
        return false;
    }

    switch (rd_data->state) {
    case BACNET_REINIT_COLDSTART:
    case BACNET_REINIT_WARMSTART:
        (void)dcc_set_status_duration(COMMUNICATION_ENABLE, 0);
        break;

    case BACNET_REINIT_STARTBACKUP:
        break;

    case BACNET_REINIT_ENDBACKUP:
        break;

    case BACNET_REINIT_STARTRESTORE:
        break;

    case BACNET_REINIT_ENDRESTORE:
        break;

    case BACNET_REINIT_ABORTRESTORE:
        break;

    default:
        break;
    }

    return true;
}

int device_init(cJSON *object)
{
    object_instance_t *device;
    cJSON *tmp;

    if (device_instance) {
        return OK;
    }
    
    if (object == NULL) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    device_impl_init();
    if (!device_impl) {
        APP_ERROR("%s: device impl init failed\r\n", __func__);
        return -EPERM;
    }

    device = (object_instance_t *)malloc(sizeof(object_instance_t));
    if (!device) {
        APP_ERROR("%s: not enough memory\r\n", __func__);
        return -EPERM;
    }
    memset(device, 0, sizeof(*device));
    device->type = device_impl;

    tmp = cJSON_GetObjectItem(object, "Device_Id");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        APP_ERROR("%s: get Device_Id item failed\r\n", __func__);
        goto out;
    }
    
    if ((uint32_t)tmp->valueint >= BACNET_MAX_INSTANCE) {
        APP_ERROR("%s: invalid Device_Id(%d)\r\n", __func__, tmp->valueint);
        goto out;
    }
    device->instance = (uint32_t)tmp->valueint;

    tmp = cJSON_GetObjectItem(object, "Device_Name");
    if ((tmp == NULL) || (tmp->type != cJSON_String)) {
        APP_ERROR("%s: get Device_Name item failed\r\n", __func__);
        goto out;
    }
    if (!vbuf_fr_str(&device->object_name.vbuf, tmp->valuestring, OBJECT_NAME_MAX_LEN)) {
        APP_ERROR("%s: set name overflow\r\n", __func__);
        goto out;
    }

    if (!object_add(device)) {
        APP_ERROR("%s: object add failed\r\n", __func__);
        goto out;
    }

    device_instance = device;
    APP_VERBOS("%s: OK\r\n", __func__);

    return OK;

out:
    free(device);
    
    return -EPERM;
}

