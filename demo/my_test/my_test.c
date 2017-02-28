/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * my_test.c
 * Original Author:  linzhixian, 2016-3-31
 *
 * My Test
 *
 * History
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/prctl.h>

#include "my_test.h"
#include "bacnet/bacnet.h"
#include "bacnet/tsm.h"
#include "bacnet/apdu.h"
#include "bacnet/config.h"
#include "bacnet/bacapp.h"
#include "bacnet/bactext.h"
#include "bacnet/addressbind.h"
#include "bacnet/service/rpm.h"
#include "bacnet/service/error.h"
#include "bacnet/bip.h"
#include "bacnet/network.h"
#include "bacnet/app.h"
#include "connect_mng.h"
#include "web_service.h"
#include "debug.h"
#include "misc/utils.h"

static pthread_t my_thread;
static my_object_list_t my_object_list;

static int __object_hash(watch_object_t *object)
{
    uint8_t *start, *end;
    int code;

    code = 0;
    start = (uint8_t *)&(object->device_id);
    end = (uint8_t *)&(object->instance);

    do {
        code = ROTATE_LEFT(code, 8);
        code += *start;
    } while (++start < end);

    return code;
}

static int my_watch_object_list_add(my_object_t *object, watch_object_t *watch_node)
{
    RWLOCK_WRLOCK(&object->rwlock);

    list_add(&watch_node->list, &object->watch_head);
    hash_add(object->watch_object_table, &watch_node->hlist, __object_hash(watch_node));
    object->watch_object_count++;

    RWLOCK_UNLOCK(&object->rwlock);

    return OK;
}

static watch_object_t *my_watch_object_list_search(my_object_t *object, uint32_t device_id,
                        BACNET_OBJECT_TYPE type, uint32_t instance)
{
    watch_object_t *watch_node;
    watch_object_t tmp;
    int key;

    tmp.device_id = device_id;
    tmp.type = type;
    tmp.instance = instance;

    key = __object_hash(&tmp);

    RWLOCK_RDLOCK(&object->rwlock);
    
    hash_for_each_possible(object->watch_object_table, watch_node, hlist, key) {
        if ((watch_node->device_id == device_id) && (watch_node->type == type)
                && (watch_node->instance == instance)) {
            RWLOCK_UNLOCK(&object->rwlock);
            return watch_node;
        }
    }

    RWLOCK_UNLOCK(&object->rwlock);
    
    return NULL;   
}

static void my_watch_object_list_destroy(my_object_t *object)
{
    watch_object_t *watch_node;
    struct hlist_node *tmp_watch;
    int bkt;
    
    RWLOCK_WRLOCK(&object->rwlock);
    
    hash_for_each_safe(object->watch_object_table, bkt, watch_node, tmp_watch, hlist) {
        hash_del(&watch_node->hlist);
        list_del(&watch_node->list);
        free(watch_node);
    }
    
    object->watch_object_count = 0;

    RWLOCK_UNLOCK(&object->rwlock);

    (void)pthread_rwlock_destroy(&object->rwlock);

    return;
}

static void my_av_instance_delete(object_av_t *av)
{
    object_instance_t *instance;
    
    instance = object_find(OBJECT_ANALOG_VALUE, av->base.base.instance);
    if (instance == NULL) {
        printf("%s: object find failed\r\n", __func__);
        return;
    }
    
    object_detach(instance);
    free(instance);

    return;
}

