/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * rr.h
 * Original Author:  linzhixian, 2016-5-26
 *
 * Read Range
 *
 * History
 */

#ifndef _RR_H_
#define _RR_H_

#include <stdint.h>

#include "bacnet/apdu.h"
#include "bacnet/datetime.h"
#include "bacnet/bacenum.h"
#include "bacnet/bacstr.h"
#include "bacnet/bacnet_buf.h"
#include "bacnet/service/rp.h"
#include "bacnet/tsm.h"

typedef enum {
    RR_BY_POSITION = 0,
    RR_BY_SEQUENCE = 1,
    RR_BY_TIME = 2,
    RR_READ_ALL = 3
} RR_TYPE;

typedef enum {
    RESULT_FLAG_FIRST_ITEM = 0,
    RESULT_FLAG_LAST_ITEM = 1,
    RESULT_FLAG_MORE_ITEMS = 2
} BACNET_RESULT_FLAGS;

typedef struct RR_Range {
    RR_TYPE RequestType;                /* Index, sequence or time based request. */
    union {
        uint32_t RefIndex;
        uint32_t RefSeqNum;
        BACNET_DATE_TIME RefTime;
    } Range;
    int Count;                          /* SIGNED value as +ve vs -ve  is important. */
} RR_RANGE;

typedef struct BACnet_Read_Range_Data {
    BACNET_READ_PROPERTY_DATA rpdata;
    RR_RANGE Range;
    BACNET_BIT_STRING ResultFlags;      /* FIRST_ITEM, LAST_ITEM, MORE_ITEMS. */ 
    uint32_t ItemCount;
    uint32_t FirstSequence;
} BACNET_READ_RANGE_DATA;

extern int rr_ack_decode(uint8_t *apdu, uint16_t apdu_len, BACNET_READ_RANGE_DATA *rrdata);

extern void handler_read_range(BACNET_CONFIRMED_SERVICE_DATA *service_data, bacnet_buf_t *reply_apdu, 
                bacnet_addr_t *src);

extern int rr_encode_service_request(bacnet_buf_t *apdu, uint8_t invoke_id,
            BACNET_READ_RANGE_DATA *rrdata);

extern int Send_ReadRange_Request(tsm_invoker_t *invoker, BACNET_READ_RANGE_DATA *rrdata);

#endif /* _RR_H_ */

