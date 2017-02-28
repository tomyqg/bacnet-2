/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * factory_test.c
 * Original Author:  linzhixian, 2016-7-13
 *
 * Factory Test
 *
 * History
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "bacnet/bacenum.h"
#include "bacnet/tsm.h"
#include "bacnet/service/rpm.h"
#include "bacnet/bacnet.h"
#include "bacnet/addressbind.h"
#include "bacnet/service/error.h"
#include "bacnet/mstp.h"
#include "bacnet/bip.h"
#include "bacnet/network.h"
#include "bacnet/app.h"
#include "bacnet/apdu.h"
#include "bacnet/config.h"
#include "bacnet/object/device.h"
#include "misc/utils.h"
#include "misc/eventloop.h"

typedef struct factory_mib_s {
    uint32_t target_device_id;
    uint32_t tx_ok;
    uint32_t tx_err;
    uint32_t rx_all;
} factory_mib_t;

static char app_conf[20] = "app.conf_";
static char network_conf[20] = "network.conf_";

static factory_mib_t *factory_mib_array = NULL;
static uint32_t target_device_count = 0;

static uint32_t factory_rx_count = 0;
static const char LED_TRIGGER[] = "/sys/devices/platform/gpio-leds/leds/zbt-wa05:blue:status/trigger";
static const char LED_SHOT[] = "/sys/devices/platform/gpio-leds/leds/zbt-wa05:blue:status/shot";

static int shot_fd = -1;

cJSON *bacnet_get_app_cfg(void)
{
    cJSON *cfg;

    cfg = load_json_file(app_conf);
    if (cfg == NULL) {
        printf("%s: load %s failed\r\n", __func__, app_conf);
        return NULL;
    }
    
    if (cfg->type != cJSON_Object) {
        printf("%s: invalid cJSON type\r\n", __func__);
        cJSON_Delete(cfg);
        return NULL;
    }

    return cfg;
}

cJSON *bacnet_get_network_cfg(void)
{
    cJSON *cfg;
    
    cfg = load_json_file(network_conf);
    if (cfg == NULL) {
        printf("%s: load %s failed\r\n", __func__, network_conf);
        return NULL;
    }

    if (cfg->type != cJSON_Object) {
        printf("%s: invalid cJSON type\r\n", __func__);
        cJSON_Delete(cfg);
        return NULL;
    }

    return cfg;
}

static int factory_status_led_init(void)
{
    int fd;
    int nwrite;
    
    fd = open(LED_TRIGGER, O_RDWR);
    if (fd < 0) {
        printf("%s: open led trigger failed(%s).\n", __func__, strerror(errno));
        return -EPERM;
    }

    nwrite = write(fd, "oneshot", sizeof("oneshot"));
    close(fd);
    if (nwrite < 0) {
        printf("%s: write to led trigger failed(%s).\n", __func__, strerror(errno));
        return -EPERM;
    }

    shot_fd = open(LED_SHOT, O_WRONLY);
    if (shot_fd < 0) {
        printf("%s: open shot file failed(%s)\n", __func__, strerror(errno));
        return -EPERM;
    }

    return OK;
}

static void factory_status_led_shot(void)
{
    if (shot_fd < 0) {
        printf("%s: invalid shot_fd(%d)\r\n", __func__, shot_fd);
        return;
    }

    if (write(shot_fd, "1", 1) < 0) {
        printf("%s: write shot file failed(%s)\n", __func__, strerror(errno));
    }
}

static void factory_status_led_exit(void)
{
    if (shot_fd < 0) {
        return;
    }
    
    close(shot_fd);
}

static void factory_mib_show(void)
{
    int i;

    printf("\r\n#DeviceID(%u) MIB show:\r\n", device_object_instance_number());
    for (i = 0; i < target_device_count; i++) {
        printf("Target_DeviceId(%u): tx_err(%u), tx_ok(%u), rx_all(%u)\r\n",
            factory_mib_array[i].target_device_id, factory_mib_array[i].tx_err, 
            factory_mib_array[i].tx_ok, factory_mib_array[i].rx_all);
    }   
}

