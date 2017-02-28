/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * whois.h
 * Original Author:  linzhixian, 2015-2-11
 *
 * Who Is Request Service
 *
 * History
 */

#ifndef _WHOIS_H_
#define _WHOIS_H_

#include <stdint.h>

#include "bacnet/bacdef.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * whois_decode_service_request
 * @param pdu
 * @param pdu_len
 * @param pLow_limit
 * @param pHigh_limit
 * @return < 0 error
 */
extern int whois_decode_service_request(uint8_t *pdu, uint16_t pdu_len, uint32_t *pLow_limit,
            uint32_t *pHigh_limit);

extern void handler_who_is(uint8_t *request, uint16_t request_len, bacnet_addr_t *src);

extern void Send_WhoIs_Local(int32_t low_limit, int32_t high_limit);

extern void Send_WhoIs_Global(int32_t low_limit, int32_t high_limit);

extern void Send_WhoIs_Remote(uint16_t net, int32_t low_limit, int32_t high_limit);

extern void Send_WhoIs(int32_t low_limit, int32_t high_limit);

#ifdef __cplusplus
}
#endif

#endif  /* _WHOIS_H_ */
