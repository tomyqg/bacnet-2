/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * debug_test.c
 * Original Author:  linzhixian, 2015-12-8
 *
 * Debug Demo
 *
 * History
 */

#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "debug.h"
#include "misc/cJSON.h"
#include "connect_mng.h"
#include "bacnet/bacint.h"

#define DEBUG_IF_NO_ARGUMENT_RETURN(a)                                      \
do {                                                                        \
    if ((a) < 1) {                                                          \
        printf("please input more arguments\r\n");                          \
        return;                                                             \
    }                                                                       \
} while (0)

#define DEBUG_IF_LESS_ARGUMENT_RETURN(a, b)                                 \
do {                                                                        \
    if ((a) < (b)) {                                                        \
        printf("please input %d more arguments\r\n", (b) - (a));            \
        return;                                                             \
    }                                                                       \
} while (0)

#define DEBUG_IF_MORE_ARGUMENT_RETURN(a, b)                                 \
do {                                                                        \
    if ((a) > (b)) {                                                        \
        printf("please input %d less arguments\r\n", (a) - (b));            \
        return;                                                             \
    }                                                                       \
} while (0)

static int client_fd = -1;

static int debug_init(void)
{
    struct sockaddr_un serv_addr;
    int rv;
    
    client_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (client_fd < 0) {
        printf("debug_connect: create socket failed cause %s\r\n", strerror(errno));
        return client_fd;
    }

    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sun_family = AF_LOCAL;
    (void)strncpy(serv_addr.sun_path, SERVER_UNIXDG_PATH, sizeof(serv_addr.sun_path) - 1);

    rv = connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (rv < 0) {
        printf("debug_connect: connect %s failed cause %s\r\n", SERVER_UNIXDG_PATH, strerror(errno));
        close(client_fd);
    }

    return rv;
}

static void debug_exit(void)
{
    close(client_fd);
    client_fd = -1;
}

static void debug_send_request(uint32_t choice, const char *argv)
{
    cJSON *request;
    long int value;
    char *data;
    ssize_t data_len;
    uint8_t *pkt;
    uint32_t pkt_len;
    char *endptr = NULL;
    
    request = cJSON_CreateObject();
    if (request == NULL) {
        printf("debug_send_request: create request object failed\r\n");
        return;
    }

    cJSON_AddNumberToObject(request, "request", choice);
    
    switch (choice) {
    case DEBUG_SET_DEBUG_DBG_STATUS:
    case DEBUG_SET_CONNECT_MNG_DBG_STATUS:
    case DEBUG_SET_WEB_DBG_STATUS:
    case DEBUG_SET_APP_DBG_STATUS:
    case DEBUG_SET_NETWORK_DBG_STATUS:
    case DEBUG_SET_DATALINK_DBG_STATUS:
    case DEBUG_SET_MSTP_DBG_STATUS:
    case DEBUG_SET_BIP_DBG_STATUS:
    case DEBUG_SET_ETHERNET_DBG_STATUS:
        if (argv == NULL) {
            printf("debug_send_request: null dbg level\r\n");
            goto out;
        }
        
        value = strtol(argv, &endptr, 0);
        if ((endptr != NULL) && (*endptr != '\0')) {
            printf("dbg level value should be numberical in [%d, %d]\r\n", MIN_DBG_LEVEL,
                MAX_DBG_LEVEL);
            goto out;
        }
        
        if ((value < MIN_DBG_LEVEL) || (value > MAX_DBG_LEVEL)) {
            printf("dbg level value should be in [%d, %d]\r\n", MIN_DBG_LEVEL, MAX_DBG_LEVEL);
            goto out;
        }

        cJSON_AddNumberToObject(request, "dbg_level", value);
        
        break;
    
    case DEBUG_SHOW_NETWORK_ROUTE_TABLE:
        /* do nothing */
        break;
    
    default:
        printf("debug_send_request: invalid debug service choice(%d)\r\n", choice);
        goto out;
    }

    data = cJSON_Print(request);
    if (data == NULL) {
        printf("debug_send_request: cJSON Print failed\r\n");
        goto out;
    }

    data_len = strlen(data) + 1;
    pkt_len = 8 + data_len;
    pkt = (uint8_t *)malloc(pkt_len);
    if (pkt == NULL) {
        printf("debug_send_request: malloc %d bytes failed\r\n", pkt_len);
        goto out1;
    }

    (void)encode_unsigned32(pkt, BACNET_DEBUG_SERVICE);
    (void)encode_unsigned32(pkt + 4, data_len);
    memcpy(pkt + 8, data, data_len);
        
    if (write(client_fd, pkt, pkt_len) != pkt_len) {
        printf("debug_send_request: write data failed\r\n");
    }
    free(pkt);

out1:
    free(data);

out:
    cJSON_Delete(request);
    
    return;
}

