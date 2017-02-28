/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * readprop.c
 * Original Author:  linzhixian, 2015-5-29
 *
 * Test for Trend Log Object
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
#include "bacnet/bacdevobjpropref.h"
#include "bacnet/addressbind.h"
#include "bacnet/object/trendlog.h"
#include "bacnet/config.h"
#include "misc/eventloop.h"

static el_timer_t *my_timer = NULL;

static uint32_t Target_Device_Object_Instance = BACNET_MAX_INSTANCE;
static BACNET_OBJECT_TYPE Target_Object_Type = OBJECT_ANALOG_INPUT;
static uint32_t Target_Object_Instance = BACNET_MAX_INSTANCE;
static BACNET_PROPERTY_ID Target_Object_Property = PROP_ACKED_TRANSITIONS;
static int32_t Target_Object_Index = BACNET_ARRAY_ALL;
static uint32_t tx_count = 0;

static void rp_confirmed_ack_handler(bacnet_addr_t *src, BACNET_CONFIRMED_SERVICE_ACK_DATA *ack_data)
{
    BACNET_READ_PROPERTY_DATA rp_data;
    BACNET_APPLICATION_DATA_VALUE value;
    BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE property;
    uint8_t service_choice;
    uint8_t *application_data;
    uint16_t application_data_len;
    uint32_t src_device_id;
    object_instance_t *tl_obj;
    int len;
    int rv;

    if (!query_device_from_address(src, NULL, &src_device_id)) {
        printf("%s: get deviceid from src address failed\r\n", __func__);
        return;
    }
    
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

    property.deviceIndentifier.type = OBJECT_DEVICE;
    property.deviceIndentifier.instance = src_device_id;
    property.objectIdentifier.type = (uint16_t)rp_data.object_type;
    property.objectIdentifier.instance = rp_data.object_instance;
    property.propertyIdentifier = rp_data.property_id;
    property.arrayIndex = rp_data.array_index;

    tl_obj= trend_log_find_by_property(&property);
    if (tl_obj == NULL) {
        printf("%s: trendlog find failed\r\n", __func__);
        return;
    }

    printf("src_device_id = %d\r\n", src_device_id);
    printf("rp_data.object_type = %d\r\n", rp_data.object_type);
    printf("rp_data.object_instance = %d\r\n", rp_data.object_instance);
    printf("rp_data.object_property = %d\r\n", rp_data.property_id);
    printf("rp_data.array_index = %d\r\n", rp_data.array_index);
    printf("trendlog_instance = %d\r\n", tl_obj->instance);
    
    application_data = rp_data.application_data;
    application_data_len = rp_data.application_data_len;
    for (;;) {
        len = bacapp_decode_application_data(application_data, application_data_len, &value);
        if (len < 0) {
            printf("%s: decode application data failed(%d)\r\n", __func__, len);
            break;
        }

        if (len > application_data_len) {
            printf("%s: decode_len(%d) is larger than data_len(%d)\r\n", __func__, len,
                application_data_len);
            break;
        }

        rv = trend_log_record(tl_obj, &property, &value, NULL);
        if (rv < 0) {
            printf("%s: trendlog record failed(%d)\r\n", __func__, rv);
            break;
        }

        if (len < application_data_len) {
            application_data += len;
            application_data_len -= len;
        } else {
            break;
        }
    }

    return;
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

    if (apdu_decode_complex_ack(apdu, &service_ack_data)) {
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
        printf("%s: invalid service choice(%d)\r\n",__func__, service_ack_data.service_choice);
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

static void my_timer_handler(el_timer_t *timer)
{
    DECLARE_BACNET_BUF(tx_apdu, MIN_APDU);
    bacnet_addr_t dst_addr;
    tsm_invoker_t *invoker;
    int rv;

    if (!query_address_from_device(Target_Device_Object_Instance, NULL, &dst_addr)) {
        printf("%s: get address from device(%d) failed\r\n", __func__, Target_Device_Object_Instance);
        goto out;
    }

    invoker = tsm_alloc_invokeID(&dst_addr, SERVICE_CONFIRMED_READ_PROPERTY,
        confirmed_ack_handler, NULL);
    if (invoker == NULL) {
        printf("%s: alloc invokeID failed\r\n", __func__);
        goto out;
    }

    (void)bacnet_buf_init(&tx_apdu.buf, MIN_APDU);
    rv = rp_encode_apdu(&tx_apdu.buf, invoker->invokeID, Target_Object_Type, Target_Object_Instance,
        Target_Object_Property, Target_Object_Index);
    if ((rv < 0) || (rv > MIN_APDU)) {
        printf("%s: encode apdu failed(%d)\r\n", __func__, rv);
        tsm_free_invokeID(invoker);
        goto out;
    }

    rv = tsm_send_apdu(invoker, &tx_apdu.buf, PRIORITY_NORMAL, 0);
    if (rv < 0) {
        printf("\r\n%s: Send RP Request failed(%d)\r\n", __func__, rv);
        tsm_free_invokeID(invoker);
    } else {
        tx_count++;
        printf("\r\n%s: Send RP Request ok(%d)\r\n", __func__, tx_count);
    }

out:
    el_timer_mod(&el_default_loop, timer, 10 * 1000);
}

int main(int argc, char *argv[])
{
    int scan_count;
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
        Target_Device_Object_Instance = strtol(argv[1], NULL, 0);
        Target_Object_Type = strtol(argv[2], NULL, 0);
        Target_Object_Instance = strtol(argv[3], NULL, 0);
        scan_count = sscanf(argv[4], "%u[%u]", &Target_Object_Property, &Target_Object_Index);
        if (scan_count < 1) {
            printf("invalid argv[4]\r\n");
            return -EINVAL;
        }
    } else {
        printf("invalid argument\r\n");
        return -EINVAL;
    }
    
    if (Target_Device_Object_Instance > BACNET_MAX_INSTANCE) {
        printf("device-instance=%u - it must be less than %u\r\n", Target_Device_Object_Instance, 
            BACNET_MAX_INSTANCE);
        return -EINVAL;
    }

    printf("Target_Device_Object_Instance: %d\r\n", Target_Device_Object_Instance);
    printf("Target_Object_Type: %d\r\n", Target_Object_Type);
    printf("Target_Object_Instance: %d\r\n", Target_Object_Instance);
    printf("Target_Object_Property: %d\r\n", Target_Object_Property);
    if (scan_count == 2) {
        printf("Target_Object_Index: %d\r\n", Target_Object_Index);
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

    my_timer = el_timer_create(&el_default_loop, 2 * 1000);
    if (my_timer == NULL) {
        printf("%s: create timer failed\r\n", __func__);
        goto out0;
    }
    my_timer->handler = my_timer_handler;

out1:
    while (1) {
        sleep(10);
    }

out0:
    bacnet_exit();

    return 0;
}

