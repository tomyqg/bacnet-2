/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * writeprop.c
 * Original Author:  linzhixian, 2015-3-17
 *
 * Write Property Demo
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
#include "bacnet/service/wp.h"
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
    printf("%s: invalid pdu for write_property\r\n", __func__);
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
    if (invoker == NULL) {
        printf("%s: null invoker\r\n", __func__);
        return;
    }

    if (invoker->choice != SERVICE_CONFIRMED_WRITE_PROPERTY) {
        printf("%s: invalid service choice(%d)\r\n", __func__, invoker->choice);
        return;
    }

    if ((apdu == NULL) || (apdu->data == NULL)) {
        printf("%s: invalid argument\r\n", __func__);
        goto out;
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

out:
    tsm_free_invokeID(invoker);

    return;
}

/* ./writeprop & */
/* ./writeprop --help & */
/* ./writeprop Device_Id Type Instance Property[Index] Priority [CTag] Tag Value... & */
int main(int argc, char *argv[])
{
    uint32_t target_device_id = BACNET_MAX_INSTANCE;
    uint32_t object_instance = BACNET_MAX_INSTANCE;
    BACNET_OBJECT_TYPE object_type = OBJECT_ANALOG_INPUT;
    BACNET_PROPERTY_ID property_id = PROP_ACKED_TRANSITIONS;
    int32_t array_index = BACNET_ARRAY_ALL;    
    static uint8_t priority = 0;    /* 0 if not set, 1..16 if set */
    DECLARE_BACNET_BUF(apdu, MAX_APDU);
    bacnet_addr_t dst_addr;
    tsm_invoker_t *invoker;
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
            "%s Device_Instance Object_Type Object_Instance Property[Index] Priority Value &\r\n\r\n", argv[0]);
        return OK;
    } else if (argc != 7) {
        printf("invalid argc(%d)\r\n", argc);
        return -EINVAL;
    }

    target_device_id = strtol(argv[1], NULL, 0);
    object_type = strtol(argv[2], NULL, 0);
    object_instance = strtol(argv[3], NULL, 0);
    scan_count = sscanf(argv[4], "%u[%u]", &property_id, &array_index);
    if (scan_count < 1) {
        printf("invalid argv[4]\r\n");
        return -EINVAL;
    }

    priority = (uint8_t)strtol(argv[5], NULL, 0);

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
    
    printf("Target_Device_Object_Instance: %d\r\n", target_device_id);
    printf("Target_Object_Type: %d\r\n", object_type);
    printf("Target_Object_Instance: %d\r\n", object_instance);
    printf("Target_Object_Property: %d\r\n", property_id);
    printf("Target_Object_Index: %d\r\n", array_index);
    printf("Target_Object_Property_Priority: %d\r\n", priority);

    count = 5;
    while (count--) {
        sleep(1);
        if (!query_address_from_device(target_device_id, NULL, &dst_addr)) {
            printf("get address from device(%d) failed\r\n", target_device_id);
            continue;
        }

        invoker = tsm_alloc_invokeID(&dst_addr, SERVICE_CONFIRMED_WRITE_PROPERTY,
            confirmed_ack_handler, NULL);
        if (invoker == NULL) {
            printf("alloc invokeID failed\r\n");
            continue;
        }

        bacnet_buf_init(&apdu.buf, MAX_APDU);
        wp_req_encode(&apdu.buf, object_type, object_instance, property_id, array_index);
        if (!bacapp_parse_application_data(&apdu.buf, argv[6])) {
            printf("Error: unable to parse the tag value\r\n");
            tsm_free_invokeID(invoker);
            continue;
        }

        if (!wp_req_encode_end(&apdu.buf, invoker->invokeID, priority)) {
            printf("Error: buffer overflow\r\n");
            tsm_free_invokeID(invoker);
            continue;
        }
        
        rv = tsm_send_apdu(invoker, &apdu.buf, PRIORITY_NORMAL, 0);
        if (rv < 0) {
            printf("\r\nSend_Write_Property_Request failed(%d)\r\n", rv);
            tsm_free_invokeID(invoker);
        } else {
            printf("\r\nSend_Write_Property_Request ok\r\n");
        }
    }

out1:
    while (1) {
        sleep(10);
    }

out0:
    bacnet_exit();

    return 0;
}