static void factory_complex_ack_handler(bacnet_addr_t *src, bacnet_buf_t *apdu)
{
    BACNET_CONFIRMED_SERVICE_ACK_DATA ack_data;
    
    if (apdu_decode_complex_ack(apdu, &ack_data) < 0) {
        printf("%s: decode ack header failed\r\n", __func__);
        return;

    }

    if (ack_data.service_choice != SERVICE_CONFIRMED_READ_PROP_MULTIPLE) {
        printf("%s: invalid service choice(%d)\r\n", __func__, ack_data.service_choice);
        return;
    }

    //rpm_ack_print_data(ack_data.service_data, ack_data.service_data_len);
    
    return;
}

static void factory_simple_ack_handler(bacnet_addr_t *src, bacnet_buf_t *apdu)
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

static void factory_error_handler(bacnet_addr_t *src, bacnet_buf_t *apdu)
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

static void factory_reject_handler(bacnet_addr_t *src, bacnet_buf_t *apdu)
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

static void factory_abort_handler(bacnet_addr_t *src, bacnet_buf_t *apdu)
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

static void factory_confirmed_ack_handler(tsm_invoker_t *invoker, bacnet_buf_t *apdu,
                BACNET_PDU_TYPE apdu_type)
{
    factory_mib_t *factory_mib;

    if (invoker == NULL) {
        printf("%s: null invoker\r\n", __func__);
        return;
    }

    if (invoker->choice != SERVICE_CONFIRMED_READ_PROP_MULTIPLE) {
        printf("%s: invalid service choice(%d)\r\n", __func__, invoker->choice);
        return;
    }

    if ((apdu == NULL) && (apdu_type == MAX_PDU_TYPE)) {
        printf("%s: ack timeout\r\n", __func__);
        goto out;
    }
    
    if (apdu->data == NULL) {
        printf("%s: null apdu\r\n", __func__);
        goto out;
    }

    factory_mib = (factory_mib_t *)invoker->data;
    if (factory_mib == NULL) {
        printf("%s: null factory mib\r\n", __func__);
        goto out;
    }

    factory_rx_count++;
    factory_mib->rx_all++;
    
    switch (apdu_type) {
    case PDU_TYPE_SIMPLE_ACK:
        factory_simple_ack_handler(&invoker->addr, apdu);
        break;
        
    case PDU_TYPE_COMPLEX_ACK:
        factory_complex_ack_handler(&invoker->addr, apdu);
        break;
        
    case PDU_TYPE_ERROR:
        factory_error_handler(&invoker->addr, apdu);
        break;
        
    case PDU_TYPE_REJECT:
        factory_reject_handler(&invoker->addr, apdu);
        break;
        
    case PDU_TYPE_ABORT:
        factory_abort_handler(&invoker->addr, apdu);
        break;
        
    default:
        printf("%s: unknown apdu type(%d)\r\n", __func__, apdu_type);
        break;
    }

out:
    tsm_free_invokeID(invoker);

    return;
}

static int mstp_test(datalink_mstp_t *mstp)
{
    uint8_t test_data[501] = {};
    uint32_t seconds;
    uint32_t test_len;
    int i;
    
    if (mstp == NULL) {
        return -EINVAL;
    }
    
    for (i = 0; i < sizeof(test_data); i++) {
        test_data[i] = (uint8_t)i;
    }

    seconds = el_current_second();
    test_len = seconds % sizeof(test_data) + 1;
    if (mstp_test_remote(mstp, mstp->max_master, test_data, test_len, NULL, NULL) < 0) {
        printf("mstp_test_remote failed\r\n");
        return -EPERM;
    }

    return OK;
}