static object_av_t *my_av_instance_create(cJSON *object, uint32_t instance, object_impl_t *type)
{
    object_av_writable_t *av_writable;
    object_av_commandable_t *av_commandable;
    object_av_t *av;
    cJSON *tmp;
    bool writable;
    bool commandable;
    
    writable = false;
    commandable = false;

    tmp = cJSON_GetObjectItem(object, "Writable");
    if (tmp != NULL) {
        if ((tmp->type != cJSON_False) && (tmp->type != cJSON_True)) {
            printf("%s: get Writable not boolean\r\n", __func__);
            return NULL;
        }
        writable = (tmp->type == cJSON_True)? true: false;
    }

    tmp = cJSON_GetObjectItem(object, "Commandable");
    if (tmp != NULL) {
        if ((tmp->type != cJSON_False) && (tmp->type != cJSON_True)) {
            printf("%s: get Commandable not boolean\r\n", __func__);
            return NULL;
        }
        commandable = (tmp->type == cJSON_True)? true: false;
    }
    
    if (writable && commandable) {
        printf("%s: get Writable/Commandable both true\r\n", __func__);
        return NULL;
    }

    av = NULL;
    if (writable) {
        av_writable = (object_av_writable_t *)malloc(sizeof(object_av_writable_t));
        if (!av_writable) {
            printf("%s: not enough memory\r\n", __func__);
            return NULL;
        }
        
        memset(av_writable, 0, sizeof(*av_writable));
        av = av_writable;
        av->present = 0.0;
    } else if (commandable) {
        tmp = cJSON_GetObjectItem(object, "Relinquish_Default");
        if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
            printf("%s: get Relinquish_Default item failed\r\n", __func__);
            return NULL;
        }

        av_commandable = (object_av_commandable_t *)malloc(sizeof(object_av_commandable_t));
        if (!av_commandable) {
            printf("%s: not enough memory\r\n", __func__);
            return NULL;
        }

        memset(av_commandable, 0, sizeof(*av_commandable));
        av_commandable->relinquish_default = tmp->valuedouble;
        av_commandable->active_bit = BACNET_MAX_PRIORITY;
        av = &av_commandable->base;
        av->present = tmp->valuedouble;
    } else {
        av = (object_av_t *)malloc(sizeof(object_av_t));
        if (!av) {
            printf("%s: not enough memory\r\n", __func__);
            return NULL;
        }
        
        memset(av, 0, sizeof(*av));
        av->present = 0.0;
    }

    tmp = cJSON_GetObjectItem(object, "Name");
    if ((tmp == NULL) || (tmp->type != cJSON_String)) {
        printf("%s: get Name item failed\r\n", __func__);
        free(av);
        return NULL;
    }

    if (!vbuf_fr_str(&av->base.base.object_name.vbuf, tmp->valuestring, OBJECT_NAME_MAX_LEN)) {
        printf("%s: set object name overflow\r\n", __func__);
        free(av);
        return NULL;
    }

    tmp = cJSON_GetObjectItem(object, "Out_Of_Service");
    if ((tmp == NULL) || ((tmp->type != cJSON_False) && (tmp->type != cJSON_True))) {
        printf("%s: get Out_Of_Service item failed\r\n", __func__);
        free(av);
        return NULL;
    }
    av->base.Out_Of_Service = (tmp->type == cJSON_True)? true: false;
    
    tmp = cJSON_GetObjectItem(object, "Units");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        printf("%s: get Units item failed\r\n", __func__);
        free(av);
        return NULL;
    }
    av->units = (BACNET_ENGINEERING_UNITS)(uint32_t)tmp->valueint;

    av->base.base.instance = instance;
    av->base.base.type = (object_impl_t *)type;

    if (!object_add(&av->base.base)) {
        printf("%s: object add failed\r\n", __func__);
        free(av);
        return NULL;
    }

    return av;
}

static void my_object_list_destroy(void)
{
    my_object_t *object, *tmp_object;

    RWLOCK_WRLOCK(&my_object_list.rwlock);

    list_for_each_entry_safe(object, tmp_object, &my_object_list.head, list) {
        list_del(&object->list);
        el_timer_destroy(&el_default_loop, object->timer);
        my_watch_object_list_destroy(object);
        my_av_instance_delete(object->av);
        free(object);
    }

    my_object_list.count = 0;

    RWLOCK_UNLOCK(&my_object_list.rwlock);

    (void)pthread_rwlock_destroy(&my_object_list.rwlock);

    return;
}