static void debug_app_parse(int argc, const char *argv[])
{
    int i;

    i = 0;
    DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
    
    if (strcmp(argv[i], "set") == 0) {
        i++;
        DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
        if (strcmp(argv[i], "dbg") == 0) {
            i++;
            DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
            DEBUG_IF_MORE_ARGUMENT_RETURN(argc - i, 1);
            debug_send_request(DEBUG_SET_APP_DBG_STATUS, argv[i]);
            return;
        }
    } else {
        /* do nothing */
    }

    printf("invalid argument: %s\r\n", argv[i]);
}

static void debug_network_parse(int argc, const char *argv[])
{
    int i;

    i = 0;
    DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);

    if (strcmp(argv[i], "set") == 0) {
        i++;
        DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
        if (strcmp(argv[i], "dbg") == 0) {
            i++;
            DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
            DEBUG_IF_MORE_ARGUMENT_RETURN(argc - i, 1);
            debug_send_request(DEBUG_SET_NETWORK_DBG_STATUS, argv[i]);
            return;
        }
    } else if (strcmp(argv[i], "show") == 0) {
        i++;
        DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
        if (strcmp(argv[i], "route_table") == 0) {
            i++;
            DEBUG_IF_MORE_ARGUMENT_RETURN(argc - i, 0);
            debug_send_request(DEBUG_SHOW_NETWORK_ROUTE_TABLE, NULL);
            return;
        }
    } else {
        /* do nothing */
    }

    printf("invalid argument: %s\r\n", argv[i]);
}

static void debug_datalink_parse(int argc, const char *argv[])
{
    int i;

    i = 0;
    DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
    
    if (strcmp(argv[i], "set") == 0) {
        i++;
        DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
        if (strcmp(argv[i], "dbg") == 0) {
            i++;
            DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
            DEBUG_IF_MORE_ARGUMENT_RETURN(argc - i, 1);
            debug_send_request(DEBUG_SET_DATALINK_DBG_STATUS, argv[i]);
            return;
        }
    } else {
        /* do nothing */
    }

    printf("invalid argument: %s\r\n", argv[i]);
}

static void debug_mstp_parse(int argc, const char *argv[])
{
    int i;

    i = 0;
    DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
    
    if (strcmp(argv[i], "set") == 0) {
        i++;
        DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
        if (strcmp(argv[i], "dbg") == 0) {
            i++;
            DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
            DEBUG_IF_MORE_ARGUMENT_RETURN(argc - i, 1);
            debug_send_request(DEBUG_SET_MSTP_DBG_STATUS, argv[i]);
            return;
        }
    } else {
        /* do nothing */
    }

    printf("invalid argument: %s\r\n", argv[i]);
}

static void debug_bip_parse(int argc, const char *argv[])
{
    int i;

    i = 0;
    DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
    
    if (strcmp(argv[i], "set") == 0) {
        i++;
        DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
        if (strcmp(argv[i], "dbg") == 0) {
            i++;
            DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
            DEBUG_IF_MORE_ARGUMENT_RETURN(argc - i, 1);
            debug_send_request(DEBUG_SET_BIP_DBG_STATUS, argv[i]);
            return;
        }
    } else {
        /* do nothing */
    }

    printf("invalid argument: %s\r\n", argv[i]);
}

