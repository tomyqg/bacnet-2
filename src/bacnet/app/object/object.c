/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * object.c
 * Original Author:  linzhixian, 2015-12-24
 *
 * BACnet Object
 *
 * History
 */

#include <stdlib.h>
#include <errno.h>

#include "bacnet/object/object.h"
#include "bacnet/object/device.h"
#include "bacnet/object/ai.h"
#include "bacnet/object/ao.h"
#include "bacnet/object/av.h"
#include "bacnet/object/bi.h"
#include "bacnet/object/bo.h"
#include "bacnet/object/bv.h"
#include "bacnet/object/msi.h"
#include "bacnet/object/mso.h"
#include "bacnet/object/msv.h"
#include "bacnet/object/trendlog.h"
#include "bacnet/bactext.h"
#include "bacnet/config.h"
#include "bacnet/app.h"
#include "misc/hashtable.h"
#include "bacnet/bacdcode.h"

#define NAME_TABLE_BITS     (7)

static DECLARE_HASHTABLE(name_table, NAME_TABLE_BITS);   /* name to instance */

static uint8_t object_types_supported_bits[(MAX_ASHRAE_OBJECT_TYPE + 7) >> 3] = {0, };

static struct rb_root object_root = {.rb_node = NULL};

static bool Object_Initialized = false;

bool client_device = false;

static BACNET_OBJECT_TYPE Object_Types_Supported[] = {
    OBJECT_ANALOG_INPUT,
    OBJECT_ANALOG_OUTPUT,
    OBJECT_ANALOG_VALUE,
    OBJECT_BINARY_INPUT,
    OBJECT_BINARY_OUTPUT,
    OBJECT_BINARY_VALUE,
    OBJECT_DEVICE,
    OBJECT_MULTI_STATE_INPUT,
    OBJECT_MULTI_STATE_OUTPUT,
    OBJECT_MULTI_STATE_VALUE,
    OBJECT_TRENDLOG
};

static int __string_hash(const char *str, uint32_t len)
{
    const char *end;
    int code;
    
    code = 0;
    end = str + len;
    do {
        code = ROTATE_LEFT(code, 8);
        code += (uint8_t)*str;
    } while (++str < end);

    return code;
}

static object_store_t *_find_store(BACNET_OBJECT_TYPE type)
{
    struct rb_node *snode;
    object_store_t *store;
    
    snode = object_root.rb_node;
    while (snode) {
        store = rb_entry(snode, object_store_t, node);
        if (type < store->object_type) {
            snode = snode->rb_left;
        } else if (type > store->object_type) {
            snode = snode->rb_right;
        } else {
            return store;
        }
    }
    
    return NULL;
}

static object_instance_t *_find_instance(object_store_t *store, uint32_t instance)
{
    struct rb_node *onode;
    object_instance_t *object;

    onode = store->instance_root.rb_node;
    while (onode) {
        object = rb_entry(onode, object_instance_t, node_type);
        if (instance < object->instance) {
            onode = onode->rb_left;
        } else if (instance > object->instance) {
            onode = onode->rb_right;
        } else {
            return object;
        }
    }
    
    return NULL;
}

static object_instance_t *_find_name(int key, const char *str, uint32_t len)
{
    object_instance_t *object;
    
    hash_for_each_possible(name_table, object, node_name, key) {
        if ((object->object_name.vbuf.length == len)
                && (!memcmp(object->object_name.vbuf.value, str, len))) {
            return object;
        }
    }
    
    return NULL;
}

object_instance_t *object_find(BACNET_OBJECT_TYPE type, uint32_t instance)
{
    object_store_t *store;
    
    if (((uint32_t)type >= MAX_BACNET_OBJECT_TYPE) || (instance >= BACNET_MAX_INSTANCE)) {
        APP_ERROR("%s: invalid argument, type(%d), instance(%d)\r\n", __func__, type, instance);
        return NULL;
    }

    store = _find_store(type);
    if (store) {
        return _find_instance(store, instance);
    }

    return NULL;
}

bool object_add(object_instance_t *object)
{
    struct rb_node **pps, *ps;
    struct rb_node **ppo, *po;
    object_store_t *store;
    object_instance_t *obj;
    BACNET_OBJECT_TYPE type;
    uint32_t instance;
    int key;

    if (!object) {
        APP_ERROR("%s: null argument\r\n", __func__);
        return false;
    }
    if (!object->type) {
        APP_ERROR("%s: object not bind yet\r\n", __func__);
        return false;
    }
    if (object->type->property_required_count
            + object->type->property_optional_count
            + object->type->property_proprietary_count <= 0) {
        APP_ERROR("%s: invalid type without property supported\r\n", __func__);
        return false;
    }

    type = object->type->type;
    instance = object->instance;
    if ((uint32_t)type >= MAX_BACNET_OBJECT_TYPE) {
        APP_ERROR("%s: object bind to invalid type(%d)\r\n", __func__, type);
        return false;
    }
    if (instance >= BACNET_MAX_INSTANCE) {
        APP_ERROR("%s: invalid instance(%d)\r\n", __func__, instance);
        return false;
    }

    if (hash_hashed(&object->node_name)) {
        APP_ERROR("%s: duplicated add object?\r\n", __func__);
        return false;
    }

    key = __string_hash((char*)object->object_name.vbuf.value,
            object->object_name.vbuf.length);
    if (_find_name(key, (char*)object->object_name.vbuf.value,
            object->object_name.vbuf.length)) {
        APP_ERROR("%s: duplicated name\r\n", __func__);
        return false;
    }

    pps = &object_root.rb_node;
    ps = NULL;
    while (*pps) {
        ps = *pps;
        store = rb_entry(ps, object_store_t, node);
        if (type < store->object_type) {
            pps = &ps->rb_left;
        } else if (type > store->object_type) {
            pps = &ps->rb_right;
        } else {
            ppo = &store->instance_root.rb_node;
            po = NULL;
            while (*ppo) {
                po = *ppo;
                obj = rb_entry(po, object_instance_t, node_type);
                if (instance < obj->instance) {
                    ppo = &po->rb_left;
                } else if (instance > obj->instance) {
                    ppo = &po->rb_right;
                } else {
                    APP_ERROR("%s: duplicated instance(%d)\r\n", __func__, instance);
                    return false;
                }
            }

            store->object_count++;
            rb_link_node(&object->node_type, po, ppo);
            rb_insert_color(&object->node_type, &store->instance_root);
            goto end;
        }
    }

    store = (object_store_t *)malloc(sizeof(object_store_t));
    if (!store) {
        APP_ERROR("%s: malloc object store failed\r\n", __func__);
        return false;
    }
    
    rb_link_node(&object->node_type, NULL, &store->instance_root.rb_node);
    rb_insert_color(&object->node_type, &store->instance_root);
    store->object_count = 1;
    store->object_type = type;
    rb_link_node(&store->node, ps, pps);
    rb_insert_color(&store->node, &object_root);
    
end:
    hash_add(name_table, &object->node_name, key);
    
    return true;
}