static void my_complex_ack_handler(my_object_t *object, uint32_t src_device_id, bacnet_buf_t *apdu)
{
    BACNET_CONFIRMED_SERVICE_ACK_DATA service_ack_data;
    BACNET_RPM_ACK_DECODER decoder;
    BACNET_READ_PROPERTY_DATA rp_data;
    BACNET_APPLICATION_DATA_VALUE value;
    watch_object_t *watch_object;
    int rv;

    rv = apdu_decode_complex_ack(apdu, &service_ack_data);
    if (rv < 0) {
        printf("%s: decode ack header failed(%d)\r\n", __func__, rv);
        return;
    }

    if (service_ack_data.service_choice != SERVICE_CONFIRMED_READ_PROP_MULTIPLE) {
        printf("%s: invalid service choice(%d)\r\n", __func__, service_ack_data.service_choice);
        return;
    }

    rpm_ack_decode_init(&decoder, service_ack_data.service_data, service_ack_data.service_data_len);
    for (;;) {
        rv = rpm_ack_decode_object(&decoder, &rp_data);
        if (rv < 0) {
            printf("%s: decode object_id failed\r\n", __func__);
            goto out;
        } else if (rv == 0) {
            break;
        }

        watch_object = my_watch_object_list_search(object, src_device_id, rp_data.object_type,
            rp_data.object_instance);
        if (watch_object == NULL) {
            printf("%s: object(%d, %d, %d) is not present\r\n", __func__, src_device_id,
                rp_data.object_type, rp_data.object_instance);
            goto out;
        }
        
        for (;;) {
            rv = rpm_ack_decode_property(&decoder, &rp_data);
            if (rv < 0) {
                printf("%s: decode property failed(%d)\r\n", __func__, rv);
                goto out;
            } else if (rv == 0) {
                break;
            }

            if ((rp_data.property_id != PROP_PRESENT_VALUE)
                    && (rp_data.property_id != PROP_STATUS_FLAGS)) {
                printf("%s: unexpected property(%d)\r\n", __func__, rp_data.property_id);
                continue;
            }

            if (rp_data.array_index != BACNET_ARRAY_ALL) {
                printf("%s: invalid property index(%d)\r\n", __func__, rp_data.array_index);
                continue;
            }

            if (rp_data.application_data == NULL) {
                printf("%s: BACnet Error: %s: %s\r\n", __func__,
                    bactext_error_class_name((int)rp_data.error_class),
                    bactext_error_code_name((int)rp_data.error_code));
                continue;
            }

            rv = bacapp_decode_application_data(rp_data.application_data,
                    rp_data.application_data_len, &value);
            if (rv < 0) {
                printf("%s: decode property(%d) failed(%d)\r\n", __func__, rp_data.property_id, rv);
                goto out;
            }

            if (rp_data.property_id == PROP_PRESENT_VALUE) {
                if (value.tag != BACNET_APPLICATION_TAG_REAL) {
                    printf("%s: invalid tag(%d)\r\n", __func__, value.tag);
                    goto out;
                }
                watch_object->present_value = value.type.Real;
                printf("%s: device_id(%d) object_type(%d) instance(%d) present_value(%f)\r\n", 
                    __func__, src_device_id, rp_data.object_type, rp_data.object_instance,
                    watch_object->present_value);
            } else {
                if (value.tag != BACNET_APPLICATION_TAG_BIT_STRING) {
                    printf("%s: invalid tag(%d)\r\n", __func__, value.tag);
                    goto out;
                }

                if ((value.type.Bit_String.byte_len != 1)
                        || (value.type.Bit_String.last_byte_bits_unused != 4)) {
                    printf("%s: invalid Bit_String\r\n", __func__);
                    goto out;
                }
                watch_object->status_flag_fault = bitstring_get_bit(&value.type.Bit_String,
                    STATUS_FLAG_FAULT);
                printf("%s: device_id(%d) object_type(%d) instance(%d) status_flag_fault(%d)\r\n", 
                    __func__, src_device_id, rp_data.object_type, rp_data.object_instance,
                    watch_object->status_flag_fault);
            }
            
            watch_object->update_time = el_current_second();
        }
    }

out:

    return;
}

