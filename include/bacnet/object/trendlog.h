/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * trendlog.h
 * Original Author:  linzhixian, 2015-2-2
 *
 * History
 */

#ifndef _TRENDLOG_H_
#define _TRENDLOG_H_

#include <stdint.h>
#include <stdbool.h>

#include "bacnet/datetime.h"
#include "bacnet/bacdevobjpropref.h"
#include "bacnet/bacstr.h"
#include "bacnet/object/object.h"
#include "misc/cJSON.h"
#include "misc/list.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* Error code for Trend Log storage */
typedef struct tl_error {
    uint16_t usClass;
    uint16_t usCode;
} TL_ERROR;

/* Bit string of up to 32 bits for Trend Log storage */
typedef struct tl_bits {
    uint8_t ucLen;          /* bytes used in upper nibble/bits free in lower nibble */
    uint8_t ucStore[4];
} TL_BITS;

/* Storage structure for Trend Log data */
typedef struct tl_data_record {
    time_t tTimeStamp;              /* When the event occurred */
    uint8_t ucRecType;              /* What type of Event */
    uint8_t ucStatus;               /* Optional Status for read value in b0-b2, b7 = 1 if status is used */
    union {
        uint8_t ucLogStatus;        /* Change of log state flags */
        uint8_t ucBoolean;          /* Stored boolean value */
        float fReal;                /* Stored floating point value */
        uint32_t ulEnum;            /* Stored enumerated value - max 32 bits */
        uint32_t ulUValue;          /* Stored unsigned value - max 32 bits */
        int32_t lSValue;            /* Stored signed value - max 32 bits */
        TL_BITS Bits;               /* Stored bitstring - max 32 bits */
        TL_ERROR Error;             /* Two part error class/code combo */
        float fTime;                /* Interval value for change of time - seconds */
    } Datum;
} TL_DATA_REC;

typedef struct object_tl_s {
    object_seor_t base;
    bool bEnable;                           /* Trend log is active when this is true */
    BACNET_DATE_TIME StartTime;             /* BACnet format start time */
    BACNET_DATE_TIME StopTime;              /* BACnet format stop time */
    time_t tStartTime;                      /* Local time working copy of start time */
    time_t tStopTime;                       /* Local time working copy of stop time */
    uint8_t ucTimeFlags;                    /* Shorthand info on times */
    BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE Source;     /* Where the data comes from */
    uint32_t ulLogInterval;                 /* Time between entries in seconds */
    bool bStopWhenFull;                     /* Log halts when full if true */
    uint32_t ulRecordCount;                 /* Count of items currently in the buffer */
    uint32_t ulTotalRecordCount;            /* Count of all items that have ever been inserted into the buffer */
    BACNET_LOGGING_TYPE LoggingType;        /* Polled/cov/triggered */
    bool bAlignIntervals;                   /* If true align to the clock */
    uint32_t ulIntervalOffset;              /* Offset from start of period for taking reading in seconds */
    bool bTrigger;                          /* Set to 1 to cause a reading to be taken */
    int iIndex;                             /* Current insertion point */
    time_t tLastDataTime;
    TL_DATA_REC *Logs;
    struct hlist_node node;
} object_tl_t;


extern object_instance_t *trend_log_find_by_property(BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE *property);

extern int trend_log_record(object_instance_t *object, BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE *property,
            BACNET_APPLICATION_DATA_VALUE *value, BACNET_BIT_STRING *status_flags);

extern object_impl_t *object_create_impl_tl(void);

extern int trend_log_init(cJSON *object);

#ifdef __cplusplus
}
#endif

#endif /* _TRENDLOG_H_ */