void object_detach(object_instance_t *object)
{
    object_store_t *store;
    
    if (!object) {
        APP_ERROR("%s: null argument\r\n", __func__);
        return;
    }

    if (!hash_hashed(&object->node_name)) {
        APP_ERROR("%s: object already detached?\r\n", __func__);
        return;
    }

    store = _find_store(object->type->type);
    if (!store) {
        APP_ERROR("%s: store not found\r\n", __func__);
        return;
    }

    rb_erase(&object->node_type, &store->instance_root);
    hash_del(&object->node_name);
    if (!--store->object_count) {
        rb_erase(&store->node, &object_root);
    }
}

const property_impl_t *object_impl_find_property(const object_impl_t *type,
                        BACNET_PROPERTY_ID property)
{
    int start, end, idx;
    
    if (!type) {
        APP_ERROR("%s: null argument\r\n", __func__);
        return NULL;
    }

    start = 0;
    end = type->property_required_count + type->property_optional_count
            + type->property_proprietary_count - 1;

    if (((uint32_t) type->all_property[start].object_property
            > (uint32_t) property)
            || ((uint32_t) type->all_property[end].object_property
                    < (uint32_t) property)) {
        APP_ERROR("%s: invalid property(%d)\r\n", __func__, property);
        return NULL;
    }

    /* divide to 2 to find property_id */
    for (idx = end/2; ;) {
        if ((uint32_t)property < (uint32_t)type->all_property[idx].object_property) {
            end = idx - 1;
            if (end < start) {
                break;
            }
        } else if ((uint32_t)property > (uint32_t)type->all_property[idx].object_property) {
            start = idx + 1;
            if (start > end) {
                break;
            }
        } else {
            return &type->all_property[idx];
        }
        
        idx = (start + end)/2;
    }

    return NULL;
}

object_impl_t *object_impl_clone(const object_impl_t *type)
{
    object_impl_t *newtype;
    int count;
    
    if (!type) {
        APP_ERROR("%s: null argument\r\n", __func__);
        return NULL;
    }

    newtype = (object_impl_t *)malloc(sizeof(object_impl_t));
    if (!newtype) {
        goto outofmem;
    }
    memset(newtype, 0, sizeof(object_impl_t));

    newtype->type = type->type;
    newtype->property_required_count = type->property_required_count;
    newtype->property_optional_count = type->property_optional_count;
    newtype->property_proprietary_count = type->property_proprietary_count;

    count = type->property_required_count + type->property_optional_count
        + type->property_proprietary_count;

    newtype->all_property = (property_impl_t *)malloc(sizeof(property_impl_t) * count);
    if (!newtype->all_property) {
        goto outofmem;
    }
    memcpy(newtype->all_property, type->all_property, sizeof(property_impl_t) * count);

    newtype->property_required = (BACNET_PROPERTY_ID *)malloc(sizeof(BACNET_PROPERTY_ID)
        * type->property_required_count);
    if (!newtype->property_required) {
        goto outofmem;
    }
    memcpy(newtype->property_required, type->property_required,
        sizeof(BACNET_PROPERTY_ID) * type->property_required_count);

    newtype->property_optional = (BACNET_PROPERTY_ID *)malloc(sizeof(BACNET_PROPERTY_ID) 
        * type->property_optional_count);
    if (!newtype->property_optional) {
        goto outofmem;
    }
    memcpy(newtype->property_optional, type->property_optional,
        sizeof(BACNET_PROPERTY_ID) * type->property_optional_count);

    newtype->property_proprietary = (BACNET_PROPERTY_ID *)malloc(sizeof(BACNET_PROPERTY_ID)
        * type->property_proprietary_count);
    if (!newtype->property_proprietary) {
        goto outofmem;
    }
    memcpy(newtype->property_proprietary, type->property_proprietary,
        sizeof(BACNET_PROPERTY_ID) * type->property_proprietary_count);

    return newtype;

outofmem:
    APP_ERROR("%s: not enough memory\r\n", __func__);
    if (newtype) {
        if (newtype->all_property) {
            free(newtype->all_property);
        }
        if (newtype->property_required) {
            free(newtype->property_required);
        }
        if (newtype->property_optional) {
            free(newtype->property_optional);
        }
        if (newtype->property_proprietary) {
            free(newtype->property_proprietary);
        }
        
        free(newtype);
    }
    
    return NULL;
}

void object_impl_destroy(object_impl_t *type)
{
    if (!type) {
        APP_ERROR("%s: null argument\r\n", __func__);
        return;
    }

    if (type->all_property) {
        free(type->all_property);
    }
    
    if (type->property_required) {
        free(type->property_required);
    }
    
    if (type->property_optional) {
        free(type->property_optional);
    }
    
    if (type->property_proprietary) {
        free(type->property_proprietary);
    }
    
    free(type);
}

/** found id in id_array
 * @return true if found, false if not found
 */
static bool _find_property_id(BACNET_PROPERTY_ID *id_array, uint16_t count, BACNET_PROPERTY_ID id)
{
    int i;
    
    for (i = 0; i < count; ++i) {
        if (id_array[i] == id) {
            return true;
        }
    }
    
    return false;
}

/** append new property_id to property_id array
 * @param id_array, input/output, return new property_id array
 * @param count, input/output, return new count
 * @param newid, input, new property id to append
 * @return true, success append, false fail
 */
static bool _append_property_id(BACNET_PROPERTY_ID **id_array, uint16_t *count,
                BACNET_PROPERTY_ID newid)
{
    BACNET_PROPERTY_ID *newarray = (BACNET_PROPERTY_ID *)realloc(*id_array,
        sizeof(BACNET_PROPERTY_ID) * (*count + 1));
    if (!newarray) {
        return false;
    }

    *id_array = newarray;
    newarray[*count] = newid;
    (*count)++;
    return true;
}