static void my_error_handler(my_object_t *object, uint32_t src_device_id, bacnet_buf_t *apdu)
{
    BACNET_CONFIRMED_SERVICE service_choice;
    BACNET_ERROR_CLASS error_class;
    BACNET_ERROR_CODE error_code;
    int len;
    
    if (apdu->data_len < 4) {
        printf("%s: invalid error_pdu len(%d)\r\n", __func__, apdu->data_len);
        return;
    }

    len = bacerror_decode_apdu(apdu, NULL, &service_choice, &error_class, &error_code);
    if (len < 0) {
        printf("%s: decode apdu failed(%d)\r\n", __func__, len);
        return;
    }

    printf("%s: service_choice(%d)\r\n", __func__, service_choice);
    printf("%s: error_class(%d)\r\n", __func__, error_class);
    printf("%s: error_code(%d)\r\n", __func__, error_code);
    
    return;
}

static void my_reject_handler(my_object_t *object, uint32_t src_device_id, bacnet_buf_t *apdu)
{
    uint8_t *pdu;
    uint8_t reason;

    if (apdu->data_len != 3) {
        printf("%s: invalid reject_pdu len(%d)\r\n", __func__, apdu->data_len);
        return;
    }
    
    pdu = apdu->data;
    reason = pdu[2];

    printf("%s: reason(%d)\r\n", __func__, reason);

    return;
}

static void my_abort_handler(my_object_t *object, uint32_t src_device_id, bacnet_buf_t *apdu)
{
    uint8_t *pdu;
    uint8_t reason;

    if (apdu->data_len != 3) {
        printf("%s: invalid abort_pdu len(%d)\r\n", __func__, apdu->data_len);
        return;
    }

    pdu = apdu->data;
    reason = pdu[2];

    printf("%s: reason(%d)\r\n", __func__, reason);
    
    return;
}

static void my_confirmed_ack_handler(tsm_invoker_t *invoker, bacnet_buf_t *apdu,
                BACNET_PDU_TYPE apdu_type)
{
    my_object_t *object;
    uint32_t src_device_id;

    if (invoker == NULL) {
        printf("%s: null invoker\r\n", __func__);
        return;
    }

    if (invoker->choice != SERVICE_CONFIRMED_READ_PROP_MULTIPLE) {
        printf("%s: invalid service choice(%d)\r\n", __func__, invoker->choice);
        return;
    }
    
    if ((apdu == NULL) || (apdu->data == NULL)) {
        printf("%s: invalid argument\r\n", __func__);
        goto out;
    }

    if (!query_device_from_address(&invoker->addr, NULL, &src_device_id)) {
        printf("%s: get src_device_id from src_address failed\r\n", __func__);
        goto out;
    }
    
    object = (my_object_t *)invoker->data;
    if (object == NULL) {
        printf("%s: null object\r\n", __func__);
        goto out;
    }
    
    switch (apdu_type) {
    case PDU_TYPE_SIMPLE_ACK:
        printf("%s: invalid apdu type(%d)\r\n", __func__, apdu_type);
        break;
        
    case PDU_TYPE_COMPLEX_ACK:
        my_complex_ack_handler(object, src_device_id, apdu);
        break;
        
    case PDU_TYPE_ERROR:
        my_error_handler(object, src_device_id, apdu);
        break;
        
    case PDU_TYPE_REJECT:
        my_reject_handler(object, src_device_id, apdu);
        break;
        
    case PDU_TYPE_ABORT:
        my_abort_handler(object, src_device_id, apdu);
        break;
        
    default:
        printf("%s: unknown apdu type(%d)\r\n", __func__, apdu_type);
        break;
    }

out:
    tsm_free_invokeID(invoker);

    return;
}