static void debug_ethernet_parse(int argc, const char *argv[])
{
    int i;

    i = 0;
    DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
    
    if (strcmp(argv[i], "set") == 0) {
        i++;
        DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
        if (strcmp(argv[i], "dbg") == 0) {
            i++;
            DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
            DEBUG_IF_MORE_ARGUMENT_RETURN(argc - i, 1);
            debug_send_request(DEBUG_SET_ETHERNET_DBG_STATUS, argv[i]);
            return;
        }
    } else {
        /* do nothing */
    }

    printf("invalid argument: %s\r\n", argv[i]);
}

static void debug_connect_mng_parse(int argc, const char *argv[])
{
    int i;

    i = 0;
    DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
    
    if (strcmp(argv[i], "set") == 0) {
        i++;
        DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
        if (strcmp(argv[i], "dbg") == 0) {
            i++;
            DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
            DEBUG_IF_MORE_ARGUMENT_RETURN(argc - i, 1);
            debug_send_request(DEBUG_SET_CONNECT_MNG_DBG_STATUS, argv[i]);
            return;
        }
    } else {
        /* do nothing */
    }

    printf("invalid argument: %s\r\n", argv[i]);
}

static void debug_web_parse(int argc, const char *argv[])
{
    int i;

    i = 0;
    DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
    
    if (strcmp(argv[i], "set") == 0) {
        i++;
        DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
        if (strcmp(argv[i], "dbg") == 0) {
            i++;
            DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
            DEBUG_IF_MORE_ARGUMENT_RETURN(argc - i, 1);
            debug_send_request(DEBUG_SET_WEB_DBG_STATUS, argv[i]);
            return;
        }
    } else {
        /* do nothing */
    }

    printf("invalid argument: %s\r\n", argv[i]);
}

static void debug_debug_parse(int argc, const char *argv[])
{
    int i;

    i = 0;
    DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
    
    if (strcmp(argv[i], "set") == 0) {
        i++;
        DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
        if (strcmp(argv[i], "dbg") == 0) {
            i++;
            DEBUG_IF_NO_ARGUMENT_RETURN(argc - i);
            DEBUG_IF_MORE_ARGUMENT_RETURN(argc - i, 1);
            debug_send_request(DEBUG_SET_DEBUG_DBG_STATUS, argv[i]);
            return;
        }
    } else {
        /* do nothing */
    }

    printf("invalid argument: %s\r\n", argv[i]);
}

static void debug_parse(int argc, const char *argv[])
{
    DEBUG_IF_NO_ARGUMENT_RETURN(argc);

    if (strcmp(argv[0], "app") == 0) {
        debug_app_parse(argc - 1, &argv[1]);
    } else if (strcmp(argv[0], "network") == 0) {
        debug_network_parse(argc - 1, &argv[1]);
    } else if (strcmp(argv[0], "datalink") == 0) {
        debug_datalink_parse(argc - 1, &argv[1]);
    } else if (strcmp(argv[0], "mstp") == 0) {
        debug_mstp_parse(argc - 1, &argv[1]);
    } else if (strcmp(argv[0], "bip") == 0) {
        debug_bip_parse(argc - 1, &argv[1]);
    } else if (strcmp(argv[0], "ethernet") == 0) {
        debug_ethernet_parse(argc - 1, &argv[1]);
    } else if (strcmp(argv[0], "connect_mng") == 0) {
        debug_connect_mng_parse(argc - 1, &argv[1]);
    } else if (strcmp(argv[0], "web") == 0) {
        debug_web_parse(argc - 1, &argv[1]);
    } else if (strcmp(argv[0], "debug") == 0) {
        debug_debug_parse(argc - 1, &argv[1]);
    } else {
        printf("invalid argument: %s\r\n", argv[0]);
    }

    return;
}

int main(int argc, const char *argv[])
{
    int rv;

    rv = debug_init();
    if (rv < 0) {
        printf("debug init failed(%d)\r\n", rv);
        goto out;
    }

    debug_parse(argc - 1, &argv[1]);
    
    debug_exit();
    
out:

    exit(0);
}