property_impl_t *object_impl_extend(object_impl_t *type, BACNET_PROPERTY_ID object_property,
                    property_type_t property_type)
{
    property_impl_t *newall;
    int start, end, idx;
    int count;
    bool found;
    
    if (!type) {
        APP_ERROR("%s: null argument\r\n", __func__);
        return NULL;
    }
    
    if ((uint32_t)property_type >= PROPERTY_TYPE_MAX) {
        APP_ERROR("%s: invalid property type(%d)\r\n", __func__, property_type);
        return NULL;
    }

    /* divide to 2 to find property_id */
    count = type->property_required_count + type->property_optional_count
        + type->property_proprietary_count;

    start = 0;
    end = count - 1;
    
    if ((uint32_t)type->all_property[start].object_property
            > (uint32_t)object_property) {
        goto notfound;
    }
    
    if ((uint32_t)type->all_property[end].object_property
            < (uint32_t)object_property) {
        start = count;
        goto notfound;
    }
    
    for (idx = end/2; ;) {
        if ((uint32_t)object_property < type->all_property[idx].object_property) {
            end = idx - 1;
            if (end < start) {
                break;
            }
        } else if ((uint32_t)object_property > type->all_property[idx].object_property) {
            start = idx + 1;
            if (start > end) {
                break;
            }
        } else {
            switch(property_type) {
            case PROPERTY_TYPE_REQUIRED:
                found = _find_property_id(type->property_required,
                        type->property_required_count, object_property);
                break;
            
            case PROPERTY_TYPE_OPTIONAL:
                found = _find_property_id(type->property_optional,
                        type->property_optional_count, object_property);
                break;
            
            case PROPERTY_TYPE_PROPRIETARY:
                found = _find_property_id(type->property_proprietary,
                        type->property_proprietary_count, object_property);
                break;
            
            default:
                found = false;
                break;
            }
            
            if (!found) {
                APP_ERROR("%s: property(%d) type not match\r\n", __func__, object_property);
                return NULL;
            }
            
            return &type->all_property[idx];
        }
        
        idx = (start + end)/2;
    }

notfound:
    newall = (property_impl_t *)realloc(type->all_property, sizeof(property_impl_t) * (count + 1));
    if (!newall) {
        goto out;
    }
    type->all_property = newall;

    switch(property_type) {
    case PROPERTY_TYPE_REQUIRED:
        if (!_append_property_id(&type->property_required,
                &type->property_required_count, object_property))
            goto out;
        break;
    
    case PROPERTY_TYPE_OPTIONAL:
        if (!_append_property_id(&type->property_optional,
                &type->property_optional_count, object_property))
            goto out;
        break;
    
    case PROPERTY_TYPE_PROPRIETARY:
        if (!_append_property_id(&type->property_proprietary,
                &type->property_proprietary_count, object_property))
            goto out;
        break;

    default:
        goto out;
    }

    memmove(newall + start + 1, newall + start, sizeof(property_impl_t) * (count - start));
    newall[start].object_property = object_property;
    newall[start].read_property = NULL;
    newall[start].write_property = NULL;

    return &newall[start];

out:
    APP_ERROR("%s: not enough memory\r\n", __func__);
    
    return NULL;
}

static int base_read_object_type(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }
    
    return encode_application_enumerated(rp_data->application_data, object->type->type);
}

static int base_read_object_id(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }
    
    return encode_application_object_id(rp_data->application_data, object->type->type,
            object->instance);
}

static int base_read_object_name(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }
    
    if (object->object_name.vbuf.length >= rp_data->application_data_len) {
        rp_data->abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
        return BACNET_STATUS_ABORT;
    }
    
    return encode_application_ansi_character_string(rp_data->application_data,
            (char *)object->object_name.vbuf.value, object->object_name.vbuf.length);
}

object_impl_t *object_create_impl_base(void)
{
    object_impl_t *base;
    property_impl_t *p_impl;
    
    base = (object_impl_t *)malloc(sizeof(object_impl_t));
    if (!base) {
        APP_ERROR("%s: not enough memory\r\n", __func__);
        return NULL;
    }
    memset(base, 0, sizeof(object_impl_t));

    base->type = MAX_BACNET_OBJECT_TYPE;
    base->property_required_count = 1;
    base->all_property = (property_impl_t *)malloc(sizeof(property_impl_t));
    if (!base->all_property) {
        APP_ERROR("%s: not enough memory\r\n", __func__);
        goto out;
    }
    base->all_property[0].object_property = PROP_OBJECT_IDENTIFIER;
    base->all_property[0].read_property = base_read_object_id;
    base->all_property[0].write_property = NULL;

    base->property_required = (BACNET_PROPERTY_ID *)malloc(sizeof(BACNET_PROPERTY_ID));
    if (!base->property_required) {
        APP_ERROR("%s: not enough memory\r\n", __func__);
        goto out;
    }
    base->property_required[0] = PROP_OBJECT_IDENTIFIER;

    p_impl = object_impl_extend(base, PROP_OBJECT_NAME, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_OBJECT_NAME failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = base_read_object_name;

    p_impl = object_impl_extend(base, PROP_OBJECT_TYPE, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_OBJECT_TYPE failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = base_read_object_type;

    return base;

out:
    object_impl_destroy(base);
    
    return NULL;
}

static int seor_read_status_flag(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    BACNET_BIT_STRING bit_string;
    object_seor_t *seo_obj;
    uint8_t tmpbuf[4];
    
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    seo_obj = container_of(object, object_seor_t, base);

    bitstring_init(&bit_string, tmpbuf, 4);
    bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM, seo_obj->Event_State != EVENT_STATE_NORMAL);
    bitstring_set_bit(&bit_string, STATUS_FLAG_FAULT,
        seo_obj->Reliability != RELIABILITY_NO_FAULT_DETECTED);
    bitstring_set_bit(&bit_string, STATUS_FLAG_OVERRIDDEN, seo_obj->Overridden);
    bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE, seo_obj->Out_Of_Service);

    return encode_application_bitstring(rp_data->application_data, &bit_string);
}

static int seor_read_event_state(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_seor_t *seo_obj;

    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    seo_obj = container_of(object, object_seor_t, base);

    return encode_application_enumerated(rp_data->application_data, seo_obj->Event_State);
}

