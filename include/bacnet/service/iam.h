/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * iam.h
 * Original Author:  linzhixian, 2015-2-11
 *
 * I Am Request Service
 *
 * History
 */

#ifndef _IAM_H_
#define _IAM_H_

#include <stdint.h>

#include "bacnet/bacdef.h"
#include "bacnet/bacenum.h"
#include "bacnet/bacnet_buf.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct BACnet_I_Am_Data {
    uint32_t device_id;
    uint32_t max_apdu;
    int segmentation;
    uint16_t vendor_id;
} BACNET_I_AM_DATA;

extern void handler_i_am(uint8_t *request, uint16_t request_len, bacnet_addr_t *src);

extern void Send_I_Am(bacnet_addr_t *dst);

extern void Send_I_Am_Remote(uint16_t net);

/**
 * Build_I_Am_Service - 构建i-am请求包
 */
extern void Build_I_Am_Service(bacnet_buf_t *buf, uint32_t device_id, uint32_t max_apdu,
        uint16_t vendor_id, BACNET_SEGMENTATION seg_support);

#ifdef __cplusplus
}
#endif

#endif  /* _IAM_H_ */

