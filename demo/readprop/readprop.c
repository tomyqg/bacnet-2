/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * readprop.c
 * Original Author:  linzhixian, 2015-3-4
 *
 * Read Property Demo
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

#include "bacnet/bacenum.h"
#include "bacnet/tsm.h"
#include "bacnet/service/rp.h"
#include "bacnet/bacnet.h"
#include "bacnet/service/error.h"
#include "bacnet/addressbind.h"
#include "bacnet/apdu.h"
#include "bacnet/config.h"
#include "misc/bits.h"
#include "misc/eventloop.h"

static void rp_confirmed_ack_handler(bacnet_addr_t *src, BACNET_CONFIRMED_SERVICE_ACK_DATA *ack_data)
{
    BACNET_READ_PROPERTY_DATA rp_data;
    uint8_t service_choice; 
    int len;

    service_choice = ack_data->service_choice;
    
    if (service_choice != SERVICE_CONFIRMED_READ_PROPERTY) {
        printf("%s: invalid service_choice(%d)\r\n", __func__, service_choice);
        return;
    }

    len = rp_ack_decode(ack_data->service_data, ack_data->service_data_len, &rp_data);
    if (len < 0) {
        printf("%s: decode rp ack failed(%d)\r\n", __func__, len);
        return;
    }

    rp_ack_print_data(&rp_data);
}

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
    BACNET_CONFIRMED_SERVICE_ACK_DATA service_ack_data;
    
    if (apdu_decode_complex_ack(apdu, &service_ack_data) < 0) {
        printf("%s: decode ack header failed\r\n", __func__);
        return;

    }

    switch (service_ack_data.service_choice) {
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
        rp_confirmed_ack_handler(src, &service_ack_data);
        break;
        
    default:
        printf("%s: invalid service choice(%d)\r\n", __func__, service_ack_data.service_choice);
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
    if (invoker == NULL) {
        printf("%s: null invoker\r\n", __func__);
        return;
    }

    if (invoker->choice != SERVICE_CONFIRMED_READ_PROPERTY) {
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

/* ./readprop & */
/* ./readprop --help & */
/* ./readprop DeviceID Type Instance Property[Index] & */
int main(int argc, char *argv[])
{
    DECLARE_BACNET_BUF(tx_apdu, MIN_APDU);
    uint32_t target_device_id;
    BACNET_OBJECT_TYPE object_type;
    uint32_t object_instance;
    BACNET_PROPERTY_ID property_id;
    int32_t array_index = BACNET_ARRAY_ALL;
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
    }

    if ((argc == 2) && ((strcmp(argv[1], "--help") == 0))) {
        printf("\r\n[Usage]:\r\n"
            "%s Device_Instance Object_Type Object_Instance Property[Index] & \r\n\r\n", argv[0]);
        return OK;
    }

    if (argc == 5) {
        target_device_id = strtol(argv[1], NULL, 0);
        object_type = strtol(argv[2], NULL, 0);
        object_instance = strtol(argv[3], NULL, 0);
        scan_count = sscanf(argv[4], "%u[%u]", &property_id, &array_index);
        if (scan_count < 1) {
            printf("invalid argv[4]\r\n");
            return -EINVAL;
        }
    } else {
        printf("invalid argument\r\n");
        return -EINVAL;
    }
    
    if (target_device_id > BACNET_MAX_INSTANCE) {
        printf("device-instance=%u - it must be less than %u\r\n", target_device_id, 
            BACNET_MAX_INSTANCE);
        return -EINVAL;
    }

    printf("Target_Device_Object_Instance: %d\r\n", target_device_id);
    printf("Target_Object_Type: %d\r\n", object_type);
    printf("Target_Object_Instance: %d\r\n", object_instance);
    printf("Target_Object_Property: %d\r\n", property_id);
    if (scan_count == 2) {
        printf("Target_Object_Index: %d\r\n", array_index);
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
    
    count = 5;
    while (count--) {
        sleep(1);
        if (!query_address_from_device(target_device_id, NULL, &dst_addr)) {
            printf("get address from device(%d) failed\r\n", target_device_id);
            continue;
        }

        invoker = tsm_alloc_invokeID(&dst_addr, SERVICE_CONFIRMED_READ_PROPERTY,
            confirmed_ack_handler, NULL);
        if (invoker == NULL) {
            printf("alloc invokeID failed\r\n");
            continue;
        }

        (void)bacnet_buf_init(&tx_apdu.buf, MIN_APDU);
        rv = rp_encode_apdu(&tx_apdu.buf, invoker->invokeID, object_type, object_instance,
            property_id, array_index);
        if ((rv < 0) || (rv > MIN_APDU)) {
            printf("encode apdu failed(%d)\r\n", rv);
            tsm_free_invokeID(invoker);
            continue;
        }

        rv = tsm_send_apdu(invoker, &tx_apdu.buf, PRIORITY_NORMAL, 0);
        if (rv < 0) {
            printf("\r\nSend_Read_Property_Request failed(%d)\r\n", rv);
            tsm_free_invokeID(invoker);
        } else {
            printf("\r\nSend_Read_Property_Request ok\r\n");
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