static void *my_thread_handler(void *arg)
{
    my_object_t *object;
    watch_object_t *watch_node;
    struct hlist_node *tmp_node;
    int bkt;
    float value, tmp;
    bool first;
    uint32_t current_time;
    int rv;
    
    prctl(PR_SET_NAME, "my_pthr");

    rv = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    if (rv != 0) {
        printf("%s: setcancelstate failed cause %s\r\n", __func__, strerror(rv));
        goto exit;
    }

    rv = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    if (rv != 0) {
        printf("%s: setcanceltype failed cause %s\r\n", __func__, strerror(rv));
        goto exit;
    }

    rv = pthread_detach(pthread_self());
    if (rv != 0) {
        printf("%s: pthread_detach failed cause %s\r\n", __func__, strerror(rv));
        goto exit;
    }

    while (1) {
        sleep(20);
        RWLOCK_WRLOCK(&my_object_list.rwlock);
    
        list_for_each_entry(object, &my_object_list.head, list) {
            first = true;
            value = 0.0f;
            current_time = el_current_second();

            RWLOCK_RDLOCK(&object->rwlock);

            hash_for_each_safe(object->watch_object_table, bkt, watch_node, tmp_node, hlist) {
                if (watch_node->status_flag_fault) {
                    continue;
                }

                if (current_time - watch_node->update_time > MY_MAX_ACK_INTERVAL) {
                    continue;
                }
        
                tmp = watch_node->present_value;
                if (first) {
                    value = tmp;
                    first = false;
                    continue;
                }

                if (tmp < 0.0f) {
                    if (value < 0.0f) {
                        value += tmp;
                    } else {
                        value = tmp;
                    }
                } else {
                    if (tmp < value) {
                        value = tmp;
                    }
                }
            }

            RWLOCK_UNLOCK(&object->rwlock);

            if (first) {
                object->av->present = 0.0f;
                object->av->base.Reliability = RELIABILITY_UNRELIABLE_OTHER;
            } else {
                object->av->present = value;
                object->av->base.Reliability = RELIABILITY_NO_FAULT_DETECTED;
            }

            printf("%s: status_flag_fault(%s), present_value(%f)\r\n", __func__,
                object->av->base.Reliability != RELIABILITY_NO_FAULT_DETECTED ? "FAULT": "NO_FAULT",
                object->av->present);
        }

        RWLOCK_UNLOCK(&my_object_list.rwlock);
    }

exit:

    pthread_exit(NULL);
}

static void my_timer_handler(el_timer_t *timer)
{
    DECLARE_BACNET_BUF(tx_apdu, MAX_APDU);
    my_object_t *object;
    watch_object_t *prev_watch, *next_watch;
    bacnet_addr_t dst_addr;
    tsm_invoker_t *invoker;
    int rv;

    object = (my_object_t *)timer->data;

    RWLOCK_WRLOCK(&object->rwlock);
    
    prev_watch = object->last_tx_watch;
    if ((prev_watch == NULL) || (prev_watch->list.next == &object->watch_head)) {
        next_watch = list_first_entry_or_null(&object->watch_head, watch_object_t, list);
    } else {
        next_watch = list_next_entry(prev_watch, list);
    }

    object->last_tx_watch = next_watch;
    if (next_watch != NULL) {
        if (!query_address_from_device(next_watch->device_id, NULL, &dst_addr)) {
            printf("%s: get address from device(%d) failed\r\n", __func__, next_watch->device_id);
            goto out;
        }

        invoker = tsm_alloc_invokeID(&dst_addr, SERVICE_CONFIRMED_READ_PROP_MULTIPLE,
            my_confirmed_ack_handler, (void *)object);
        if (invoker == NULL) {
            printf("%s: alloc invokeID failed\r\n", __func__);
            goto out;
        }
        
        (void)bacnet_buf_init(&tx_apdu.buf, MAX_APDU);
        (void)rpm_req_encode_object(&tx_apdu.buf, next_watch->type, next_watch->instance);
        (void)rpm_req_encode_property(&tx_apdu.buf, PROP_PRESENT_VALUE, BACNET_ARRAY_ALL);
        (void)rpm_req_encode_property(&tx_apdu.buf, PROP_STATUS_FLAGS, BACNET_ARRAY_ALL);
        (void)rpm_req_encode_end(&tx_apdu.buf, invoker->invokeID);

        rv = tsm_send_apdu(invoker, &tx_apdu.buf, PRIORITY_NORMAL, 0);
        if (rv < 0) {
            printf("%s: send RPM request to device(%d) failed(%d)\r\n", __func__,
                next_watch->device_id, rv);
            tsm_free_invokeID(invoker);
        }
    }

out:
    el_timer_mod(&el_default_loop, object->timer, 5000);

    RWLOCK_UNLOCK(&object->rwlock);

    return;
}

