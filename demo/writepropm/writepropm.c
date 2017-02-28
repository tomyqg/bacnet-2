/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * writepropm.c
 * Original Author:  linzhixian, 2015-3-17
 *
 * Write Property Multiple Demo
 *
 * History
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "bacnet/bacenum.h"
#include "bacnet/tsm.h"
#include "bacnet/service/wpm.h"
#include "bacnet/bacnet.h"
#include "bacnet/service/error.h"
#include "bacnet/addressbind.h"
#include "bacnet/apdu.h"
#include "bacnet/config.h"
#include "misc/eventloop.h"

static void my_simple_ack_handler(bacnet_addr_t *src, bacnet_buf_t *apdu)
{
    uint8_t *pdu;
    uint8_t service_choice;
    
    pdu = apdu->data;
    service_choice = pdu[2];
    
    if (apdu->data_len != 3) {
        printf("%s: invalid pdu_len(%d)\r\n", __func__, apdu->data_len);
        return;
    }

    switch (service_choice) {
    case SERVICE_CONFIRMED_ACKNOWLEDGE_ALARM:
    case SERVICE_CONFIRMED_COV_NOTIFICATION:
    case SERVICE_CONFIRMED_EVENT_NOTIFICATION:
    case SERVICE_CONFIRMED_SUBSCRIBE_COV:
    case SERVICE_CONFIRMED_SUBSCRIBE_COV_PROPERTY:
    case SERVICE_CONFIRMED_LIFE_SAFETY_OPERATION:
    /* Object Access Services */
    case SERVICE_CONFIRMED_ADD_LIST_ELEMENT:
    case SERVICE_CONFIRMED_REMOVE_LIST_ELEMENT:
    case SERVICE_CONFIRMED_DELETE_OBJECT:
    case SERVICE_CONFIRMED_WRITE_PROPERTY:
    case SERVICE_CONFIRMED_WRITE_PROP_MULTIPLE:
    /* Remote Device Management Services */
    case SERVICE_CONFIRMED_DEVICE_COMMUNICATION_CONTROL:
    case SERVICE_CONFIRMED_REINITIALIZE_DEVICE:
    case SERVICE_CONFIRMED_TEXT_MESSAGE:
    /* Virtual Terminal Services */
    case SERVICE_CONFIRMED_VT_CLOSE:
    /* Security Services */
    case SERVICE_CONFIRMED_REQUEST_KEY:
        printf("%s: receive service choice(%d) ack ok\r\n", __func__, service_choice);
        break;
        
    default:
        printf("%s: invalid service_choice(%d)\r\n", __func__, service_choice);
        break;
    }

    return;
}

static void my_complex_ack_handler(bacnet_addr_t *src, bacnet_buf_t *apdu)
{
    printf("%s: invalid pdu for write_property multiple\r\n", __func__);
}

static void my_error_handler(bacnet_addr_t *src, bacnet_buf_t *apdu)
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

static void my_reject_handler(bacnet_addr_t *src, bacnet_buf_t *apdu)
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

static void my_abort_handler(bacnet_addr_t *src, bacnet_buf_t *apdu)
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

static void confirmed_ack_handler(tsm_invoker_t *invoker, bacnet_buf_t *apdu,
        BACNET_PDU_TYPE apdu_type)
{    
    if ((invoker == NULL) || (apdu == NULL) || (apdu->data == NULL)) {
        printf("%s: invalid argument\r\n", __func__);
        return;
    }

    if (invoker->choice != SERVICE_CONFIRMED_WRITE_PROP_MULTIPLE) {
        printf("%s: invalid service choice(%d)\r\n", __func__, invoker->choice);
        return;
    }
    
    switch (apdu_type) {
    case PDU_TYPE_SIMPLE_ACK:
        my_simple_ack_handler(&invoker->addr, apdu);
        break;
        
    case PDU_TYPE_COMPLEX_ACK:
        my_complex_ack_handler(&invoker->addr, apdu);
        break;
        
    case PDU_TYPE_ERROR:
        my_error_handler(&invoker->addr, apdu);
        break;
        
    case PDU_TYPE_REJECT:
        my_reject_handler(&invoker->addr, apdu);
        break;
        
    case PDU_TYPE_ABORT:
        my_abort_handler(&invoker->addr, apdu);
        break;
        
    default:
        printf("%s: unknown apdu type(%d)\r\n", __func__, apdu_type);
        break;
    }

    return;
}

