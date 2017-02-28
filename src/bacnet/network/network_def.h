/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * network_dbg.h
 * Original Author:  linzhixian, 2014-7-7
 *
 * BACnet网络层内部调试头文件
 *
 * History
 */

#ifndef _NETWORK_DEF_H_
#define _NETWORK_DEF_H_

#include "route.h"

extern bool network_dbg_err;
extern bool network_dbg_warn;
extern bool network_dbg_verbos;

#define INTERVAL_Who_Is_Router_To_Network       (10)

#define NETWORK_ERROR(fmt, args...)             \
do {                                            \
    if (network_dbg_err) {                      \
       printf(fmt, ##args);                     \
    }                                           \
} while (0)

#define NETWORK_WARN(fmt, args...)              \
do {                                            \
    if (network_dbg_warn) {                     \
        printf(fmt, ##args);                    \
    }                                           \
} while (0)

#define NETWORK_VERBOS(fmt, args...)            \
do {                                            \
    if (network_dbg_verbos) {                   \
       printf(fmt, ##args);                     \
    }                                           \
} while (0)

#define NETWORK_IF_ERROR_RETURN(func)           \
do {                                            \
    int rv = -EPERM;                            \
    if (func) {                                 \
        return rv;                              \
    }                                           \
} while (0)

extern int network_relay_handler(bacnet_port_t *in_port, bacnet_addr_t *src_addr, 
            bacnet_buf_t *npdu, npci_info_t *npci_info);

#endif  /* _NETWORK_DEF_H_ */