static int my_object_list_init(void)
{
    cJSON *cfg, *object_list, *object, *watch_object_list, *watch_object, *tmp;
    my_object_t *object_node;
    watch_object_t *watch_object_node;
    object_impl_t *av_type;
    int i, j;
    int rv;

    my_object_list.count = 0;
    INIT_LIST_HEAD(&my_object_list.head);
    rv = pthread_rwlock_init(&my_object_list.rwlock, NULL);
    if (rv) {
        printf("%s: init my_object_list rwlock failed(%d)\r\n", __func__, rv);
        goto out0;
    }
    
    cfg = load_json_file("my.conf");
    if (cfg == NULL) {
        printf("%s: load cfg file failed\r\n", __func__);
        goto out1;
    }

    if (cfg->type != cJSON_Object) {
        printf("%s: is not a cJSON Object\r\n", __func__);
        goto out2;
    }

    object_list = cJSON_GetObjectItem(cfg, "Instance_List");
    if ((object_list == NULL) || (object_list->type != cJSON_Array)) {
        printf("%s: get Instance_List item failed\r\n", __func__);
        goto out2;
    }

    av_type = NULL;
    
    RWLOCK_WRLOCK(&my_object_list.rwlock);

    i = 0;
    cJSON_ArrayForEach(object, object_list) {
        if (object->type != cJSON_Object) {
            printf("%s: invalid object[%d] item type\r\n", __func__, i);
            goto out3;
        }

        watch_object_list = cJSON_GetObjectItem(object, "watch_object_list");
        if (!watch_object_list || watch_object_list->type != cJSON_Array) {
            printf("%s: get watch_object_list item failed\r\n", __func__);
            goto out3;
        }

        object_node = (my_object_t *)malloc(sizeof(my_object_t));
        if (object_node == NULL) {
            printf("%s: malloc object node failed\r\n", __func__);
            goto out3;
        }

        if (av_type == NULL) {
            av_type = object_create_impl_av();
            if (av_type == NULL) {
                printf("%s: create av type failed\r\n", __func__);
                free(object_node);
                goto out3;
            }
        }

        object_node->av = my_av_instance_create(object, i, av_type);
        if (object_node->av == NULL) {
            printf("%s: create av instance(%d) failed\r\n", __func__, i);
            free(object_node);
            goto out3;
        }
        
        rv = pthread_rwlock_init(&object_node->rwlock, NULL);
        if (rv) {
            printf("%s: init object_node rwlock failed(%d)\r\n", __func__, rv);
            free(object_node);
            goto out3;
        }

        object_node->timer = el_timer_create(&el_default_loop, 20 * 1000);
        if (object_node->timer == NULL) {
            printf("%s: create timer failed\r\n", __func__);
            free(object_node);
            goto out3;
        }
        object_node->timer->data = (void *)object_node;
        object_node->timer->handler = my_timer_handler;

        object_node->last_tx_watch = NULL;
        object_node->watch_object_count = 0;
        INIT_LIST_HEAD(&object_node->watch_head);
        hash_init(object_node->watch_object_table);
        list_add(&object_node->list, &my_object_list.head);

        j = 0;
        cJSON_ArrayForEach(watch_object, watch_object_list) {
            if (watch_object->type != cJSON_Object) {
                printf("%s: invalid watch_object[%d] item type\r\n", __func__, j);
                goto out3;
            }

            watch_object_node = (watch_object_t *)malloc(sizeof(watch_object_t));
            if (watch_object_node == NULL) {
                printf("%s: malloc watch_object_node failed\r\n", __func__);
                goto out3;
            }

            tmp = cJSON_GetObjectItem(watch_object, "device_id");
            if (!tmp || tmp->type != cJSON_Number) {
                printf("%s: get watch_object[%d] device_id item failed\r\n", __func__, j);
                free(watch_object_node);
                goto out3;
            }
            if ((tmp->valueint < 0) || (tmp->valueint >= BACNET_MAX_INSTANCE)) {
                printf("%s: invalid watch_object[%d] device_id(%d)\r\n", __func__, j, tmp->valueint);
                free(watch_object_node);
                goto out3;
            }
            watch_object_node->device_id = (uint32_t)tmp->valueint;

            tmp = cJSON_GetObjectItem(watch_object, "object_type");
            if (!tmp || tmp->type != cJSON_Number) {
                printf("%s: get watch_object[%d] object_type item failed\r\n", __func__, j);
                free(watch_object_node);
                goto out3;
            }
            if ((tmp->valueint < 0) || (tmp->valueint >= MAX_BACNET_OBJECT_TYPE)) {
                printf("%s: invalid watch_object[%d] object_type(%d)\r\n", __func__, j,
                    tmp->valueint);
                free(watch_object_node);
                goto out3;
            }
            watch_object_node->type = (BACNET_OBJECT_TYPE)tmp->valueint;

            tmp = cJSON_GetObjectItem(watch_object, "object_instance");
            if (!tmp || tmp->type != cJSON_Number) {
                printf("%s: get watch_object[%d] object_instance item failed\r\n", __func__, j);
                free(watch_object_node);
                goto out3;
            }
            if ((tmp->valueint < 0) || (tmp->valueint >= BACNET_MAX_INSTANCE)) {
                printf("%s: invalid watch_object[%d] object_instance(%d)\r\n", __func__, j,
                    tmp->valueint);
                free(watch_object_node);
                goto out3;
            }
            watch_object_node->instance = (uint32_t)tmp->valueint;

            watch_object_node->status_flag_fault = true;
            watch_object_node->present_value = 0.0f;
            watch_object_node->update_time = el_current_second();
            (void)my_watch_object_list_add(object_node, watch_object_node);
            j++;
        }
        i++;
    }

    RWLOCK_UNLOCK(&my_object_list.rwlock);
    
    return OK;

out3:
    RWLOCK_UNLOCK(&my_object_list.rwlock);

out2:
    cJSON_Delete(cfg);

out1:
    my_object_list_destroy();

out0:

    return -EPERM;
}

