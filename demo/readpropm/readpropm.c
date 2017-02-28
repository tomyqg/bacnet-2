/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * readpropm.c
 * Original Author:  linzhixian, 2015-3-18
 *
 * Read Property Multiple Demo
 *
 * History
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "bacnet/config.h"
#include "bacnet/bacenum.h"
#include "bacnet/tsm.h"
#include "bacnet/service/rpm.h"
#include "bacnet/bacnet.h"
#include "bacnet/service/error.h"
#include "bacnet/addressbind.h"
#include "bacnet/apdu.h"
#include "misc/bits.h"
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
    BACNET_CONFIRMED_SERVICE_ACK_DATA ack_data;

    if (apdu_decode_complex_ack(apdu, &ack_data)) {
        printf("%s: decode ack header failed\r\n", __func__);
        return;
    }

    switch (ack_data.service_choice) {
    case SERVICE_CONFIRMED_GET_ALARM_SUMMARY:
    case SERVICE_CONFIRMED_GET_ENROLLMENT_SUMMARY:
    case SERVICE_CONFIRMED_GET_EVENT_INFORMATION:
    /* File Access Services */
    case SERVICE_CONFIRMED_ATOMIC_READ_FILE:
    case SERVICE_CONFIRMED_ATOMIC_WRITE_FILE:
    /* Object Access Services */
    case SERVICE_CONFIRMED_CREATE_OBJECT:
    case SERVICE_CONFIRMED_READ_PROPERTY:
    case SERVICE_CONFIRMED_READ_PROP_CONDITIONAL:
    case SERVICE_CONFIRMED_READ_PROP_MULTIPLE:
    case SERVICE_CONFIRMED_READ_RANGE:
    case SERVICE_CONFIRMED_PRIVATE_TRANSFER:
    /* Virtual Terminal Services */
    case SERVICE_CONFIRMED_VT_OPEN:
    case SERVICE_CONFIRMED_VT_DATA:
    /* Security Services */
    case SERVICE_CONFIRMED_AUTHENTICATE:
        rpm_ack_print_data(ack_data.service_data, ack_data.service_data_len);
        break;
        
    default:
        printf("%s: invalid service choice(%d)\r\n",__func__, ack_data.service_choice);
        break;
    }

    return;
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

    if (invoker->choice != SERVICE_CONFIRMED_READ_PROP_MULTIPLE) {
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

/* ./readpropm & */
/* ./readpropm --help & */
/* ./readpropm DeviceID Type Instance Property[Index][,Property[Index]] [Type ...] & */
int main(int argc, char *argv[])
{
    uint32_t target_device_id = BACNET_MAX_INSTANCE;

    DECLARE_BACNET_BUF(tx_apdu, MAX_APDU);
    char *filename;
    char *property_token;
    BACNET_OBJECT_TYPE  object_type;
    uint32_t instance;
    BACNET_PROPERTY_ID property_id;
    uint32_t property_array_index;
    int args_remaining;
    int tag_value_arg;
    int arg_sets;
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
        
        goto out2;
    }

    if ((argc == 2) && ((strcmp(argv[1], "--help") == 0))) {
        filename = argv[0];
        printf("\r\n[Usage]:\r\n"
            "%s Device_Instance Object_Type Object_Instance "
            "Property[Index][,Property[Index]] [Object_Type ...] & \r\n", filename);
        
        printf("\r\n[Example]:\r\n"
                "If you want to read the PRESENT_VALUE property and various\r\n"
                "array elements of the PRIORITY_ARRAY in Device 123\r\n"
                "Analog Output object 99, use the following command:\r\n"
                "%s 123 1 99 85,87[0],87 &\r\n\r\n"
                "If you want to read the PRESENT_VALUE property in objects\r\n"
                "Analog Input 77 and Analog Input 78 in Device 123\r\n"
                "use the following command:\r\n"
                "%s 123 0 77 85 0 78 85 &\r\n\r\n"
                "If you want to read the ALL property in\r\n"
                "Device object 123, you would use the following command:\r\n"
                "%s 123 8 123 8 &\r\n\r\n"
                "If you want to read the OPTIONAL property in\r\n"
                "Device object 123, you would use the following command:\r\n"
                "%s 123 8 123 80 &\r\n\r\n"
                "If you want to read the REQUIRED property in\r\n"
                "Device object 123, you would use the following command:\r\n"
                "%s 123 8 123 105 &\r\n\r\n", filename, filename, filename, filename, filename);

        return 0;
    }

    if (argc < 5) {
        printf("invalid argc(%d)\r\n", argc);
        return -EINVAL;
    }

    args_remaining = (argc - 2);
    if (args_remaining % 3) {
        printf("invalid argc(%d)\r\n", argc);
        return -EINVAL;
    }
    
    target_device_id = strtol(argv[1], NULL, 0);
    if (target_device_id >= BACNET_MAX_INSTANCE) {
        printf("device-instance=%u - it must be less than %u\r\n", target_device_id, 
            BACNET_MAX_INSTANCE);
        return -EINVAL;
    }

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
    invoker = tsm_alloc_invokeID(&dst_addr, SERVICE_CONFIRMED_READ_PROP_MULTIPLE,
        confirmed_ack_handler, NULL);
    if (invoker == NULL) {
        printf("alloc invokeID failed\r\n");
        goto out0;
    }

    (void)bacnet_buf_init(&tx_apdu.buf, MAX_APDU);

    arg_sets = 0;
    args_remaining = (argc - 2);
    while (args_remaining > 0) {
        tag_value_arg = 2 + (arg_sets * 3);
        object_type = strtol(argv[tag_value_arg], NULL, 0);
        if (object_type >= MAX_BACNET_OBJECT_TYPE) {
            printf("object-type=%u - it must be less than %u\r\n", object_type,
                MAX_BACNET_OBJECT_TYPE);
            goto out1;
        }
        tag_value_arg++;
        args_remaining--;
        if (args_remaining <= 0) {
            printf("Error: not enough object property triples.\r\n");
            goto out1;
        }
        
        instance = strtol(argv[tag_value_arg], NULL, 0);
        if (instance > BACNET_MAX_INSTANCE) {
            printf("object-instance=%u - it must be less than %u\r\n", instance,
                BACNET_MAX_INSTANCE + 1);
            goto out1;
        }
        tag_value_arg++;
        args_remaining--;
        if (args_remaining <= 0) {
            printf("Error: not enough object property triples.\r\n");
            goto out1;
        }
        
        (void)rpm_req_encode_object(&tx_apdu.buf, object_type, instance);
            
        property_token = strtok(argv[tag_value_arg], ",");
        /* add all the properties and optional index to our list */
        do {
            scan_count = sscanf(property_token, "%u[%u]", &property_id, &property_array_index);
            if (scan_count > 0) {
                if (property_id > MAX_BACNET_PROPERTY_ID) {
                    printf("property=%u - it must be less than %u\r\n", property_id, 
                        MAX_BACNET_PROPERTY_ID + 1);
                    goto out1;
                }
            }
            if (scan_count <= 1) {
                property_array_index = BACNET_ARRAY_ALL;
            }
                
            (void)rpm_req_encode_property(&tx_apdu.buf, property_id, property_array_index);

            /* is there another property? */
            property_token = strtok(NULL, ",");
        } while (property_token != NULL);
        
        /* used up another arg */
        tag_value_arg++;
        args_remaining--;
        arg_sets++;
    }

    (void)rpm_req_encode_end(&tx_apdu.buf, invoker->invokeID);

    uint8_t *tmp_data, *tmp_end;
    uint32_t tmp_data_len;

    tmp_data = tx_apdu.buf.data;
    tmp_end = tx_apdu.buf.end;
    tmp_data_len = tx_apdu.buf.data_len;

    count = 50;
    while (count--) {
        rv = tsm_send_apdu(invoker, &tx_apdu.buf, PRIORITY_NORMAL, 0);
        if (rv < 0) {
            printf("\r\nsend RPM request failed(%d)\r\n", rv);
        } else {
            printf("\r\nsend RPM request ok\r\n");
        }

        tx_apdu.buf.data = tmp_data;
        tx_apdu.buf.end = tmp_end;
        tx_apdu.buf.data_len = tmp_data_len;
        
        sleep(1);
    }

out2:
    while (1) {
        sleep(10);
    }

out1:
    tsm_free_invokeID(invoker);

out0:
    bacnet_exit();

    return 0;
}

