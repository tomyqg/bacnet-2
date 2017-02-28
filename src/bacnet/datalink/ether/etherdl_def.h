/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * etherdl_def.h
 * Original Author:  linzhixian, 2015-1-15
 *
 * Ethernet
 *
 * History
 */

#ifndef _ETHERDL_DEF_H_
#define _ETHERDL_DEF_H_

#include <stdio.h>
#include <stdbool.h>

extern bool ether_dbg_verbos;
extern bool ether_dbg_warn;
extern bool ether_dbg_err;

#define ETH_ERROR(fmt, args...)                     \
do {                                                \
    if (ether_dbg_err) {                            \
       printf(fmt, ##args);                         \
    }                                               \
} while (0)

#define ETH_WARN(fmt, args...)                      \
do {                                                \
    if (ether_dbg_warn) {                           \
        printf(fmt, ##args);                        \
    }                                               \
} while (0)

#define ETH_VERBOS(fmt, args...)                    \
do {                                                \
    if (ether_dbg_verbos) {                         \
       printf(fmt, ##args);                         \
    }                                               \
} while (0)

#define MAX_ETH_PDU                 (1500)
#define ETH_802_3_HEADER            (14)
#define MAX_ETH_802_3_LEN           (MAX_ETH_PDU + ETH_802_3_HEADER)

#define ETH_MPDU_HEADER             (3)
#define MAX_ETH_NPDU                (MAX_ETH_PDU - ETH_MPDU_HEADER)
#define ETH_MPDU_ALL_HEADER         (ETH_802_3_HEADER + ETH_MPDU_HEADER)

#endif /* _ETHERDL_DEF_H_ */

