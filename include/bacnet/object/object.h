/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * object.h
 * Original Author:  linzhixian, 2015-12-24
 *
 * BACnet Object
 *
 * History
 */

#ifndef _OBJECT_H_
#define _OBJECT_H_

#include <stdint.h>
#include <stdbool.h>

#include "bacnet/bacenum.h"
#include "bacnet/bacstr.h"
#include "bacnet/bacapp.h"
#include "bacnet/service/rp.h"
#include "bacnet/service/rr.h"
#include "bacnet/service/wp.h"
#include "bacnet/service/rpm.h"
#include "misc/cJSON.h"
#include "misc/list.h"
#include "misc/rbtree.h"

#ifdef __cplusplus
extern "C"
{
#endif

extern bool client_device;

typedef struct object_instance_s object_instance_t;

typedef struct object_store_s {
    BACNET_OBJECT_TYPE  object_type;
    struct rb_node      node;
    struct rb_root      instance_root; /* object_instance_t */
    uint32_t            object_count;
} object_store_t;

typedef struct property_impl_s {
    BACNET_PROPERTY_ID object_property;
    int (*write_property)(object_instance_t *object, BACNET_WRITE_PROPERTY_DATA *wp_data);
    int (*read_property)(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range);
} property_impl_t;

typedef enum {
    PROPERTY_TYPE_REQUIRED = 0,
    PROPERTY_TYPE_OPTIONAL,
    PROPERTY_TYPE_PROPRIETARY,
    PROPERTY_TYPE_MAX,
} property_type_t;

typedef struct object_impl_s {
    BACNET_OBJECT_TYPE  type;
    property_impl_t     *all_property;          /* property_function_t */
    BACNET_PROPERTY_ID  *property_required;
    BACNET_PROPERTY_ID  *property_optional;
    BACNET_PROPERTY_ID  *property_proprietary;
    uint16_t            property_required_count;
    uint16_t            property_optional_count;
    uint16_t            property_proprietary_count;
} object_impl_t;

#define OBJECT_NAME_MAX_LEN     (32)

struct object_instance_s {
    struct rb_node      node_type;
    struct hlist_node   node_name;
    const object_impl_t *type;
    uint32_t            instance;
    DECLARE_VBUF(object_name, OBJECT_NAME_MAX_LEN);
};

extern object_instance_t *object_find(BACNET_OBJECT_TYPE type, uint32_t instance);
extern bool object_add(object_instance_t *object);
extern void object_detach(object_instance_t *object);

extern const property_impl_t *object_impl_find_property(const object_impl_t *type,
        BACNET_PROPERTY_ID property);

extern object_impl_t *object_impl_clone(const object_impl_t *type);

extern void object_impl_destroy(object_impl_t *type);

extern property_impl_t *object_impl_extend(object_impl_t *type,
        BACNET_PROPERTY_ID object_property, property_type_t property_type);

extern object_impl_t* object_create_impl_base(void);

/* object with Status_Flags/Event_State/Out_of_Service/Reliability */
typedef struct object_seor_s {
    object_instance_t   base;
    BACNET_RELIABILITY  Reliability : 24;
    BACNET_EVENT_STATE  Event_State : 4;
    bool                Out_Of_Service : 1;
    bool                Overridden : 1;
    /* used to notify present_value need to output
     * or to be updated from input, other as reliability */
    void (*notify) (struct object_seor_s *obj);
} object_seor_t;

extern object_impl_t *object_create_impl_seor(bool has_event_state, bool has_out_of_service, bool reliability_optional);

typedef int (*object_init_handler)(cJSON *cfg);

extern uint32_t object_list_count(void);

extern bool object_find_index(uint32_t index, object_store_t **store, object_instance_t **object);

extern bool object_find_next(object_store_t **store, object_instance_t **object);

/** find object by index within same type.
 * @return true if found
 */
extern bool object_type_find_object_index(BACNET_OBJECT_TYPE type,
        uint32_t index, object_instance_t **object);

/**  find next object with same type.
 * @param object, input/output
 * @return true if found.
 */
extern bool object_type_find_object_next(object_instance_t **object);

extern bool object_get_name(BACNET_OBJECT_TYPE type, uint32_t instance,
                BACNET_CHARACTER_STRING *name);

extern bool object_find_name(BACNET_CHARACTER_STRING *name, BACNET_OBJECT_TYPE *type,
                uint32_t *instance);

/**
 * rename object name
 * @return MAX_ERROR_CODE = success, other is error code
 */
extern BACNET_ERROR_CODE object_rename(object_instance_t *object, BACNET_CHARACTER_STRING *new_name);

extern bool object_property_lists(BACNET_OBJECT_TYPE object_type, uint32_t instance,
                special_property_list_t *pPropertyList);

extern int object_read_property(BACNET_READ_PROPERTY_DATA *rp_data, RR_RANGE *range);

extern int object_write_property(BACNET_WRITE_PROPERTY_DATA *wp_data);

extern cJSON *object_get_object_list(void);

extern cJSON *object_get_property_list(BACNET_OBJECT_TYPE object_type, uint32_t instance);

extern cJSON *object_get_property_value(BACNET_DEVICE_OBJECT_PROPERTY *property);

extern int object_init(cJSON *app);

extern void object_exit(void);

extern bool object_get_types_supported(BACNET_BIT_STRING *types);

#ifdef __cplusplus
}
#endif

#endif /* _OBJECT_H_ */

