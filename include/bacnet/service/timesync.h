/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * timesync.h
 * Original Author:  linzhixian, 2015-1-15
 *
 * 
 *
 * History
 */

#ifndef _TIMESYNC_H_
#define _TIMESYNC_H_

#include <stdint.h>

#include "bacnet/bacdef.h"
#include "bacnet/datetime.h"
#include "bacnet/bacnet_buf.h"

#ifdef __cplusplus
extern "C"
{
#endif

extern void handler_timesync(uint8_t *request, uint16_t request_len, bacnet_addr_t *src);

extern void handler_timesync_utc(uint8_t *request, uint16_t request_len, bacnet_addr_t *src);

extern int Send_TimeSync_Remote(bacnet_addr_t *dst, BACNET_DATE *bdate, BACNET_TIME *btime);

extern int Send_TimeSync(BACNET_DATE *bdate, BACNET_TIME *btime);

extern int Send_TimeSyncUTC(BACNET_DATE *bdate, BACNET_TIME *btime);

#ifdef __cplusplus
}
#endif

#endif  /* _TIMESYNC_H_ */

