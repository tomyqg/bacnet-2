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
#include <sysexits.h>

#include "bacnet/bacnet.h"
#include "bacnet/app.h"
#include "bacnet/network.h"
#include "bacnet/datalink.h"
#include "bacnet/mstp.h"
#include "bacnet/bip.h"
#include "bacnet/etherdl.h"
#include "misc/eventloop.h"
#include "connect_mng.h"
#include "web_service.h"
#include "debug.h"

int main(int argc, char *argv[])
{
    int rv;

    setvbuf(stdout, NULL, _IOLBF, 0);

    rv = bacnet_init();
    if (rv < 0) {
        printf("bacnet init failed(%d)\r\n", rv);
        return EX_CONFIG;
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

    rv = web_service_init();
    if (rv < 0) {
        printf("web service init failed(%d)\r\n", rv);
        goto out2;
    }

    rv = el_loop_start(&el_default_loop);
    if (rv < 0) {
        printf("el loop start failed(%d)\r\n", rv);
        goto out3;
    }

    app_set_dbg_level(4);
    network_set_dbg_level(6);
    datalink_set_dbg_level(6);
    mstp_set_dbg_level(6);
    bip_set_dbg_level(6);
    ether_set_dbg_level(6);
    
    while (1) {
        sleep(10);
    }

out3:
    web_service_exit();

out2:
    debug_service_exit();

out1:
    connect_mng_exit();

out0:
    bacnet_exit();

    return 1;
}

