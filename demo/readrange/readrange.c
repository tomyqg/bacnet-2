/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * readrange.c
 * Original Author:  linzhixian, 2015-3-4
 *
 * Read Range Demo
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
#include "bacnet/service/rr.h"
#include "bacnet/bacnet.h"
#include "bacnet/service/error.h"
#include "bacnet/mstp.h"
#include "bacnet/bip.h"
#include "bacnet/network.h"
#include "bacnet/app.h"
#include "bacnet/apdu.h"
#include "bacnet/bactext.h"
#include "bacnet/bacapp.h"
#include "bacnet/addressbind.h"
#include "bacnet/config.h"
#include "misc/bits.h"
#include "misc/eventloop.h"

static uint32_t rr_tx_ok = 0;
static uint32_t rr_tx_err = 0;
static uint32_t rr_rx_all = 0;

static void rr_ack_print_data(BACNET_READ_RANGE_DATA *rrdata)
{
    int bit_number;
    int i;
    
    if ((rrdata == NULL) || (rrdata->rpdata.application_data == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return;
    }

    fprintf(stdout, "%s #%lu ", bactext_object_type_name(rrdata->rpdata.object_type),
        (unsigned long)rrdata->rpdata.object_instance);

    if (rrdata->rpdata.property_id < 512) {
        fprintf(stdout, "%s", bactext_property_name(rrdata->rpdata.property_id));
    } else {
        fprintf(stdout, "proprietary %u", (unsigned)rrdata->rpdata.property_id);
    }

    if (rrdata->rpdata.array_index != BACNET_ARRAY_ALL) {
        fprintf(stdout, "[%d]", rrdata->rpdata.array_index);
    }
    fprintf(stdout, ": ");

    bacapp_fprint_value(stdout, rrdata->rpdata.application_data, rrdata->rpdata.application_data_len);

    fprintf(stdout, " ItemCount: %d ", rrdata->ItemCount);
    fprintf(stdout, "FirstSequence: %d ", rrdata->FirstSequence);
    fprintf(stdout, "ResultFlags: ");
    
    bit_number = bitstring_size(&rrdata->ResultFlags);
    for (i = 0; i < bit_number; i++) {
        fprintf(stdout, "%s ", bitstring_get_bit(&rrdata->ResultFlags, i) ? "1" : "0");
    }

    fprintf(stdout, "\r\n");
    
    return;
}

static void rr_complex_ack_handler(bacnet_addr_t *src, bacnet_buf_t *apdu)
{
    BACNET_CONFIRMED_SERVICE_ACK_DATA service_ack_data;
    BACNET_READ_RANGE_DATA rrdata;
    int len;
    
    if (apdu_decode_complex_ack(apdu, &service_ack_data) < 0) {
        printf("%s: decode ack header failed\r\n", __func__);
        return;

    }

    if (service_ack_data.service_choice != SERVICE_CONFIRMED_READ_RANGE) {
        printf("%s: invalid service choice(%d)\r\n", __func__, service_ack_data.service_choice);
        return;
    }

    memset(&rrdata, 0, sizeof(rrdata));
    len = rr_ack_decode(service_ack_data.service_data, service_ack_data.service_data_len, &rrdata);
    if (len < 0) {
        printf("%s: decode service ack failed(%d)\r\n", __func__, len);
        return;
    }

    rr_ack_print_data(&rrdata);
    
    return;
}

static void rr_simple_ack_handler(bacnet_addr_t *src, bacnet_buf_t *apdu)
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

static void rr_error_handler(bacnet_addr_t *src, bacnet_buf_t *apdu)
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

static void rr_reject_handler(bacnet_addr_t *src, bacnet_buf_t *apdu)
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

static void rr_abort_handler(bacnet_addr_t *src, bacnet_buf_t *apdu)
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

static void rr_confirmed_ack_handler(tsm_invoker_t *invoker, bacnet_buf_t *apdu,
                BACNET_PDU_TYPE apdu_type)
{    
    if (invoker == NULL) {
        printf("%s: null invoker\r\n", __func__);
        return;
    }

    if (invoker->choice != SERVICE_CONFIRMED_READ_RANGE) {
        printf("%s: invalid service choice(%d)\r\n", __func__, invoker->choice);
        return;
    }

    if ((apdu == NULL) || (apdu->data == NULL)) {
        printf("%s: invalid argument\r\n", __func__);
        goto out;
    }

    rr_rx_all++;
    
    switch (apdu_type) {
    case PDU_TYPE_SIMPLE_ACK:
        rr_simple_ack_handler(&invoker->addr, apdu);
        break;
        
    case PDU_TYPE_COMPLEX_ACK:
        rr_complex_ack_handler(&invoker->addr, apdu);
        break;
        
    case PDU_TYPE_ERROR:
        rr_error_handler(&invoker->addr, apdu);
        break;
        
    case PDU_TYPE_REJECT:
        rr_reject_handler(&invoker->addr, apdu);
        break;
        
    case PDU_TYPE_ABORT:
        rr_abort_handler(&invoker->addr, apdu);
        break;
        
    default:
        printf("%s: unknown apdu type(%d)\r\n", __func__, apdu_type);
        break;
    }

out:
    tsm_free_invokeID(invoker);

    return;
}

/* ./readrange & */
/* ./readrange --help & */
/* ./readrange DeviceID ObjectType ObjectInstance Property[Index] RangeType Index Count & */
int main(int argc, char *argv[])
{
    DECLARE_BACNET_BUF(tx_apdu, MIN_APDU);
    BACNET_READ_RANGE_DATA rrdata;
    uint32_t target_device_id;
    bacnet_addr_t dst_addr;
    tsm_invoker_t *invoker;
    long int value;
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

        app_set_dbg_level(6);
        network_set_dbg_level(6);
        datalink_set_dbg_level(6);
        mstp_set_dbg_level(6);
        bip_set_dbg_level(6);
    
        goto out1;
    }

    if ((argc == 2) && ((strcmp(argv[1], "--help") == 0))) {
        printf("\r\n[Usage]:\r\n"
            "%s Device_Instance Object_Type Object_Instance Property[Index] RangeType Index Count & \r\n\r\n",
                argv[0]);
        return OK;
    }

    if (argc != 8) {
        printf("invalid argument count\r\n");
        return -EINVAL;
    }

    value = strtol(argv[1], NULL, 0);
    if ((value < 1) || (value > BACNET_MAX_INSTANCE)) {
        printf("invalid argv[1]: %ld\r\n", value);
        return -EINVAL;
    }
    target_device_id = value;
    
    value = strtol(argv[2], NULL, 0);
    if ((value < 0) || (value >= MAX_BACNET_OBJECT_TYPE)) {
        printf("invalid argv[2]: %ld\r\n", value);
        return -EINVAL;
    }
    rrdata.rpdata.object_type = value;

    value = strtol(argv[3], NULL, 0);
    if ((value < 0) || (value > BACNET_MAX_INSTANCE)) {
        printf("invalid argv[3]: %ld\r\n", value);
        return -EINVAL;
    }
    rrdata.rpdata.object_instance = value;
    
    scan_count = sscanf(argv[4], "%u[%u]", &rrdata.rpdata.property_id, &rrdata.rpdata.array_index);
    if (scan_count < 1) {
        printf("invalid argv[4]: %s\r\n", argv[4]);
        return -EINVAL;
    }
    
    value = strtol(argv[5], NULL, 0);
    if (value > RR_READ_ALL) {
        printf("invalid argv[5]: %ld\r\n", value);
        return -EINVAL;
    }
    rrdata.Range.RequestType = value;

    value = strtol(argv[6], NULL, 0);
    if (value <= 0) {
        printf("invalid argv[6]: %ld\r\n", value);
        return -EINVAL;
    }
    rrdata.Range.Range.RefIndex = value;

    value = strtol(argv[7], NULL, 0);
    if (value == 0) {
        printf("invalid argv[7]: %ld\r\n", value);
        return -EINVAL;
    }
    rrdata.Range.Count = value;

    printf("Target_Device_Object_Instance: %d\r\n", target_device_id);
    printf("Target_Object_Type: %d\r\n", rrdata.rpdata.object_type);
    printf("Target_Object_Instance: %d\r\n", rrdata.rpdata.object_instance);
    printf("Target_Object_Property: %d\r\n", rrdata.rpdata.property_id);
    if (scan_count == 2) {
        printf("Target_Object_Index: %d\r\n", rrdata.rpdata.array_index);
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

    app_set_dbg_level(6);
    network_set_dbg_level(6);
    datalink_set_dbg_level(6);
    mstp_set_dbg_level(6);
    bip_set_dbg_level(6);
    
    sleep(5);

    count = 100;
    while (count--) {
        if (!query_address_from_device(target_device_id, NULL, &dst_addr)) {
            printf("get address from device(%d) failed\r\n", target_device_id);
            continue;
        }

        invoker = tsm_alloc_invokeID(&dst_addr, SERVICE_CONFIRMED_READ_RANGE,
            rr_confirmed_ack_handler, NULL);
        if (invoker == NULL) {
            printf("alloc invokeID failed\r\n");
            continue;
        }

        (void)bacnet_buf_init(&tx_apdu.buf, MIN_APDU);
        rv = rr_encode_service_request(&tx_apdu.buf, invoker->invokeID, &rrdata);
        if ((rv < 0) || (rv > MIN_APDU)) {
            APP_ERROR("encode RR request failed(%d)\r\n", rv);
            tsm_free_invokeID(invoker);
            continue;
        }
    
        rv = tsm_send_apdu(invoker, &tx_apdu.buf, PRIORITY_NORMAL, 0);
        if (rv < 0) {
            printf("\r\nsend RR request failed(%d)\r\n", rv);
            tsm_free_invokeID(invoker);
            rr_tx_err++;
        } else {
            printf("\r\nsend RR request ok\r\n");
            rr_tx_ok++;
        }

        sleep(1);
    }

    printf("rr_tx_err(%d), rr_tx_ok(%d), rr_rx_all(%d)\r\n", rr_tx_err, rr_tx_ok, rr_rx_all);

out1:
    while (1) {
        sleep(10);
    }

out0:
    bacnet_exit();
    
    return 0;
}

