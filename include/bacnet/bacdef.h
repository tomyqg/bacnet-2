/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bacdef.h
 * Original Author:  linzhixian, 2014-7-10
 *
 * BACnet协议头文件
 *
 * History
 */

#ifndef _BACDEF_H_
#define _BACDEF_H_

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef OK
#define OK                              (0)
#endif

#define MAX_MAC_LEN                     (7)
#define MAX_MAC_STR_LEN                 ((2 * MAX_MAC_LEN) + 1)
#define BACNET_BROADCAST_NETWORK        (0xFFFF)

#define BACNET_STATUS_OK                (0)
#define BACNET_STATUS_ERROR             (-1)
#define BACNET_STATUS_ABORT             (-2)
#define BACNET_STATUS_REJECT            (-3)

#define BACNET_INSTANCE_BITS            (22)
#define BACNET_MAX_OBJECT               (0x3FF)

/* Priority Array for commandable objects */
#define BACNET_NO_PRIORITY              (0)
#define BACNET_MIN_PRIORITY             (1)
#define BACNET_MAX_PRIORITY             (16)

/* Largest BACnet Instance Number */
/* Also used as a device instance number wildcard address */
#define BACNET_MAX_INSTANCE             (0x3FFFFF)
#define BACNET_ARRAY_ALL                (0xFFFFFFFFU)

#define RWLOCK_RDLOCK(lock)             (pthread_rwlock_rdlock(lock))
#define RWLOCK_WRLOCK(lock)             (pthread_rwlock_wrlock(lock))
#define RWLOCK_UNLOCK(lock)             (pthread_rwlock_unlock(lock))

#define BACNET_PROTOCOL_VERSION             (1)
#define BACNET_PROTOCOL_REVISION            (14)

#if (BACNET_PROTOCOL_REVISION == 0)
#define MAX_ASHRAE_OBJECT_TYPE              (18)
#define MAX_BACNET_SERVICES_SUPPORTED       (35)
#elif (BACNET_PROTOCOL_REVISION == 1)
#define MAX_ASHRAE_OBJECT_TYPE              (21)
#define MAX_BACNET_SERVICES_SUPPORTED       (37)
#elif (BACNET_PROTOCOL_REVISION == 2)
    /* from 135-2001 version of the BACnet Standard */
#define MAX_ASHRAE_OBJECT_TYPE              (23)
#define MAX_BACNET_SERVICES_SUPPORTED       (40)
#elif (BACNET_PROTOCOL_REVISION == 3)
#define MAX_ASHRAE_OBJECT_TYPE              (23)
#define MAX_BACNET_SERVICES_SUPPORTED       (40)
#elif (BACNET_PROTOCOL_REVISION == 4)
    /* from 135-2004 version of the BACnet Standard */
#define MAX_ASHRAE_OBJECT_TYPE              (25)
#define MAX_BACNET_SERVICES_SUPPORTED       (40)
#elif (BACNET_PROTOCOL_REVISION == 5)
#define MAX_ASHRAE_OBJECT_TYPE              (30)
#define MAX_BACNET_SERVICES_SUPPORTED       (40)
#elif (BACNET_PROTOCOL_REVISION == 6)
#define MAX_ASHRAE_OBJECT_TYPE              (31)
#define MAX_BACNET_SERVICES_SUPPORTED       (40)
#elif (BACNET_PROTOCOL_REVISION == 7)
#define MAX_ASHRAE_OBJECT_TYPE              (31)
#define MAX_BACNET_SERVICES_SUPPORTED       (40)
#elif (BACNET_PROTOCOL_REVISION == 8)
#define MAX_ASHRAE_OBJECT_TYPE              (31)
#define MAX_BACNET_SERVICES_SUPPORTED       (40)
#elif (BACNET_PROTOCOL_REVISION == 9)
    /* from 135-2008 version of the BACnet Standard */
#define MAX_ASHRAE_OBJECT_TYPE              (38)
#define MAX_BACNET_SERVICES_SUPPORTED       (40)
#elif (BACNET_PROTOCOL_REVISION == 10)
#define MAX_ASHRAE_OBJECT_TYPE              (51)
#define MAX_BACNET_SERVICES_SUPPORTED       (40)
#elif (BACNET_PROTOCOL_REVISION == 11)
#define MAX_ASHRAE_OBJECT_TYPE              (51)
#define MAX_BACNET_SERVICES_SUPPORTED       (40)
#elif (BACNET_PROTOCOL_REVISION == 12)
    /* from 135-2010 version of the BACnet Standard */
#define MAX_ASHRAE_OBJECT_TYPE              (51)
#define MAX_BACNET_SERVICES_SUPPORTED       (40)
#elif (BACNET_PROTOCOL_REVISION == 13)
#define MAX_ASHRAE_OBJECT_TYPE              (53)
#define MAX_BACNET_SERVICES_SUPPORTED       (40)
#elif (BACNET_PROTOCOL_REVISION == 14)
    /* from 135-2012 version of the BACnet Standard */
#define MAX_ASHRAE_OBJECT_TYPE              (55)
#define MAX_BACNET_SERVICES_SUPPORTED       (41)
#else
#error MAX_ASHRAE_OBJECT_TYPE and MAX_BACNET_SERVICES_SUPPORTED not defined!
#endif

/* 报文优先级 */
typedef enum bacnet_prio_s {
    PRIORITY_NORMAL = 0,
    PRIORITY_URGENT = 1,
    PRIORITY_CRITICAL_EQUIPMENT = 2,
    PRIORITY_LIFE_SAFETY = 3
} bacnet_prio_t;

#define NETWORK_PRIO_IS_VALID(prio)     (((prio) >= PRIORITY_NORMAL) && ((prio) <= PRIORITY_LIFE_SAFETY))

/* bacnet设备地址 */
typedef struct bacnet_addr_s {
    uint16_t net;                   /* BACnet network number */
    /* LEN = 0 denotes broadcast MAC ADR and ADR field is absent */
    /* LEN > 0 specifies length of ADR field */
    uint8_t len;                    /* length of MAC address */
    uint8_t adr[MAX_MAC_LEN];       /* hwaddr (MAC) address */
} bacnet_addr_t;

static inline bool address_equal(const bacnet_addr_t *src, const bacnet_addr_t *dst)
{
    return memcmp(src, dst, &src->adr[src->len] - (uint8_t *)src) == 0;
}

#define PRINT_BACNET_ADDRESS(addr)                                      \
do {                                                                    \
    int mac_len;                                                        \
    printf("Net: %d, Len: %d, Mac(0X): ", (addr)->net, (addr)->len);    \
    for (mac_len = 0; mac_len < (addr)->len; mac_len++) {               \
        printf("%02X.", (addr)->adr[mac_len]);                          \
    }                                                                   \
    printf("\r\n");                                                     \
} while (0)                             

#ifdef __cplusplus
}
#endif

#endif  /* _BACDEF_H_ */

