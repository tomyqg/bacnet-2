/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * web_server_test.c
 * Original Author:  linzhixian, 2015-11-18
 *
 * Web Server
 *
 * History
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "bacnet/bacnet.h"
#include "bacnet/apdu.h"
#include "misc/eventloop.h"
#include "web_service.h"
#include "connect_mng.h"
#include "debug.h"

int main(void)
{
    int rv;
    
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
    
    rv = web_service_init();
    if (rv < 0) {
        printf("web service init failed(%d)\r\n", rv);
        goto out2;
    }

    apdu_set_default_service_handler();
    rv = el_loop_start(&el_default_loop);
    if (rv < 0) {
        printf("el loop start failed(%d)\r\n", rv);
        goto out3;
    }

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
    
    return 0;
}

