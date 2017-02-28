/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * web_client_test.c
 * Original Author:  linzhixian, 2015-11-18
 *
 * Web Client
 *
 * History
 */

#include <stdint.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "misc/eventloop.h"
#include "connect_mng.h"
#include "web_service.h"
#include "bacnet/bacnet.h"
#include "misc/cJSON.h"

static void client_callback(connect_client_async_t *connect, connect_client_state state)
{
    switch(state) {
    case CONNECT_CLIENT_CONNECTED:
        break;
    case CONNECT_CLIENT_SENT:
        free(connect->data);
        break;
    case CONNECT_CLIENT_OK:
        printf("%s\r\n", connect->data);
        exit(0);
        break;
    case CONNECT_CLIENT_CONNECT_FAILED:
    case CONNECT_CLIENT_SEND_FAIL:
        free(connect->data);
        goto print_error;
    default:
print_error:
        fprintf(stderr, "client_callback: failed(%d)\r\n", state);
        exit(1);
        break;
    }
}

static void web_async_request(uint8_t *data, uint32_t data_len)
{
    el_sync(&el_default_loop);
    connect_client_async_t *connect = connect_client_async_create(&el_default_loop, -1);
    if (connect == NULL) {
        printf("web_async_request: connect client async create failed\r\n");
        free(data);
        el_unsync(&el_default_loop);
        return;
    }

    connect->type = BACNET_WEB_SERVICE;
    connect->data = (uint8_t*)data;
    connect->len = data_len;
    connect->callback = client_callback;
    el_unsync(&el_default_loop);
    return;
}

static void web_send_request(uint8_t *data, uint32_t data_len)
{
    connect_client_state state;
    uint8_t *rsp;
    uint32_t rsp_len;

    state = connect_client(BACNET_WEB_SERVICE, data, data_len, &rsp, &rsp_len, -1);
    if (state != CONNECT_CLIENT_OK) {
        printf("web_send_request: sync request failed(%d)\r\n", (int)state);
    } else if (rsp != NULL) {
        printf("%s\r\n", rsp);
        free(rsp);
    }

    free(data);
}

int main(int argc, const char *argv[])
{
    if (argc > 2) {
        fprintf(stderr, "Usage: web_client_test [a]\r\n");
        fprintf(stderr, "a: async mode");
        return 1;
    }

    if (argc > 1 && strcmp(argv[1], "a") != 0) {
        fprintf(stderr, "unknown arugment: %s\r\n", argv[1]);
        return 1;
    }

    uint32_t data_size = 4096;
    uint8_t *data = malloc(data_size);
    uint32_t data_len = 0;
    if (data == NULL) {
        fprintf(stderr, "not enough memory\r\n");
        return 1;
    }

    for (;;) {
        int rv = fread(data + data_len, 1, data_size - 1 - data_len, stdin);
        data_len += rv;
        if (ferror(stdin)) {
            fprintf(stderr, "read failed\r\n");
            return 1;
        }
        if (feof(stdin)) {
            data[data_len] = 0;
            data_len++;
            break;
        }
        if (data_len == data_size - 1) {
            data_size *= 2;
            data = realloc(data, data_size);
            if (data == NULL) {
                fprintf(stderr, "not enough memory\r\n");
                return 1;
            }
        }
    }

    if (argc == 1) {
        web_send_request(data, data_len);
    } else {
        el_loop_init(&el_default_loop);
        el_loop_start(&el_default_loop);
        web_async_request(data, data_len);
        for (;;) sleep(1);
    }

    return 0;
}