/* ./writepropm & */
/* ./writepropm Device_Id Type Instance Property[Index] Priority [CTag] Tag Value & */
int main(int argc, char *argv[])
{
    uint32_t target_device_id = BACNET_MAX_INSTANCE;
    BACNET_OBJECT_TYPE object_type;
    uint32_t object_instance;
    BACNET_PROPERTY_ID property_id;
    uint32_t array_index;
    uint8_t priority;
    DECLARE_BACNET_BUF(apdu, MAX_APDU);
    bool status;
    int args_remaining;
    int tag_value_arg;
    int scan_count;
    int count;
    int rv;

    if (argc == 1) {
        rv = bacnet_init();
        if (rv < 0) {
            printf("bacnet init failed(%d)\r\n", rv);
            return rv;
        }

        apdu_set_default_service_handler();
        rv = el_loop_start(&el_default_loop);
        if (rv < 0) {
            printf("el loop start failed(%d)\r\n", rv);
            goto out0;
        }
        
        goto out1;
    } else if ((argc == 2) && ((strcmp(argv[1], "--help") == 0))) {
        printf("\r\n[Usage]:\r\n"
            "%s Device_Instance Type Instance Property[Index] Priority Value \r\n"
            "                                        [Property[Index] Priority Value] \r\n"
            "                          [, Type Instance Property[Index] Priority Value \r\n"
            "                                        [Property[Index] Priority Value] ] & \r\n", argv[0]);

        printf("\r\n[Example]:\r\n"
        "If you want to write multiple properties in one Object,\r\n"
        "you could use the following command:\r\n"
        "%s 10 1 0 81 1 true 85 1 f100.0 &\r\n\r\n"
        "If you want to write multiple properties in multiple Objects,\r\n"
        "you could use the following command:\r\n"
        "%s 10 1 0 81 1 true 85 1 f100.0 , 1 1 81 1 true 85 1 f8.0 & \r\n\r\n", argv[0], argv[0]);
        return OK;
    } else if (argc < 7) {
        printf("invalid argc(%d)\r\n", argc);
        return -EINVAL;
    }

    target_device_id = strtol(argv[1], NULL, 0);
    if (target_device_id >= BACNET_MAX_INSTANCE) {
        printf("device-instance=%u - it must be less than %u\r\n", target_device_id, 
            BACNET_MAX_INSTANCE);
        return -EINVAL;
    }
    
    tag_value_arg = 2;
    args_remaining = (argc - tag_value_arg);
    bacnet_buf_init(&apdu.buf, MAX_APDU);
    
    while (1) {
        /* Object_Type */
        object_type = strtol(argv[tag_value_arg], NULL, 0);
        if (object_type >= MAX_BACNET_OBJECT_TYPE) {
            printf("object-type=%u - it must be less than %u\r\n", object_type,
                MAX_BACNET_OBJECT_TYPE);
            return -EINVAL;
        }
        tag_value_arg++;
        args_remaining--;
        if (args_remaining <= 0) {
            printf("Error: not enough object property triples.\r\n");
            return -EINVAL;
        }

        /* Object_Instance */
        object_instance = strtol(argv[tag_value_arg], NULL, 0);
        if (object_instance > BACNET_MAX_INSTANCE) {
            printf("object-instance=%u - it must be less than %u\r\n", object_instance,
                BACNET_MAX_INSTANCE + 1);
            return -EINVAL;
        }
        tag_value_arg++;
        args_remaining--;
        if (args_remaining <= 0) {
            printf("Error: not enough object property triples.\r\n");
            return -EINVAL;
        }
        if (!wpm_req_encode_object(&apdu.buf, object_type, object_instance)) {
            printf("wpm req encode object error\r\n");
            return -EPERM;
        }

        while (1) {
            /* Property */
            scan_count = sscanf(argv[tag_value_arg], "%u[%u]", &property_id, &array_index);
            if (scan_count > 0) {
                if (property_id > MAX_BACNET_PROPERTY_ID) {
                    printf("property=%u - it must be less than %u\r\n", property_id,
                        MAX_BACNET_PROPERTY_ID + 1);
                    return -EINVAL;
                }
                if (scan_count <= 1) {
                    array_index = BACNET_ARRAY_ALL;
                }
            } else {
                printf("invalid property %s\r\n", argv[tag_value_arg]);
                return -EINVAL;
            }

            tag_value_arg++;
            args_remaining--;
            if (args_remaining <= 0) {
                printf("Error: not enough object property triples.\r\n");
                return -EINVAL;
            }

            /* Priority */
            priority = (uint8_t)strtol(argv[tag_value_arg], NULL, 0);
            if (priority > BACNET_MAX_PRIORITY) {
                printf("priority=%d - it must be less than %d\r\n", priority,
                    BACNET_MAX_PRIORITY + 1);
                return -EINVAL;
            }
            tag_value_arg++;
            args_remaining--;
            if (args_remaining <= 0) {
                printf("Error: not enough object property triples.\r\n");
                return -EINVAL;
            }

            if (!wpm_req_encode_property(&apdu.buf, property_id, array_index, priority)) {
                printf("wpm req encode property error\r\n");
                return -EPERM;
            }

            status = bacapp_parse_application_data(&apdu.buf, argv[tag_value_arg]);
            if (!status) {
                printf("Error: unable to parse the tag value\r\n");
                return -EPERM;
            }

            tag_value_arg++;
            args_remaining--;
            if (args_remaining <= 0) {
                goto send;
            }

            if (strcmp(argv[tag_value_arg], ",") == 0) {
                tag_value_arg++;
                args_remaining--;
                if (args_remaining <= 0) {
                    printf("Error: unable to parse the tag value\r\n");
                    return -EPERM;
                }
                break;
            }
        }
    }

send:
    rv = bacnet_init();
    if (rv < 0) {
        printf("bacnet init failed(%d)\r\n", rv);
        return rv;
    }

    apdu_set_default_service_handler();
    rv = el_loop_start(&el_default_loop);
    if (rv < 0) {
        printf("el loop start failed(%d)\r\n", rv);
        goto out0;
    }

    bacnet_addr_t dst_addr;
    bool success = false;
    count = 10;
    while (count--) {
        success = query_address_from_device(target_device_id, NULL, &dst_addr);
        if (success == true) {
            break;
        }
        sleep(1);
    }

    if (success == false) {
        printf("get address from device(%d) failed\r\n", target_device_id);
        goto out0;
    }
    
    tsm_invoker_t *invoker;
    invoker = tsm_alloc_invokeID(&dst_addr, SERVICE_CONFIRMED_WRITE_PROP_MULTIPLE,
        confirmed_ack_handler, NULL);
    if (invoker == NULL) {
        printf("alloc invokeID failed\r\n");
        goto out0;
    }
    
    if (!wpm_req_encode_end(&apdu.buf, invoker->invokeID)) {
        printf("encode end error\n\r");
        tsm_free_invokeID(invoker);
        goto out0;
    }

    count = 5;
    while (count--) {
        rv = tsm_send_apdu(invoker, &apdu.buf, PRIORITY_NORMAL, 0);
        if (rv < 0) {
            printf("\r\nsend WPM request failed(%d)\r\n", rv);
        } else {
            printf("\r\nsend WPM request ok\r\n");
        }
 
        sleep(1);
    }

out1:
    while (1) {
        sleep(10);
    }

out0:
    bacnet_exit();
    
    return 0;
}