static int seor_read_out_of_service(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_seor_t *seor_obj;
    
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    seor_obj = container_of(object, object_seor_t, base);

    return encode_application_boolean(rp_data->application_data, seor_obj->Out_Of_Service);
}

static int seor_write_out_of_service(object_instance_t *object, BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    object_seor_t *seor_obj;
    bool value;
    bool prevalue;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    if (decode_application_boolean(wp_data->application_data, &value)
            != wp_data->application_data_len) {
        return BACNET_STATUS_ERROR;
    }
    
    seor_obj = container_of(object, object_seor_t, base);
    prevalue = seor_obj->Out_Of_Service;
    seor_obj->Out_Of_Service = value;
    
    if (prevalue && !value) {
        if (seor_obj->notify) {
            seor_obj->notify(seor_obj);
        }
    }
    
    return 0;
}

static int seor_read_reliability(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_seor_t *seor_obj;

    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    seor_obj = container_of(object, object_seor_t, base);

    return encode_application_enumerated(rp_data->application_data, seor_obj->Reliability);
}

static int seor_write_reliability(object_instance_t *object, BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    object_seor_t *seor_obj;
    uint32_t value;

    seor_obj = container_of(object, object_seor_t, base);

    if (!seor_obj->Out_Of_Service) {
        wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;;
        return BACNET_STATUS_ERROR;
    }

    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    if (decode_application_enumerated(wp_data->application_data, &value)
            != wp_data->application_data_len) {
        return BACNET_STATUS_ERROR;
    }

    if (value >= MAX_BACNET_RELIABILITY) {
        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
        return BACNET_STATUS_ERROR;
    }

    seor_obj->Reliability = value;
    
    return 0;
}

