/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * test.c
 * Original Author:  linzhixian, 2014-8-27
 *
 * BACnet网络层测试
 *
 * History
 */

/* ./test 2 & */
/* ./test 1 0 6 192.168.0.103 47808 2 & */
/* ./test 1 0 0 2 & */
/* ./test 3 192.168.0.103 47808 2 & */
/* ./test 4 192.168.0.103 47808 2 & */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "bacnet/config.h"
#include "bacnet/bacint.h"
#include "bacnet/bacnet.h"
#include "bacnet/network.h"
#include "bacnet/bacnet_buf.h"
#include "misc/eventloop.h"

extern void bip_set_dbg_level(uint32_t level);
extern int bvlc_send_read_bdt(uint32_t port_id, struct sockaddr_in *dst_bbmd);
extern int bvlc_send_read_fdt(uint32_t port_id, struct sockaddr_in *dst_bbmd);

uint16_t dnet = 0;
uint8_t dlen = 0;
uint8_t dmac[MAX_MAC_LEN];

static int Test_network_send_pdu(void)
{
    DECLARE_BACNET_BUF(apdu, MAX_APDU);
    bacnet_addr_t dst;
    bacnet_prio_t prio;
    bool der;
    int rv;
    struct timeval start, end;
    uint32_t tx_time;

    bacnet_buf_init(&apdu.buf, MAX_APDU);
    memset(apdu.buf.data, 0x11, 100);
    apdu.buf.data_len = 100;

    prio = 0;
    der = false;
    
    memset(&dst, 0, sizeof(bacnet_addr_t));
    dst.net = dnet;
    dst.len = dlen;
    memcpy(&dst.adr[0], &dmac[0], dlen);

    (void)gettimeofday(&start, NULL);
    rv = network_send_pdu(&dst, &apdu.buf, prio, der);
    (void)gettimeofday(&end, NULL);
    
    if (rv < 0) {
        printf("Test_network_send_pdu: failed(%d)\r\n", rv);
        return rv;
    }

    tx_time = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
    printf("Test_network_send_pdu: tx_time = %d us\r\n\r\n", tx_time);

    return rv;
}

int main(int argc, char *argv[])
{
    int mode;
    int tx_count;
    struct sockaddr_in dst_bbmd;
    struct in_addr dst_ip;
    uint16_t port = 0;
    int i;
    int rv;
    
    mode = 0;
    tx_count = 0;
    
    if (argc < 2) {
        printf("invalid argc(%d)\r\n", argc);
        return -1;
    }

    mode = atoi(argv[1]);
    if (mode == 2) {
        /* 收帧模式 */
        if (argc != 2) {
            printf("invalid argc(%d) for receive mode\r\n", argc);
            return -1;
        }
    } else if ((mode == 3) || (mode == 4)) {
        /* 发送read BDT/FDT Msg */
        if (argc != 5) {
            printf("invalid argc(%d)\r\n", argc);
            return -1;
        }
        
        rv = inet_aton(argv[2], &dst_ip);
        if (rv == 0) {
            printf("invalid ip address\r\n");
            return -1;
        }
        port = atoi(argv[3]);
        port = htons(port);
        tx_count = atoi(argv[4]);
    } else if (mode == 5) {
    } else if (mode == 6) {
    } else if (mode == 1) {
        /* 广播NPDU */
        if (argc == 5) {
            dnet = atoi(argv[2]);
            dlen = atoi(argv[3]);
            if (dlen != 0) {
                printf("error: bcast address length must be 0\r\n");
                return -1;
            }
            tx_count = atoi(argv[4]);
        } else if (argc == 6) {
            /* MSTP单播NPDU */
            dnet = atoi(argv[2]);
            dlen = atoi(argv[3]);
            if (dlen != 1) {
                printf("invalid dlen(%d) for mstp\r\n", dlen);
                return -1;
            }
            dmac[0] = atoi(argv[4]);
            tx_count = atoi(argv[5]);
        } else if (argc == 7) {
            /* BIP单播NPDU */
            dnet = atoi(argv[2]);
            dlen = atoi(argv[3]);
            if (dlen != 6) {
                printf("invalid dlen(%d) for bip\r\n", dlen);
                return -1;
            }
            rv = inet_aton(argv[4], &dst_ip);
            if (rv == 0) {
                printf("invalid ip address\r\n");
                return -1;
            }

            port = atoi(argv[5]);
            port = htons(port);
            
            memcpy(&dmac[0], &dst_ip, 4);
            memcpy(&dmac[4], &port, 2);
            tx_count = atoi(argv[6]);
        } else {
            printf("invalid argc(%d) for mode(%d)\r\n", argc, mode);
            return -1;
        }        
    } else {
        printf("invalid mode(%d)\r\n", mode);
        return -1;
    }

    printf("\r\nDmac: ");
    for (i = 0; i < dlen; i++) {
        printf("%02x ", dmac[i]);
    }
    printf("\r\n");

    network_set_dbg_level(6);
    bip_set_dbg_level(6);
    
    rv = bacnet_init();
    if (rv < 0) {
        printf("bacnet init failed(%d)\r\n", rv);
        return rv;
    }
    
    rv = el_loop_start(&el_default_loop);
    if (rv < 0) {
        printf("el loop start failed(%d)\r\n", rv);
        goto out;
    }

    switch (mode) {
    case 1:
        while (tx_count--) {
            (void)Test_network_send_pdu();
            sleep(1);
        }
        break;
    
    case 2:
        break;

    case 3:
        dst_bbmd.sin_port = port;
        dst_bbmd.sin_addr.s_addr = dst_ip.s_addr;

        while (tx_count--) {
            rv = bvlc_send_read_bdt(0, &dst_bbmd);
            if (rv < 0) {
                printf("send_read_bdt: failed(%d)\r\n", rv);
            }
        }
        break;

    case 4:
        dst_bbmd.sin_port = port;
        dst_bbmd.sin_addr.s_addr = dst_ip.s_addr;

        while (tx_count--) {
            rv = bvlc_send_read_fdt(0, &dst_bbmd);
            if (rv < 0) {
                printf("send_read_fdt: failed(%d)\r\n", rv);
            }
        }
        break;

    case 5:
        break;

    case 6:
        break;
    
    default:
        printf("invalid mode(%d)\r\n", mode);
        break;
    }

    while (1) {
        sleep(10);
    }

out:
    bacnet_exit();
    
    return 0;
}