static void mstp_test_result(datalink_mstp_t *mstp)
{
    if (mstp != NULL) {
        int res = mstp_get_test_result(mstp, NULL);
        if (res < 0) {
            printf("mstp_get_test_result failed(%d)\r\n", res);
        } else if (res == 0) {
            printf("mstp node(%d) not on line.\r\n", mstp->max_master);
        }
    }
}

/* ./factory_test & */
/* ./factory_test --help & */
/* ./factory_test ProcessID DeviceID_List Type Instance Property[Index] & */
int main(int argc, char *argv[])
{
    DECLARE_BACNET_BUF(tx_apdu, MAX_APDU);
    BACNET_OBJECT_TYPE object_type;
    uint32_t object_instance;
    BACNET_PROPERTY_ID property_id;
    bacnet_addr_t dst_addr;
    tsm_invoker_t *invoker;
    int32_t array_index;
    long int value;
    int count;
    int loop;
    char *token;
    int i, j;
    int rv;

    if (argc == 2) {
        if (strcmp(argv[1], "--help") == 0) {
            printf("\r\n[Usage]:\r\n"
                "%s ProcessID Target_Device_List Object_Type Object_Instance Property[Index] & \r\n\r\n", argv[0]);

            printf("\r\n[Example]:\r\n"
                "If you want to read the PRESENT_VALUE property\r\n"
                "in Analog Input Object 1 in Device 2,3 and 4,\r\n"
                "you would use the following command:\r\n"
                "%s 1 2,3,4 0 1 85 &\r\n\r\n", argv[0]);
            
            return OK;
        }

        if (strlen(argv[1]) > 2) {
            printf("the process ID value should be between 0 and 99\r\n");
            return -EPERM;
        }

        strcat(app_conf, argv[1]);
        strcat(network_conf, argv[1]);

        rv = bacnet_init();
        if (rv < 0) {
            printf("bacnet init failed(%d)\r\n", rv);
            return rv;
        }

        apdu_set_default_service_handler();
        rv = el_loop_start(&el_default_loop);
        if (rv < 0) {
            printf("el loop start failed(%d)\r\n", rv);
            bacnet_exit();
            return rv;
        }

        app_set_dbg_level(6);
        network_set_dbg_level(6);
        datalink_set_dbg_level(6);
        mstp_set_dbg_level(6);
        bip_set_dbg_level(6);
        
        goto delay;
    }

    if (argc != 6) {
        printf("invalid argument, please input: %s --help\r\n", argv[0]);
        return -EINVAL;
    }

    rv = factory_status_led_init();
    if (rv < 0) {
        printf("status led init failed(%d)\r\n", rv);
        return rv;
    }

    strcat(app_conf, argv[1]);
    strcat(network_conf, argv[1]);

    factory_mib_array = (factory_mib_t *)malloc(sizeof(factory_mib_t) * 20);
    if (factory_mib_array == NULL) {
        printf("malloc factory_mib_array failed\r\n");
        goto err0;
    }
    memset(factory_mib_array, 0, sizeof(factory_mib_t) * 20);
    
    target_device_count = 0;
    token = strtok(argv[2], ",");
    while (token) {
        value = strtol(token, NULL, 0);
        if ((value < 1) || (value > BACNET_MAX_INSTANCE)) {
            printf("invalid DeviceID: %ld\r\n", value);
            goto err1;
        }

        if (target_device_count >= 20) {
            break;
        }
        factory_mib_array[target_device_count].target_device_id = (uint32_t)value;
        target_device_count++;

        /* is there another property? */
        token = strtok(NULL, ",");
    }

    value = strtol(argv[3], NULL, 0);
    if ((value < 0) || (value >= MAX_BACNET_OBJECT_TYPE)) {
        printf("invalid argv[3]: %ld\r\n", value);
        goto err1;
    }
    object_type = (BACNET_OBJECT_TYPE)value;

    value = strtol(argv[4], NULL, 0);
    if ((value < 0) || (value > BACNET_MAX_INSTANCE)) {
        printf("invalid argv[4]: %ld\r\n", value);
        goto err1;
    }
    object_instance = (uint32_t)value;

    array_index = BACNET_ARRAY_ALL;
    count = sscanf(argv[5], "%u[%u]", &property_id, &array_index);
    if (count < 1) {
        printf("invalid argv[5]: %s\r\n", argv[5]);
        goto err1;
    }

    printf("\r\nTarget_Device_Object_Instance: ");
    for (i = 0; i < target_device_count; i++) {
        printf("%d  ", factory_mib_array[i].target_device_id);
    }
    printf("\r\nTarget_Object_Type: %d\r\n", object_type);
    printf("Target_Object_Instance: %d\r\n", object_instance);
    printf("Target_Object_Property: %d\r\n", property_id);
    if (count == 2) {
        printf("Target_Object_Index: %d\r\n", array_index);
    }
    
    rv = bacnet_init();
    if (rv < 0) {
        printf("bacnet init failed(%d)\r\n", rv);
        goto err1;
    }
    
    apdu_set_default_service_handler();
    rv = el_loop_start(&el_default_loop);
    if (rv < 0) {
        printf("el loop start failed(%d)\r\n", rv);
        goto err2;
    }

    app_set_dbg_level(6);
    network_set_dbg_level(6);
    datalink_set_dbg_level(6);
    mstp_set_dbg_level(6);
    bip_set_dbg_level(6);
    
    sleep(5);

    loop = 100;
    while (loop) {
        for (i = 0; i < target_device_count; i++) {
            if (!query_address_from_device(factory_mib_array[i].target_device_id, NULL, &dst_addr)) {
                printf("get address from device(%d) failed\r\n", factory_mib_array[i].target_device_id);
                continue;
            }

            printf("Device(%d) Bacnet Address:  ", factory_mib_array[i].target_device_id);
            PRINT_BACNET_ADDRESS(&dst_addr);

            invoker = tsm_alloc_invokeID(&dst_addr, SERVICE_CONFIRMED_READ_PROP_MULTIPLE,
                factory_confirmed_ack_handler, (void *)&factory_mib_array[i]);
            if (invoker == NULL) {
                printf("alloc invokeID failed\r\n");
                continue;
            }

            (void)bacnet_buf_init(&tx_apdu.buf, MAX_APDU);
            (void)rpm_req_encode_object(&tx_apdu.buf, object_type, object_instance);
            
            for (j = 0; j < 100; j++) {
                (void)rpm_req_encode_property(&tx_apdu.buf, PROP_PRESENT_VALUE, BACNET_ARRAY_ALL);
            }
            
            (void)rpm_req_encode_end(&tx_apdu.buf, invoker->invokeID);

            rv = tsm_send_apdu(invoker, &tx_apdu.buf, PRIORITY_NORMAL, 0);
            if (rv < 0) {
                printf("send RPM request to device(%d) failed\r\n", factory_mib_array[i].target_device_id);
                tsm_free_invokeID(invoker);
                factory_mib_array[i].tx_err++;
            } else {
                factory_mib_array[i].tx_ok++;
            }
        }

        datalink_mstp_t *mstp = mstp_next_port(NULL);
        if (mstp != NULL) {
            if (mstp_test(mstp) < 0) {
                mstp = NULL;
            }
        }

        sleep(2);

        if (mstp != NULL) {
            mstp_test_result(mstp);
        }

        count = factory_rx_count;
        factory_rx_count = 0;
        if (count == 0) {
            printf("############### No ack\r\n");
        }
        while (count--) {
            factory_status_led_shot();
            usleep(300000);
        }
    }

    factory_mib_show();

    free(factory_mib_array);

delay:
    while (1) {
        sleep(2);
    }

err2:
    bacnet_exit();

err1:
    free(factory_mib_array);

err0:
    factory_status_led_exit();
    
    return -EPERM;
}