object_impl_t *object_create_impl_seor(bool has_event_state, bool has_out_of_service,
                bool reliability_optional)
{
    object_impl_t *seor;
    property_impl_t *p_impl;
    
    seor = object_create_impl_base();
    if (!seor) {
        APP_ERROR("%s: create base impl failed\r\n", __func__);
        return NULL;
    }

    p_impl = object_impl_extend(seor, PROP_STATUS_FLAGS, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_STATUS_FLAGS failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = seor_read_status_flag;

    if (has_event_state) {
        p_impl = object_impl_extend(seor, PROP_EVENT_STATE, PROPERTY_TYPE_REQUIRED);
        if (!p_impl) {
            APP_ERROR("%s: extend PROP_EVENT_STATE failed\r\n", __func__);
            goto out;
        }
        p_impl->read_property = seor_read_event_state;
    }

    if (has_out_of_service) {
        p_impl = object_impl_extend(seor, PROP_OUT_OF_SERVICE, PROPERTY_TYPE_REQUIRED);
        if (!p_impl) {
            APP_ERROR("%s: extend PROP_OUT_OF_SERVICE failed\r\n", __func__);
            goto out;
        }
        p_impl->read_property = seor_read_out_of_service;
        p_impl->write_property = seor_write_out_of_service;
    }

    p_impl = object_impl_extend(seor, PROP_RELIABILITY,
        reliability_optional? PROPERTY_TYPE_OPTIONAL : PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_RELIABILITY failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = seor_read_reliability;
    p_impl->write_property = seor_write_reliability;

    return seor;

out:
    object_impl_destroy(seor);
    
    return NULL;
}

static int Object_Types_Supported_Init(void)
{
    BACNET_BIT_STRING types;
    uint32_t size;
    int i;

    size = sizeof(Object_Types_Supported)/sizeof(Object_Types_Supported[0]);
    
    (void)bitstring_init(&types, object_types_supported_bits, MAX_ASHRAE_OBJECT_TYPE);

    for (i = 0; i < size; i++) {
        bitstring_set_bit(&types, Object_Types_Supported[i], true);
    }
    
    return OK;
}

static bool Object_Type_Is_Supported(BACNET_OBJECT_TYPE object_type)
{
    BACNET_BIT_STRING types;
    
    if (object_type >= MAX_ASHRAE_OBJECT_TYPE) {
        return false;
    }

    (void)bitstring_init(&types, object_types_supported_bits, MAX_ASHRAE_OBJECT_TYPE);
    
    return bitstring_get_bit(&types, object_type);
}

bool object_get_types_supported(BACNET_BIT_STRING *types)
{
    if (types == NULL) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return false;
    }
    
    if (Object_Initialized == false) {
        APP_ERROR("%s: Object is not inited\r\n", __func__);
        return false;
    }

    return bitstring_init(types, object_types_supported_bits, MAX_ASHRAE_OBJECT_TYPE);
}

uint32_t object_list_count(void)
{
    struct rb_node *node;
    object_store_t *store;
    uint32_t count;

    count = 0;
    for (node = rb_first(&object_root); node; node = rb_next(node)) {
        store = rb_entry(node, object_store_t, node);
        count += store->object_count;
    }

    return count;
}

bool object_find_index(uint32_t index, object_store_t **store, object_instance_t **object)
{
    struct rb_node *snode, *onode;
    object_store_t *ss;
    
    for (snode = rb_first(&object_root); snode; snode = rb_next(snode)) {
        ss = rb_entry(snode, object_store_t, node);
        if (ss->object_count <= index) {
            index -= ss->object_count;
            continue;
        }

        for (onode = rb_first(&ss->instance_root); onode; onode = rb_next(onode)) {
            if (!index) {
                if (store) {
                    *store = ss;
                }
                if (object) {
                    *object = rb_entry(onode, object_instance_t, node_type);
                }
                return true;
            }
            index--;
        }
        
        APP_ERROR("%s: instance count not correct\r\n", __func__);
        break;
    }

    return false;
}

bool object_find_next(object_store_t **store, object_instance_t **object)
{
    struct rb_node *snode, *onode;
    
    if (store == NULL || *store == NULL) {
        APP_ERROR("%s: store or store pointing to is null\r\n", __func__);
        return false;
    }
    
    if (object == NULL || *object == NULL) {
        APP_ERROR("%s: object or object pointing to is null\r\n", __func__);
        return false;
    }

    snode = &(*store)->node;
    onode = rb_next(&(*object)->node_type);
    while (!onode) {
        snode = rb_next(snode);
        if (!snode) {
            return false;
        }
        onode = rb_first(&rb_entry(snode, object_store_t, node)->instance_root);
    }

    *store = rb_entry(snode, object_store_t, node);
    *object = rb_entry(onode, object_instance_t, node_type);
    
    return true;
}

bool object_type_find_object_index(BACNET_OBJECT_TYPE type, uint32_t index,
        object_instance_t **object)
{
    object_store_t *store;

    if (((uint32_t)type >= MAX_BACNET_OBJECT_TYPE)) {
        APP_ERROR("%s: invalid object type(%d)\r\n", __func__, type);
        return NULL;
    }

    store = _find_store(type);
    if (!store) {
        return false;
    }

    for (struct rb_node *onode = rb_first(&store->instance_root);
            onode; onode = rb_next(onode)) {
        if (!index) {
            if (object) {
                *object = rb_entry(onode, object_instance_t, node_type);
            }
            return true;
        }
        index--;
    }

    return false;
}

bool object_type_find_object_next(object_instance_t **object)
{
    struct rb_node *onode;
    
    if (object == NULL || *object == NULL) {
        APP_ERROR("%s: object or object pointing to is null\r\n", __func__);
        return false;
    }

    onode = rb_next(&(*object)->node_type);
    if (onode) {
        *object = rb_entry(onode, object_instance_t, node_type);
        return true;
    }

    return false;
}

bool object_get_name(BACNET_OBJECT_TYPE type, uint32_t instance, BACNET_CHARACTER_STRING *name)
{
    object_instance_t *object;

    object = object_find(type, instance);
    if (!object) {
        return false;
    }
    
    return characterstring_init_ansi(name, (char *)object->object_name.vbuf.value,
            object->object_name.vbuf.length);
}

/** 
 * Determine if we have an object with the given object_name.
 * If the object_type and object_instance pointers are not null,
 * and the lookup succeeds, they will be given the resulting values.
 * @param object_name[in]: The desired Object Name to look for.
 * @param object_type[out]: The BACNET_OBJECT_TYPE of the matching Object.
 * @param object_instance[out]: The object instance number of the matching Object.
 * @return True on success or else False if not found.
 */
bool object_find_name(BACNET_CHARACTER_STRING *object_name, BACNET_OBJECT_TYPE *object_type,
        uint32_t *object_instance)
{
    object_instance_t *object;
    int key;
    
    if (!object_name) {
        APP_ERROR("%s: null object_name\r\n", __func__);
        return false;
    }
    
    if (object_name->length && !object_name->value) {
        return false;
    }
    
    if ((object_name->encoding != CHARACTER_ANSI_X34) || (object_name->length > OBJECT_NAME_MAX_LEN)) {
        return false;
    }
    
    if (Object_Initialized == false) {
        return false;
    }

    key = __string_hash(object_name->value, object_name->length);
    object = _find_name(key, object_name->value, object_name->length);
    if (object) {
        if (object_type) {
            *object_type = object->type->type;
        }
        if (object_instance) {
            *object_instance = object->instance;
        }
        
        return true;
    }

    return false;
}

BACNET_ERROR_CODE object_rename(object_instance_t *object, BACNET_CHARACTER_STRING *new_name)
{
    object_instance_t *found;
    int key;

    if (!object || !new_name) {
        APP_ERROR("%s: null argument\r\n", __func__);
        return ERROR_CODE_OTHER;
    }
    
    if (new_name->length && !new_name->value) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return ERROR_CODE_OTHER;
    }

    if ((new_name->encoding != CHARACTER_ANSI_X34)) {
        return ERROR_CODE_CHARACTER_SET_NOT_SUPPORTED;
    }
    
    if (new_name->length > OBJECT_NAME_MAX_LEN) {
        return ERROR_CODE_NO_SPACE_TO_WRITE_PROPERTY;
    }
    
    if (Object_Initialized == false) {
        APP_ERROR("%s: object name hash table not initialized\r\n", __func__);
        return ERROR_CODE_OTHER;
    }

    if (new_name->length == 0 || !characterstring_printable(new_name)) {
        return ERROR_CODE_VALUE_OUT_OF_RANGE;
    }

    key = __string_hash(new_name->value, new_name->length);
    found = _find_name(key, new_name->value, new_name->length);

    if (found == object) {
        return MAX_BACNET_ERROR_CODE;
    } else if (found) {
        return ERROR_CODE_DUPLICATE_NAME;
    }

    hash_del(&object->node_name);
    vbuf_fr_buf(&object->object_name.vbuf, (uint8_t*)new_name->value, new_name->length);
    hash_add(name_table, &object->node_name, key);

    device_database_revision_increasee();
    
    return MAX_BACNET_ERROR_CODE;
}

/** For a given object, returns the special property list.
 * This function is used for ReadPropertyMultiple calls which want
 * just Required, just Optional, or All properties.
 *
 * @object_type: [in] The desired BACNET_OBJECT_TYPE whose properties are to be listed.
 * @pPropertyList: [out] Reference to the structure which will, on return, list, separately, 
 *              the Required, Optional, and Proprietary object properties with their counts.
 *
 */
bool object_property_lists(BACNET_OBJECT_TYPE object_type, uint32_t instance,
        special_property_list_t *pPropertyList)
{
    object_instance_t *object;
    
    if (pPropertyList == NULL) {
        return false;
    }

    object = object_find(object_type, instance);
    if (!object) {
        return false;
    }
    
    pPropertyList->Required.pList = object->type->property_required;
    pPropertyList->Required.count = object->type->property_required_count;
    pPropertyList->Optional.pList = object->type->property_optional;
    pPropertyList->Optional.count = object->type->property_optional_count;
    pPropertyList->Proprietary.pList = object->type->property_proprietary;
    pPropertyList->Proprietary.count = object->type->property_proprietary_count;

    return true;
}

