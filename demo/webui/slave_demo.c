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
#include <limits.h>
#include <signal.h>
#include <zlib.h>

#include "bacnet/bacnet.h"
#include "bacnet/bacenum.h"
#include "bacnet/apdu.h"
#include "misc/eventloop.h"
#include "misc/utils.h"
#include "misc/cJSON.h"
#include "misc/usbuartproxy.h"

static pid_t subprocess[256];
static int subcount = 0;

static char rs485_ifname[16] = "";

static char dev_name[32] = "";

static uint32_t serial_crc = 0;

cJSON *bacnet_get_app_cfg(void)
{
    uint32_t device_id;

    cJSON *cfg = cJSON_CreateObject();

    device_id = serial_crc & 0x0ffff;
    cJSON_AddNumberToObject(cfg, "Device_Id", device_id);

    sprintf(dev_name, "Slave %d", device_id);
    cJSON_AddStringToObject(cfg, "Device_Name", dev_name);

    return cfg;
}

cJSON *bacnet_get_network_cfg(void)
{
    cJSON *cfg = cJSON_CreateObject();
    cJSON_AddItemToObject(cfg, "route_table", cJSON_CreateArray());

    cJSON *port = cJSON_CreateArray();
    cJSON_AddItemToObject(cfg, "port", port);

    cJSON *rs485port = cJSON_CreateObject();
    cJSON_AddItemToArray(port, rs485port);

    cJSON_AddTrueToObject(rs485port, "enable");
    cJSON_AddNumberToObject(rs485port, "net_num", 0);
    cJSON_AddStringToObject(rs485port, "dl_type", "MSTP");
    cJSON_AddStringToObject(rs485port, "resource_name", "rs485");
    cJSON_AddNumberToObject(rs485port, "baudrate", 9600);
    cJSON_AddNumberToObject(rs485port, "this_station", 127);
    cJSON_AddTrueToObject(rs485port, "auto_baudrate");
    cJSON_AddTrueToObject(rs485port, "auto_polarity");
    cJSON_AddTrueToObject(rs485port, "auto_mac");
    cJSON_AddNumberToObject(rs485port, "reply_fast_timeout", 1);
    cJSON_AddNumberToObject(rs485port, "usage_fast_timeout", 1);

    cJSON *fast_nodes = cJSON_CreateArray();
    cJSON_AddItemToObject(rs485port, "fast_nodes", fast_nodes);

    for (int i=0; i<255; ++i) {
        cJSON_AddItemToArray(fast_nodes, cJSON_CreateNumber(i));
    }

    return cfg;
}

cJSON *bacnet_get_resource_cfg(void)
{
    cJSON *cfg = cJSON_CreateObject();
    cJSON *rs485 = cJSON_CreateObject();
    cJSON_AddStringToObject(rs485, "type", "USB");
    cJSON_AddStringToObject(rs485, "ifname", rs485_ifname);
    cJSON_AddItemToObject(cfg, "rs485", rs485);

    return cfg;
}

static int start_slave(uint8_t dev_idx, uint8_t itf_idx, int rpm_support)
{
    sprintf(rs485_ifname, "%d:%d", dev_idx, itf_idx);

    apdu_set_default_service_handler();
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_IS, NULL);
    if (rpm_support == 0) {
        apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_PROP_MULTIPLE, NULL);
    }

    if (bacnet_init() < 0) {
        fprintf(stderr, "bacnet init failed\r\n");
        return 1;
    }

    if (el_loop_start(&el_default_loop) < 0) {
        fprintf(stderr, "el loop start failed\r\n");
        return 1;
    }

    while(1)
        sleep(10);
}

static void term_handler(int signo)
{
    for (int i=0; i<subcount; ++i)
        kill(subprocess[i], signo);
    exit(0);
}

int main(int argc, char *argv[])
{
    unsigned long rs485_idx = 0, rpm_support = 0;
    char *endptr;

    if (argc != 1 && argc != 3) {
        fprintf(stderr, "Usage: slave_demo RS485_port_idx RPM_support\r\n");
        fprintf(stderr, "Example: slave_demo 0 1\r\n");
        fprintf(stderr, "means use first RS485 port, and support RPM\r\n");
        return 1;
    }

    if (argc != 1) {
        rs485_idx = strtoul(argv[1], &endptr, 0);
        if (rs485_idx == ULONG_MAX || *endptr != '\0') {
            fprintf(stderr, "Invalid RS485 port_idx\r\n");
            return 1;
        }

        rpm_support = strtoul(argv[2], &endptr, 0);
        if (rpm_support == ULONG_MAX || *endptr != '\0') {
            fprintf(stderr, "Invalid RPM_support\r\n");
            return 1;
        }
        rpm_support = rpm_support != 0;
    }

    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = term_handler;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    setvbuf(stdout, NULL, _IOLBF, 0);

    cJSON *usbinfo = usb_serial_query();
    if (usbinfo == NULL || usbinfo->type != cJSON_Array) {
        fprintf(stderr, "Get usb serial info failed\r\n");
        return 1;
    }

    cJSON *usbdev;
    cJSON_ArrayForEach(usbdev, usbinfo) {
        cJSON *index = cJSON_GetObjectItem(usbdev, "index");
        if (index == NULL || index->type != cJSON_Number) {
            fprintf(stderr, "Invalid index from usb serial info\r\n");
            return 1;
        }

        cJSON *snjson = cJSON_GetObjectItem(usbdev, "serial");
        if (snjson != NULL && snjson->type == cJSON_String) {
            serial_crc = crc32(serial_crc, (uint8_t*)snjson->valuestring, strlen(snjson->valuestring));
        } else {
            fprintf(stderr, "Invalid usb serial\r\n");
        }

        int devidx = index->valueint;

        cJSON *interfaces = cJSON_GetObjectItem(usbdev, "interface");
        if (interfaces == NULL || interfaces->type != cJSON_Array) {
            fprintf(stderr, "Invalid interface info from Device %d\r\n", index->valueint);
            return 1;
        }

        cJSON *itf;
        int itfidx = 0;
        cJSON_ArrayForEach(itf, interfaces) {
            if (itf->type != cJSON_Number) {
                fprintf(stderr, "Invalid subclass from Device %d\r\n", index->valueint);
                return 1;
            }

            if (itf->valueint == USB_INTERFACE_SUBCLASS_485) {
                if (argc == 1) {
                    pid_t pid = fork ();
                    if (pid == 0) {
                        start_slave(devidx, itfidx, (devidx + itfidx) & 1);
                    } else if (pid < 0) {
                        fprintf(stderr, "fork new process failed\r\n");
                        return 1;
                    } else {
                        subprocess[subcount++] = pid;
                        serial_crc >>= 16;
                    }
                } else if (rs485_idx == 0) {
                    start_slave(devidx, itfidx, rpm_support);
                } else {
                    rs485_idx--;
                }
            }

            itfidx++;
        }
    }

    if (argc != 1) {
        fprintf(stderr, "RS485 port not found\r\n");
        return 1;
    } else {
        while (1) sleep(10);
    }

    return 0;
}
