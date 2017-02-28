/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * datetime.c
 * Original Author:  linzhixian, 2015-3-17
 *
 * Manipulate BACnet Date and Time values
 *
 * History
 */

#include <stdbool.h>

#include "bacnet/datetime.h"

static bool is_leap_year(uint16_t year)
{
    if ((year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0)) {
        return true;
    } else {
        return false;
    }
}

static uint8_t month_days(uint16_t year, uint8_t month)
{
    /* note: start with a zero in the first element to save us from a
       month - 1 calculation in the lookup */
    int month_days[13] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    /* February */
    if ((month == 2) && is_leap_year(year)) {
        return 29;
    } else if (month >= 1 && month <= 12) {
        return (uint8_t)month_days[month];
    } else {
        return 0;
    }
}

static bool date_is_valid(uint16_t year, uint8_t month, uint8_t day)
{
    uint8_t monthdays;

    monthdays = month_days(year, month);
    if ((year >= 1900) && (monthdays) && (day >= 1) && (day <= monthdays)) {
        return true;
    }

    return false;
}

static uint32_t days_since_epoch(uint16_t year, uint8_t month, uint8_t day)
{
    uint32_t days;
    uint16_t years;
    uint8_t months;

    days = 0;
    years = 0;
    months = 0;
    
    if (date_is_valid(year, month, day)) {
        for (years = 1900; years < year; years++) {
            days += 365;
            if (is_leap_year(years)) {
                days++;
            }
        }
        
        for (months = 1; months < month; months++) {
            days += month_days(years, months);
        }
        
        days += (day - 1);
    }

    return days;
}

/* Jan 1, 1900 is a Monday */
/* wday 1=Monday...7=Sunday */
static uint8_t day_of_week(uint16_t year, uint8_t month, uint8_t day)
{
    return (uint8_t)((days_since_epoch(year, month, day) % 7) + 1);
}

void datetime_set_date(BACNET_DATE *bdate, uint16_t year, uint8_t month, uint8_t day)
{
    if (bdate) {
        bdate->year = year;
        bdate->month = month;
        bdate->day = day;
        bdate->wday = day_of_week(year, month, day);
    }
}

void datetime_set_time(BACNET_TIME *btime, uint8_t hour, uint8_t minute, uint8_t seconds,
        uint8_t hundredths)
{
    if (btime) {
        btime->hour = hour;
        btime->min = minute;
        btime->sec = seconds;
        btime->hundredths = hundredths;
    }
}

void datetime_set(BACNET_DATE_TIME *bdatetime, BACNET_DATE *bdate, BACNET_TIME *btime)
{
    if (bdate && btime && bdatetime) {
        bdatetime->time.hour = btime->hour;
        bdatetime->time.min = btime->min;
        bdatetime->time.sec = btime->sec;
        bdatetime->time.hundredths = btime->hundredths;
        bdatetime->date.year = bdate->year;
        bdatetime->date.month = bdate->month;
        bdatetime->date.day = bdate->day;
        bdatetime->date.wday = bdate->wday;
    }
}

void datetime_set_values(BACNET_DATE_TIME *bdatetime, uint16_t year, uint8_t month, uint8_t day,
        uint8_t hour, uint8_t minute, uint8_t seconds, uint8_t hundredths)
{
    if (bdatetime) {
        bdatetime->date.year = year;
        bdatetime->date.month = month;
        bdatetime->date.day = day;
        bdatetime->date.wday = day_of_week(year, month, day);
        bdatetime->time.hour = hour;
        bdatetime->time.min = minute;
        bdatetime->time.sec = seconds;
        bdatetime->time.hundredths = hundredths;
    }
}

/* Returns true if any type of wildcard is present except for day of week on it's own. */
bool datetime_wildcard_present(BACNET_DATE_TIME *bdatetime)
{
    bool wildcard_present;

    wildcard_present = false;
    
    if (bdatetime) {
        if ((bdatetime->date.year == (1900 + 0xFF)) || (bdatetime->date.month > 12) 
                || (bdatetime->date.day > 31) || (bdatetime->time.hour == 0xFF) 
                || (bdatetime->time.min == 0xFF) || (bdatetime->time.sec == 0xFF) 
                || (bdatetime->time.hundredths == 0xFF)) {
            wildcard_present = true;
        }
    }

    return wildcard_present;
}