static int _read_property_list_RR(const object_impl_t *type, BACNET_READ_PROPERTY_DATA *rp_data,
                RR_RANGE *range)
{
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

    item_count = type->property_required_count + type->property_optional_count
                    + type->property_proprietary_count - 3;
    
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

        if (index < type->property_required_count - 3) {
            item_data_len += encode_application_enumerated(&item_data[item_data_len],
                type->property_required[index + 3]);
        } else {
            index -= type->property_required_count - 3;
            if (index < type->property_optional_count) {
                item_data_len += encode_application_enumerated(&item_data[item_data_len],
                    type->property_optional[index]);
            } else {
                index -= type->property_optional_count;
                item_data_len += encode_application_enumerated(&item_data[item_data_len],
                    type->property_proprietary[index]);
            }
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

/* According to Addendum 135-2010ao, The Object_Name, The Object_Type, Object_Identifier and
 * Property_List properties are not included in the list. The Property_List property shall not
 * be returned when properties ALL or REQUIRED are requested.
 */
static int _read_property_list(const object_impl_t *type, BACNET_READ_PROPERTY_DATA *rp_data,
                RR_RANGE *range)
{
    uint8_t *pdu;
    uint32_t count;
    int pdu_len;
    int idx;

    if (range != NULL) {
        return _read_property_list_RR(type, rp_data, range);
    }
    
    pdu = rp_data->application_data;
    count = type->property_required_count + type->property_optional_count
                + type->property_proprietary_count - 3;
    
    if (rp_data->array_index == 0) {
        return encode_application_unsigned(pdu, count);
    }
    
    if (rp_data->array_index == BACNET_ARRAY_ALL) {
        pdu_len = 0;
        for (idx = 0; idx < type->property_required_count - 3; ++idx) {
            pdu_len += encode_application_enumerated(&pdu[pdu_len], type->property_required[idx + 3]);
            if (pdu_len >= rp_data->application_data_len) {
                rp_data->abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
                return BACNET_STATUS_ABORT;
            }
        }
        
        for (idx = 0; idx < type->property_optional_count; ++idx) {
            pdu_len += encode_application_enumerated(&pdu[pdu_len], type->property_optional[idx]);
            if (pdu_len >= rp_data->application_data_len) {
                rp_data->abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
                return BACNET_STATUS_ABORT;
            }
        }
        
        for (idx = 0; idx < type->property_proprietary_count; ++idx) {
            pdu_len += encode_application_enumerated(&pdu[pdu_len], type->property_proprietary[idx]);
            if (pdu_len >= rp_data->application_data_len) {
                rp_data->abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
                return BACNET_STATUS_ABORT;
            }
        }
        
        return pdu_len;
    }

    if (rp_data->array_index > count) {
        rp_data->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
        return BACNET_STATUS_ERROR;
    }

    idx = rp_data->array_index - 1;
    if (idx < type->property_required_count - 3) {
        return encode_application_enumerated(pdu, type->property_required[idx + 3]);
    }
    
    idx -= type->property_required_count - 3;
    if (idx < type->property_optional_count) {
        return encode_application_enumerated(pdu, type->property_optional[idx]);
    }
    
    idx -= type->property_optional_count;
    
    return encode_application_enumerated(pdu, type->property_proprietary[idx]);
}

/* 将读取到的属性值存储在rp_data->application_data所指的缓冲区里面，并返回本次属性值的编码长度 */
int object_read_property(BACNET_READ_PROPERTY_DATA *rp_data, RR_RANGE *range)
{
    object_store_t *store;
    object_instance_t *object;
    const property_impl_t *impl;
    
    if (rp_data == NULL) {
        APP_ERROR("%s: null rp_data\r\n", __func__);
        return BACNET_STATUS_ERROR;
    }

    if (Object_Initialized == false) {
        APP_ERROR("%s: Object is not inited\r\n", __func__);
        return BACNET_STATUS_ERROR;
    }

    store = _find_store(rp_data->object_type);
    if (!store) {
        APP_ERROR("%s: find object_type(%d) failed\r\n", __func__, rp_data->object_type);
        rp_data->error_class = ERROR_CLASS_OBJECT;
        rp_data->error_code = ERROR_CODE_UNSUPPORTED_OBJECT_TYPE;
        return BACNET_STATUS_ERROR;
    }

    object = _find_instance(store, rp_data->object_instance);
    if (!object) {
        APP_ERROR("%s: find object_instance(%d) failed\r\n", __func__, rp_data->object_instance);
        rp_data->error_class = ERROR_CLASS_OBJECT;
        rp_data->error_code = ERROR_CODE_UNKNOWN_OBJECT;
        return BACNET_STATUS_ERROR;
    }

    if (rp_data->property_id == PROP_PROPERTY_LIST) {
        return _read_property_list(object->type, rp_data, range);
    }

    impl = object_impl_find_property(object->type, rp_data->property_id);
    if (!impl) {
        APP_ERROR("%s: find property(%d) failed\r\n", __func__, rp_data->property_id);
        rp_data->error_class = ERROR_CLASS_PROPERTY;
        rp_data->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
        return BACNET_STATUS_ERROR;
    }
    if (!impl->read_property) {
        APP_ERROR("%s: no read property function\r\n", __func__);
        rp_data->error_class = ERROR_CLASS_PROPERTY;
        rp_data->error_code = ERROR_CODE_READ_ACCESS_DENIED;
        return BACNET_STATUS_ERROR;
    }

    rp_data->error_class = ERROR_CLASS_PROPERTY;
    rp_data->error_code = ERROR_CODE_OTHER;
    
    return impl->read_property(object, rp_data, range);
}

int object_write_property(BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    object_store_t *store;
    object_instance_t *object;
    const property_impl_t *impl;
    
    if (wp_data == NULL) {
        APP_ERROR("%s: null wp_data\r\n", __func__);
        return BACNET_STATUS_ERROR;
    }

    if (Object_Initialized == false) {
        APP_ERROR("%s: Object is not inited\r\n", __func__);
        return BACNET_STATUS_ERROR;
    }
    
    store = _find_store(wp_data->object_type);
    if (!store) {
        wp_data->error_class = ERROR_CLASS_OBJECT;
        wp_data->error_code = ERROR_CODE_UNSUPPORTED_OBJECT_TYPE;
        return BACNET_STATUS_ERROR;
    }

    object = _find_instance(store, wp_data->object_instance);
    if (!object) {
        wp_data->error_class = ERROR_CLASS_OBJECT;
        wp_data->error_code = ERROR_CODE_UNKNOWN_OBJECT;
        return BACNET_STATUS_ERROR;
    }

    if (wp_data->property_id == PROP_PROPERTY_LIST) {
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
        return BACNET_STATUS_ERROR;
    }

    impl = object_impl_find_property(object->type, wp_data->property_id);
    if (!impl) {
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
        return BACNET_STATUS_ERROR;
    }
    if (!impl->write_property) {
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
        return BACNET_STATUS_ERROR;
    }

    wp_data->error_class = ERROR_CLASS_PROPERTY;
    wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
    
    return impl->write_property(object, wp_data);
}

cJSON *object_get_object_list(void)
{
    struct rb_node *snode, *onode;
    object_store_t *store;
    object_instance_t *object;
    cJSON *result, *array, *tmp;
    
    result = cJSON_CreateObject();
    if (result == NULL) {
        APP_ERROR("%s: create result object failed\r\n", __func__);
        return NULL;
    }

    if (Object_Initialized == false) {
        APP_ERROR("%s: Object is not inited\r\n", __func__);
        cJSON_AddNumberToObject(result, "error_code", -1);
        cJSON_AddStringToObject(result, "reason", "Object is not inited");
        return result;
    }

    array = cJSON_CreateArray();
    if (array == NULL) {
        APP_ERROR("%s: create array item failed\r\n", __func__);
        cJSON_AddNumberToObject(result, "error_code", -1);
        cJSON_AddStringToObject(result, "reason", "create result item failed");
        return result;
    }
    
    for (snode = rb_first(&object_root); snode; snode = rb_next(snode)) {
        store = rb_entry(snode, object_store_t, node);
        for (onode = rb_first(&store->instance_root); onode; onode = rb_next(onode)) {
            object = rb_entry(onode, object_instance_t, node_type);
            tmp = cJSON_CreateObject();
            if (tmp == NULL) {
                APP_ERROR("%s: create object failed\r\n", __func__);
                cJSON_Delete(array);
                cJSON_AddNumberToObject(result, "error_code", -1);
                cJSON_AddStringToObject(result, "reason", "create object failed");
                return result;
            }

            cJSON_AddItemToArray(array, tmp);
            cJSON_AddNumberToObject(tmp, "object_type", object->type->type);
            cJSON_AddNumberToObject(tmp, "object_instance", object->instance);
        }
    }

    cJSON_AddItemToObject(result, "result", array);
    
    return result;    
}

cJSON *object_get_property_list(BACNET_OBJECT_TYPE object_type, uint32_t instance)
{
    special_property_list_t all_property_list;
    cJSON *result, *array, *tmp;
    int i;
    
    result = cJSON_CreateObject();
    if (result == NULL) {
        APP_ERROR("%s: create result object failed\r\n", __func__);
        return NULL;
    }

    if (Object_Initialized == false) {
        APP_ERROR("%s: Object is not inited\r\n", __func__);
        cJSON_AddNumberToObject(result, "error_code", -1);
        cJSON_AddStringToObject(result, "reason", "Object is not inited");
        return result;
    }

    if (!object_property_lists(object_type, instance, &all_property_list)) {
        APP_ERROR("%s: get special property list failed\r\n", __func__);
        cJSON_AddNumberToObject(result, "error_code", -1);
        cJSON_AddStringToObject(result, "reason", "get special property list failed");
        return result;
    }
    
    array = cJSON_CreateArray();
    if (array == NULL) {
        APP_ERROR("%s: create result item failed\r\n", __func__);
        cJSON_AddNumberToObject(result, "error_code", -1);
        cJSON_AddStringToObject(result, "reason", "create result item failed");
        return result;
    }

    /* add required property list */
    for (i = 0; i < all_property_list.Required.count; i++) {
        if ((all_property_list.Required.pList[i] == PROP_OBJECT_NAME)
                || (all_property_list.Required.pList[i] == PROP_OBJECT_TYPE)
                || (all_property_list.Required.pList[i] == PROP_OBJECT_IDENTIFIER)
                || (all_property_list.Required.pList[i] == PROP_PROPERTY_LIST)) {
            continue;
        }
    
        tmp = cJSON_CreateObject();
        if (tmp == NULL) {
            APP_ERROR("%s: create object failed\r\n", __func__);
            cJSON_Delete(array);
            cJSON_AddNumberToObject(result, "error_code", -1);
            cJSON_AddStringToObject(result, "reason", "create object failed");
            return result;
        }

        cJSON_AddItemToArray(array, tmp);
        cJSON_AddNumberToObject(tmp, "property_id", all_property_list.Required.pList[i]);
    }

    /* add optional property list */
    for (i = 0; i < all_property_list.Optional.count; i++) {
        tmp = cJSON_CreateObject();
        if (tmp == NULL) {
            APP_ERROR("%s: create object failed\r\n", __func__);
            cJSON_Delete(array);
            cJSON_AddNumberToObject(result, "error_code", -1);
            cJSON_AddStringToObject(result, "reason", "create object failed");
            return result;
        }

        cJSON_AddItemToArray(array, tmp);
        cJSON_AddNumberToObject(tmp, "property_id", all_property_list.Optional.pList[i]);
    }

    /* add proprietary property list */
    for (i = 0; i < all_property_list.Proprietary.count; i++) {
        tmp = cJSON_CreateObject();
        if (tmp == NULL) {
            APP_ERROR("%s: create object failed\r\n", __func__);
            cJSON_Delete(array);
            cJSON_AddNumberToObject(result, "error_code", -1);
            cJSON_AddStringToObject(result, "reason", "create object failed");
            return result;
        }

        cJSON_AddItemToArray(array, tmp);
        cJSON_AddNumberToObject(tmp, "property_id", all_property_list.Proprietary.pList[i]);
    }

    cJSON_AddItemToObject(result, "result", array);

    return result;
}

cJSON *object_get_property_value(BACNET_DEVICE_OBJECT_PROPERTY *property)
{
    BACNET_READ_PROPERTY_DATA rp_data;
    DECLARE_BACNET_BUF(apdu, MAX_APDU);
    char value[MAX_APDU];
    cJSON *result;
    int len;

    if (property == NULL) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return NULL;
    }
    
    result = cJSON_CreateObject();
    if (result == NULL) {
        APP_ERROR("%s: create result object failed\r\n", __func__);
        return NULL;
    }
    
    if (Object_Initialized == false) {
        APP_ERROR("%s: Object is not inited\r\n", __func__);
        cJSON_AddNumberToObject(result, "error_code", -1);
        cJSON_AddStringToObject(result, "reason", "Object is not inited");
        return result;
    }
    
    (void)bacnet_buf_init(&apdu.buf, MAX_APDU);
    rp_data.object_type = property->object_id.type;
    rp_data.object_instance = property->object_id.instance;
    rp_data.property_id = property->property_id;
    rp_data.array_index = property->array_index;
    rp_data.application_data = apdu.buf.data;
    rp_data.application_data_len = MAX_APDU;
    
    len = object_read_property(&rp_data, NULL);
    if (len < 0) {
        APP_ERROR("%s: read property failed(%d)\r\n", __func__, len);
        cJSON_AddNumberToObject(result, "error_code", -1);
        cJSON_AddStringToObject(result, "reason", "read property failed");
        return result;
    }

    if (!bacapp_snprint_value(value, sizeof(value), rp_data.application_data, len)) {
        APP_ERROR("%s: snprint value failed\r\n", __func__);
        cJSON_AddNumberToObject(result, "error_code", -1);
        cJSON_AddStringToObject(result, "reason", "snprint value failed");
        return result;
    }

    cJSON_AddStringToObject(result, "result", value);
    
    return result;
}

static object_init_handler Object_Get_Handlers(BACNET_OBJECT_TYPE object_type)
{
    object_init_handler handler;

    if (!Object_Type_Is_Supported(object_type)) {
        APP_ERROR("%s: Object Type(%d) unsupported\r\n", __func__, object_type);
        return NULL;
    }

    handler = NULL;
    switch (object_type) {
    case OBJECT_ANALOG_INPUT:
        handler = analog_input_init;
        break;

    case OBJECT_ANALOG_OUTPUT:
        handler = analog_output_init;
        break;

    case OBJECT_ANALOG_VALUE:
        handler = analog_value_init;
        break;

    case OBJECT_BINARY_INPUT:
        handler = binary_input_init;
        break;

    case OBJECT_BINARY_OUTPUT:
        handler = binary_output_init;
        break;

    case OBJECT_BINARY_VALUE:
        handler = binary_value_init;
        break;

    case OBJECT_DEVICE:
        handler = device_init;
        break;
    
    case OBJECT_MULTI_STATE_INPUT:
        handler = multistate_input_init;
        break;

    case OBJECT_MULTI_STATE_OUTPUT:
        handler = multistate_output_init;
        break;

    case OBJECT_MULTI_STATE_VALUE:
        handler = multistate_value_init;
        break;

    case OBJECT_TRENDLOG:
        handler = trend_log_init;
        break;

    default:
        APP_ERROR("%s: unknown Object Type(%d)\r\n", __func__, object_type);
        break;
    }

    return handler;
}

int object_init(cJSON *app)
{
    cJSON *object_array, *object, *tmp;
    object_init_handler handler;
    int object_type;
    int i;
    int rv;
    
    if (app == NULL) {
        APP_ERROR("%s: null argument\r\n", __func__);
        return -EINVAL;
    }

    if (Object_Initialized) {
        return OK;
    }

    hash_init(name_table);

    rv = Object_Types_Supported_Init();
    if (rv < 0) {
        APP_ERROR("%s: Object Types Supported Init failed(%d)\r\n", __func__, rv);
        goto out1;
    }

    tmp = cJSON_GetObjectItem(app, "Client_Device");
    if (tmp) {
        if ((tmp->type != cJSON_True) && (tmp->type != cJSON_False)) {
            APP_ERROR("%s: invalid Client_Device item\r\n", __func__);
            goto out1;
        }
        client_device = ((tmp->type == cJSON_True)? true: false);
    }

    if (client_device == true) {
        APP_WARN("%s: This Device is used to be Client Device\r\n", __func__);
        Object_Initialized = true;
        return OK;
    }

    /* Device Object Init */
    handler = Object_Get_Handlers(OBJECT_DEVICE);
    if (!handler) {
        APP_ERROR("%s: get Device Object Init Function failed\r\n", __func__);
        goto out1;
    }

    rv = handler(app);
    if (rv < 0) {
        APP_ERROR("%s: Device Object Init failed(%d)\r\n", __func__, rv);
        goto out1;
    }

    Object_Initialized = true;
    
    object_array = cJSON_GetObjectItem(app, "Object_List");    
    if (object_array) {
        if (object_array->type != cJSON_Array) {
            APP_ERROR("%s: invalid Object_List item type\r\n", __func__);
            goto out2;
        }
    } else {
        object_array = cJSON_CreateArray();
        if (object_array == NULL) {
            APP_ERROR("%s: create object array failed\r\n", __func__);
            goto out2;
        }
        cJSON_AddItemToObject(app, "Object_List", object_array);
    }

    i = 0;
    cJSON_ArrayForEach(object, object_array) {
        if (object->type != cJSON_Object) {
            APP_ERROR("%s: invalid Object_List[%d] item type\r\n", __func__, i);
            goto out2;
        }

        tmp = cJSON_GetObjectItem(object, "Type");
        if ((tmp == NULL) || (tmp->type != cJSON_String)) {
            APP_ERROR("%s: get Object_List[%d] Type item failed\r\n", __func__, i);
            goto out2;
        }

        object_type = bactext_get_object_type_from_name(tmp->valuestring);
        if ((object_type < 0) || (object_type >= MAX_ASHRAE_OBJECT_TYPE)) {
            APP_ERROR("%s: invalid Object_List[%d] Type(%s)\r\n", __func__, i, tmp->valuestring);
            goto out2;
        }

        handler = Object_Get_Handlers(object_type);
        if (!handler) {
            APP_ERROR("%s: get Object_Type(%s) Init Function failed(%d)\r\n", __func__,
                tmp->valuestring, rv);
            goto out2;
        }

        rv = handler(object);
        if (rv < 0) {
            APP_ERROR("%s: Object %s Init failed(%d)\r\n", __func__, tmp->valuestring, rv);
            goto out2;
        }
        i++;
    }

    APP_VERBOS("%s: OK\r\n", __func__);
    return OK;

out2:
    object_exit();
    
out1:

    return -EPERM;
}

void object_exit(void)
{
    object_store_t *store, *store_tmp;
    object_instance_t *object, *object_tmp;

    if (Object_Initialized == false) {
        return;
    }

    rbtree_postorder_for_each_entry_safe(store, store_tmp, &object_root, node) {
        rbtree_postorder_for_each_entry_safe(object, object_tmp, &store->instance_root, node_type) {
            free(object);
        }
        free(store);
    }

    object_root = RB_ROOT;

    Object_Initialized = false;
}

