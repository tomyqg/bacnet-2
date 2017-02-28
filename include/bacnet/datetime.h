/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * datetime.h
 * Original Author:  linzhixian, 2015-1-29
 *
 * BACnet DATE & TIME
 *
 * History
 */

#ifndef _DATETIME_H_
#define _DATETIME_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* date */
typedef struct BACnet_Date {
    uint16_t year;              /* AD */
    uint8_t month;              /* 1=Jan */
    uint8_t day;                /* 1..31 */
    uint8_t wday;               /* 1=Monday-7=Sunday */
} BACNET_DATE;

/* time */
typedef struct BACnet_Time {
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    uint8_t hundredths;
} BACNET_TIME;

typedef struct BACnet_DateTime {
    BACNET_DATE date;
    BACNET_TIME time;
} BACNET_DATE_TIME;

extern void datetime_set_date(BACNET_DATE *bdate, uint16_t year, uint8_t month, uint8_t day);

extern void datetime_set_time(BACNET_TIME *btime, uint8_t hour, uint8_t minute, uint8_t seconds,
                uint8_t hundredths);

extern void datetime_set(BACNET_DATE_TIME *bdatetime, BACNET_DATE *bdate, BACNET_TIME *btime);

extern void datetime_set_values(BACNET_DATE_TIME *bdatetime, uint16_t year, uint8_t month, 
                uint8_t day, uint8_t hour, uint8_t minute, uint8_t seconds, uint8_t hundredths);

extern bool datetime_wildcard_present(BACNET_DATE_TIME *bdatetime);

#ifdef __cplusplus
}
#endif

#endif  /* _DATETIME_H_ */