int main(int argc, char *argv[])
{
    int rv;

    app_set_dbg_level(6);
    network_set_dbg_level(6);
    datalink_set_dbg_level(6);
    bip_set_dbg_level(6);
    
    rv = bacnet_init();
    if (rv < 0) {
        printf("bacnet init failed(%d)\r\n", rv);
        return rv;
    }
    
    rv = connect_mng_init();
    if (rv < 0) {
        printf("connect_mng init failed(%d)\r\n", rv);
        goto out0;
    }

    rv = debug_service_init();
    if (rv < 0) {
        printf("debug service init failed(%d)\r\n", rv);
        goto out1;
    }

#if 1
    /* optional */
    rv = web_service_init();
    if (rv < 0) {
        printf("web service init failed(%d)\r\n", rv);
        goto out2;
    }
#endif

    apdu_set_default_service_handler();

    rv = my_object_list_init();
    if (rv < 0) {
        printf("get watch object list failed(%d)\r\n", rv);
        goto out2;
    }
    
    rv = pthread_create(&my_thread, NULL, my_thread_handler, NULL);
    if (rv != 0) {
        printf("create thread failed cause %s\r\n", strerror(rv));
        goto out3;
    }

    rv = el_loop_start(&el_default_loop);
    if (rv < 0) {
        printf("el loop start failed(%d)\r\n", rv);
        goto out4;
    }

    while (1) {
        sleep(10);
    }

out4:
    (void)pthread_cancel(my_thread);

out3:
    my_object_list_destroy();

out2:
    debug_service_exit();

out1:
    connect_mng_exit();

out0:
    bacnet_exit();
    
    return -EPERM;
}

