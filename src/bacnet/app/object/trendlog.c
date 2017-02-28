/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * trendlog.c
 * Original Author:  linzhixian, 2015-5-25
 *
 * Trend Log Object
 *
 * History
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "bacnet/object/trendlog.h"
#include "bacnet/object/device.h"
#include "trendlog_def.h"
#include "bacnet/app.h"
#include "bacnet/bacdcode.h"
#include "misc/hashtable.h"

DECLARE_HASHTABLE(monitored_property_table, TRENDLOG_HASH_BITS);

static int __property_hash(BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE *property)
{
    uint8_t *start, *end;
    int code;

    code = 0;
    start = (uint8_t *)property;
    end = start + sizeof(BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE);

    do {
        code = ROTATE_LEFT(code, 8);
        code += *start;
    } while (++start < end);

    return code;
}

/*
 * Use the combination of the enable flag and the enable times to determine
 * if the log is really enabled now. See 135-2008 sections 12.25.5 - 12.25.7
 *
 */
static bool Trend_Log_Is_Enabled(object_tl_t *TL)
{
    time_t tNow;
    bool bStatus;
     
    /* Not enabled so time is irrelevant */
    if (TL->bEnable == false) {
        return false;
    }

    /* Start time was after stop time as per 12.25.6 and 12.25.7 */
    if ((TL->ucTimeFlags == 0) && (TL->tStopTime < TL->tStartTime)) {
        return false;
    }

    bStatus = true;
    if (TL->ucTimeFlags != (TL_T_START_WILD | TL_T_STOP_WILD)) {
        /* enabled and either 1 wild card or none */
        tNow = time(NULL);

        if ((TL->ucTimeFlags & TL_T_START_WILD) != 0) {
            /* wild card start time */
            if (tNow > TL->tStopTime) {
                bStatus = false;
            }
        } else if ((TL->ucTimeFlags & TL_T_STOP_WILD) != 0) {
            /* wild card stop time */
            if (tNow < TL->tStartTime) {
                bStatus = false;
            }
        } else {
            /* No wildcards so use both times */
            if ((tNow < TL->tStartTime) || (tNow > TL->tStopTime)) {
                bStatus = false;
            }
        }
    }

    return bStatus;
}

/*
 * Insert a status record into a trend log - does not check for enable/log
 * full, time slots and so on as these type of entries have to go in
 * irrespective of such things which means that valid readings may get
 * pushed out of the log to make room.
 *
 */
static void Trend_Log_Insert_Status_Rec(object_tl_t *TL, BACNET_LOG_STATUS eStatus, bool bState)
{
    TL_DATA_REC TempRec;

    TempRec.tTimeStamp = time(NULL);
    TempRec.ucRecType = TL_TYPE_STATUS;
    TempRec.ucStatus = 0;
    TempRec.Datum.ucLogStatus = 0;
    
    /* Note we set the bits in correct order so that we can place them directly
     * into the bitstring structure later on when we have to encode them */
    switch (eStatus) {
    case LOG_STATUS_LOG_DISABLED:
        if (bState) {
            TempRec.Datum.ucLogStatus = 1 << LOG_STATUS_LOG_DISABLED;
        }
        break;

    case LOG_STATUS_BUFFER_PURGED:
        if (bState) {
            TempRec.Datum.ucLogStatus = 1 << LOG_STATUS_BUFFER_PURGED;
        }
        break;

    case LOG_STATUS_LOG_INTERRUPTED:
        TempRec.Datum.ucLogStatus = 1 << LOG_STATUS_LOG_INTERRUPTED;
        break;

    default:
        APP_ERROR("%s: invalid log status(%d)\r\n", __func__, eStatus);
        return;
    }

    TL->Logs[TL->iIndex++] = TempRec;
    if (TL->iIndex >= TL_MAX_ENTRIES) {
        TL->iIndex = 0;
    }

    TL->ulTotalRecordCount++;
    
    if (TL->ulRecordCount < TL_MAX_ENTRIES) {
        TL->ulRecordCount++;
    }
}

/* Convert a BACnet time into a local time in seconds since the local epoch  */
static time_t Trend_Log_BAC_Time_To_Local(BACNET_DATE_TIME *SourceTime)
{
    struct tm LocalTime;
    int iTemp;

    LocalTime.tm_year = SourceTime->date.year - 1900;   /* We store BACnet year in full format */
    
    /* Some clients send a date of all 0s to indicate start of epoch
     * even though this is not a valid date. Pick this up here and
     * correct the day and month for the local time functions.
     */
    iTemp = SourceTime->date.year + SourceTime->date.month + SourceTime->date.day;
    if (iTemp == 1900) {
        LocalTime.tm_mon = 0;
        LocalTime.tm_mday = 1;
    } else {
        LocalTime.tm_mon = SourceTime->date.month - 1;
        LocalTime.tm_mday = SourceTime->date.day;
    }

    LocalTime.tm_hour = SourceTime->time.hour;
    LocalTime.tm_min = SourceTime->time.min;
    LocalTime.tm_sec = SourceTime->time.sec;

    return mktime(&LocalTime);
}

static int tl_read_enable(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_tl_t *tl_obj;
    
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    tl_obj = container_of(object, object_tl_t, base.base);

    return encode_application_boolean(rp_data->application_data, tl_obj->bEnable);
}

static int tl_write_enable(object_instance_t *object, BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    object_tl_t *tl_obj;
    bool bEffectiveEnable;
    bool value;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    tl_obj = container_of(object, object_tl_t, base.base);
    
    if (decode_application_boolean(wp_data->application_data, &value)
            != wp_data->application_data_len) {
        return BACNET_STATUS_ERROR;
    }

    if (tl_obj->bEnable == value) {
        return 0;
    }
    
    /* Section 12.25.5 can't enable a full log with stop when full set */
    if ((value == true) && (tl_obj->ulRecordCount == TL_MAX_ENTRIES)
            && (tl_obj->bStopWhenFull == true)) {
        wp_data->error_class = ERROR_CLASS_OBJECT;
        wp_data->error_code = ERROR_CODE_LOG_BUFFER_FULL;
        return BACNET_STATUS_ERROR;
    }

    bEffectiveEnable = Trend_Log_Is_Enabled(tl_obj);
    tl_obj->bEnable = value;
    if (value == false) {
        if (bEffectiveEnable == true) {
            Trend_Log_Insert_Status_Rec(tl_obj, LOG_STATUS_LOG_DISABLED, true);
        }
    } else {
        if (Trend_Log_Is_Enabled(tl_obj)) {
            Trend_Log_Insert_Status_Rec(tl_obj, LOG_STATUS_LOG_DISABLED, false);
        }
    }

    return 0;
}

static int tl_read_stop_when_full(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_tl_t *tl_obj;
        
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    tl_obj = container_of(object, object_tl_t, base.base);

    return encode_application_boolean(rp_data->application_data, tl_obj->bStopWhenFull);
}

static int tl_write_stop_when_full(object_instance_t *object, BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    object_tl_t *tl_obj;
    bool value;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    tl_obj = container_of(object, object_tl_t, base.base);

    if (decode_application_boolean(wp_data->application_data, &value)
            != wp_data->application_data_len) {
        return BACNET_STATUS_ERROR;
    }

    if (tl_obj->bStopWhenFull != value) {
        tl_obj->bStopWhenFull = value;
        if ((value == true) && (tl_obj->ulRecordCount == TL_MAX_ENTRIES) 
                && (tl_obj->bEnable == true)) {
            tl_obj->bEnable = false;
            Trend_Log_Insert_Status_Rec(tl_obj, LOG_STATUS_LOG_DISABLED, true);
        }
    }

    return 0;
}

static int tl_read_buffer_size(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    return encode_application_unsigned(rp_data->application_data, TL_MAX_ENTRIES);
}

static int tl_read_log_buffer(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    /* You can only read the buffer via the ReadRange service */
    APP_ERROR("%s: PROP_LOG_BUFFER only can be read via the ReadRange service\r\n", __func__);
    rp_data->error_code = ERROR_CODE_READ_ACCESS_DENIED;
    
    return BACNET_STATUS_ERROR;
}

static int tl_read_record_count(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_tl_t *tl_obj;
    
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    tl_obj = container_of(object, object_tl_t, base.base);

    return encode_application_unsigned(rp_data->application_data, tl_obj->ulRecordCount);
}

static int tl_write_record_count(object_instance_t *object, BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    object_tl_t *tl_obj;
    uint32_t value;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    tl_obj = container_of(object, object_tl_t, base.base);

    if (decode_application_unsigned(wp_data->application_data, &value)
            != wp_data->application_data_len) {
        return BACNET_STATUS_ERROR;
    }

    if (value == 0) {
        tl_obj->ulRecordCount = 0;
        tl_obj->iIndex = 0;
        Trend_Log_Insert_Status_Rec(tl_obj, LOG_STATUS_BUFFER_PURGED, true);
    }

    return 0;
}

static int tl_read_total_record_count(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_tl_t *tl_obj;
    
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    tl_obj = container_of(object, object_tl_t, base.base);

    return encode_application_unsigned(rp_data->application_data, tl_obj->ulTotalRecordCount);
}

static int tl_read_logging_type(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_tl_t *tl_obj;
    
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    tl_obj = container_of(object, object_tl_t, base.base);

    return encode_application_enumerated(rp_data->application_data, tl_obj->LoggingType);
}

static int tl_write_logging_type(object_instance_t *object, BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    object_tl_t *tl_obj;
    uint32_t value;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    tl_obj = container_of(object, object_tl_t, base.base);

    if (decode_application_enumerated(wp_data->application_data, &value)
            != wp_data->application_data_len) {
        return BACNET_STATUS_ERROR;
    }

    if (value == LOGGING_TYPE_POLLED) {
        tl_obj->LoggingType = value;
        if (tl_obj->ulLogInterval == 0) {
            tl_obj->ulLogInterval = 900;        /* Default polling interval */
        }
    } else if (value == LOGGING_TYPE_TRIGGERED) {
        tl_obj->LoggingType = value;
        tl_obj->ulLogInterval = 0;
    } else if (value == LOGGING_TYPE_COV) {
        APP_ERROR("%s: we don't currrently support LOGGING_TYPE_COV\r\n", __func__);
        tl_obj->ulLogInterval = 0;
        wp_data->error_code = ERROR_CODE_OPTIONAL_FUNCTIONALITY_NOT_SUPPORTED;
        return BACNET_STATUS_ERROR;
    } else {
        APP_ERROR("%s: invalid PROP_LOGGING_TYPE value(%d)\r\n", __func__, value);
        return BACNET_STATUS_ERROR;
    }

    return 0;
}

static int tl_read_start_time(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_tl_t *tl_obj;
    int len;
    
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    tl_obj = container_of(object, object_tl_t, base.base);

    len = encode_application_date(rp_data->application_data, &(tl_obj->StartTime.date));
    len += encode_application_time(rp_data->application_data + len, &(tl_obj->StartTime.time));
    
    return len;
}

static int tl_write_start_time(object_instance_t *object, BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    object_tl_t *tl_obj;
    BACNET_DATE date;
    BACNET_TIME time;
    bool bEffectiveEnable;
    int len;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    tl_obj = container_of(object, object_tl_t, base.base);

    len = decode_application_date(wp_data->application_data, &date);
    if (len < 0) {
        return BACNET_STATUS_ERROR;
    }

    len += decode_application_time(wp_data->application_data + len, &time);
    if (len != wp_data->application_data_len) {
        return BACNET_STATUS_ERROR;
    }

    /* First record the current enable state of the log */
    bEffectiveEnable = Trend_Log_Is_Enabled(tl_obj);
    tl_obj->StartTime.date = date;
    tl_obj->StartTime.time = time;

    if (datetime_wildcard_present(&tl_obj->StartTime)) {
        /* Mark start time as wild carded */
        tl_obj->ucTimeFlags |= TL_T_START_WILD;
        tl_obj->tStartTime = 0;
    } else {
        /* Clear wild card flag and set time in local format */
        tl_obj->ucTimeFlags &= ~TL_T_START_WILD;
        tl_obj->tStartTime = Trend_Log_BAC_Time_To_Local(&tl_obj->StartTime);
    }

    /* Enable status has changed because of time update */
    if (bEffectiveEnable != Trend_Log_Is_Enabled(tl_obj)) {
        if (bEffectiveEnable == true) {
            Trend_Log_Insert_Status_Rec(tl_obj, LOG_STATUS_LOG_DISABLED, true);
        } else {
            Trend_Log_Insert_Status_Rec(tl_obj, LOG_STATUS_LOG_DISABLED, false);
        }
    }

    return 0;
}

static int tl_read_stop_time(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_tl_t *tl_obj;
    int len;
    
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    tl_obj = container_of(object, object_tl_t, base.base);

    len = encode_application_date(rp_data->application_data, &(tl_obj->StopTime.date));
    len += encode_application_time(rp_data->application_data + len, &(tl_obj->StopTime.time));
    
    return len;
}

static int tl_write_stop_time(object_instance_t *object, BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    object_tl_t *tl_obj;
    BACNET_DATE date;
    BACNET_TIME time;
    bool bEffectiveEnable;
    int len;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    tl_obj = container_of(object, object_tl_t, base.base);

    len = decode_application_date(wp_data->application_data, &date);
    if (len < 0) {
        return BACNET_STATUS_ERROR;
    }

    len += decode_application_time(wp_data->application_data + len, &time);
    if (len != wp_data->application_data_len) {
        return BACNET_STATUS_ERROR;
    }

    /* First record the current enable state of the log */
    bEffectiveEnable = Trend_Log_Is_Enabled(tl_obj);
    tl_obj->StopTime.date = date;
    tl_obj->StopTime.time = time;

    if (datetime_wildcard_present(&tl_obj->StopTime)) {
        /* Mark stop time as wild carded */
        tl_obj->ucTimeFlags |= TL_T_STOP_WILD;
        tl_obj->tStopTime = 0xFFFFFFFF;
    } else {
        /* Clear wild card flag and set time in local format */
        tl_obj->ucTimeFlags &= ~TL_T_STOP_WILD;
        tl_obj->tStopTime = Trend_Log_BAC_Time_To_Local(&tl_obj->StopTime);
    }

    /* Enable status has changed because of time update */
    if (bEffectiveEnable != Trend_Log_Is_Enabled(tl_obj)) {
        if (bEffectiveEnable == true) {
            Trend_Log_Insert_Status_Rec(tl_obj, LOG_STATUS_LOG_DISABLED, true);
        } else {
            Trend_Log_Insert_Status_Rec(tl_obj, LOG_STATUS_LOG_DISABLED, false);
        }
    }

    return 0;
}

static int tl_read_log_device_object_property(object_instance_t *object,
            BACNET_READ_PROPERTY_DATA *rp_data, RR_RANGE *range)
{
    object_tl_t *tl_obj;
    
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    tl_obj = container_of(object, object_tl_t, base.base);

    return bacapp_encode_device_obj_property_ref(rp_data->application_data, &(tl_obj->Source));;
}

static int tl_write_log_device_object_property(object_instance_t *object,
                BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE TempSource;
    object_tl_t *tl_obj;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    tl_obj = container_of(object, object_tl_t, base.base);

    if (bacapp_decode_device_obj_property_ref(wp_data->application_data, &TempSource)
            != wp_data->application_data_len) {
        APP_ERROR("%s: decode LOG_DEVICE_OBJECT_PROPERTY failed\r\n", __func__);
        wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
        return BACNET_STATUS_ERROR;
    }

    if ((TempSource.deviceIndentifier.type == OBJECT_DEVICE) 
            && (TempSource.deviceIndentifier.instance != device_object_instance_number())) {
        APP_ERROR("%s: decode invalid device id\r\n", __func__);
        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
        return BACNET_STATUS_ERROR;
    }

    /* Make sure device ID is set to ours in case not supplied */
    TempSource.deviceIndentifier.type = OBJECT_DEVICE;
    TempSource.deviceIndentifier.instance = device_object_instance_number();

    /* Quick comparison if structures are packed ... */
    if (memcmp(&TempSource, &tl_obj->Source,
            sizeof(BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE)) != 0) {
        /* Clear buffer if property being logged is changed */
        tl_obj->ulRecordCount = 0;
        tl_obj->iIndex = 0;
        Trend_Log_Insert_Status_Rec(tl_obj, LOG_STATUS_BUFFER_PURGED, true);
    }

    tl_obj->Source = TempSource;

    return 0;
}

static int tl_read_log_interval(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{   
    object_tl_t *tl_obj;
    
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    tl_obj = container_of(object, object_tl_t, base.base);

    /* We only log to 1 sec accuracy so must multiply by 100 before passing it on */
    return encode_application_unsigned(rp_data->application_data, tl_obj->ulLogInterval * 100);;
}

static int tl_write_log_interval(object_instance_t *object,BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    object_tl_t *tl_obj;
    uint32_t value;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    tl_obj = container_of(object, object_tl_t, base.base);

    if (decode_application_unsigned(wp_data->application_data, &value)
            != wp_data->application_data_len) {
        return BACNET_STATUS_ERROR;
    }

    if (tl_obj->LoggingType == LOGGING_TYPE_TRIGGERED) {
        /* Read only if triggered log so flag error and bail out */
        APP_ERROR("%s: write PROP_LOG_INTERVAL denied if LOGGING_TYPE_TRIGGERED\r\n", __func__);
        tl_obj->ulLogInterval = 0;
        wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
        return BACNET_STATUS_ERROR;
    }

    if ((tl_obj->LoggingType == LOGGING_TYPE_POLLED) && (value == 0)) {
        /* We don't support COV at the moment so don't allow switching
         * to it by clearing interval whilst in polling mode */
        APP_ERROR("%s: write PROP_LOG_INTERVAL failed cause we don't support COV\r\n", __func__);
        wp_data->error_code = ERROR_CODE_OPTIONAL_FUNCTIONALITY_NOT_SUPPORTED;
        return BACNET_STATUS_ERROR;
    }

    if ((tl_obj->LoggingType == LOGGING_TYPE_COV) && (value != 0)) {
        APP_ERROR("%s: write PROP_LOG_INTERVAL failed cause we don't support switching between COV and POLLED\r\n", __func__);
        wp_data->error_code = ERROR_CODE_OPTIONAL_FUNCTIONALITY_NOT_SUPPORTED;
        return BACNET_STATUS_ERROR;
    }

    /* We only log to 1 sec accuracy so must divide by 100 before passing it on */
    tl_obj->ulLogInterval = value / 100;

    return 0;
}

static int tl_read_align_interval(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_tl_t *tl_obj;
    
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    tl_obj = container_of(object, object_tl_t, base.base);

    return encode_application_boolean(rp_data->application_data, tl_obj->bAlignIntervals);
}

static int tl_write_align_interval(object_instance_t *object,BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    object_tl_t *tl_obj;
    bool value;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    tl_obj = container_of(object, object_tl_t, base.base);

    if (decode_application_boolean(wp_data->application_data, &value)
            != wp_data->application_data_len) {
        return BACNET_STATUS_ERROR;
    }

    tl_obj->bAlignIntervals = value;

    return 0;
}

static int tl_read_interval_offset(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{
    object_tl_t *tl_obj;
    
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    tl_obj = container_of(object, object_tl_t, base.base);

    /* We only log to 1 sec accuracy so must multiply by 100 before passing it on */
    return encode_application_unsigned(rp_data->application_data, tl_obj->ulIntervalOffset * 100);
}

static int tl_write_interval_offset(object_instance_t *object,BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    object_tl_t *tl_obj;
    uint32_t value;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    tl_obj = container_of(object, object_tl_t, base.base);

    if (decode_application_unsigned(wp_data->application_data, &value)
            != wp_data->application_data_len) {
        return BACNET_STATUS_ERROR;
    }

    /* We only log to 1 sec accuracy so must divide by 100 before passing it on */
    tl_obj->ulIntervalOffset = value / 100;

    return 0;
}

static int tl_read_trigger(object_instance_t *object, BACNET_READ_PROPERTY_DATA *rp_data,
            RR_RANGE *range)
{   
    object_tl_t *tl_obj;
    
    if ((rp_data->array_index != BACNET_ARRAY_ALL) || (range != NULL)) {
        rp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    tl_obj = container_of(object, object_tl_t, base.base);

    return encode_application_boolean(rp_data->application_data, tl_obj->bTrigger);
}

static int tl_write_trigger(object_instance_t *object,BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    object_tl_t *tl_obj;
    bool value;
    
    if (wp_data->array_index != BACNET_ARRAY_ALL) {
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return BACNET_STATUS_ERROR;
    }

    tl_obj = container_of(object, object_tl_t, base.base);

    if (decode_application_boolean(wp_data->application_data, &value)
            != wp_data->application_data_len) {
        return BACNET_STATUS_ERROR;
    }

    /* We will not allow triggered operation if polling with aligning
     * to the clock as that will produce non aligned readings which
     * goes against the reason for selscting this mode
     */
    if ((tl_obj->LoggingType == LOGGING_TYPE_POLLED) && (tl_obj->bAlignIntervals == true)) {
        APP_ERROR("%s: write PROP_TRIGGER failed cause polling with aligning\r\n", __func__);
        wp_data->error_code = ERROR_CODE_NOT_CONFIGURED_FOR_TRIGGERED_LOGGING;
        return BACNET_STATUS_ERROR;
    }
    
    tl_obj->bTrigger = value;
    
    return 0;
}

object_impl_t *object_create_impl_tl(void)
{
    object_impl_t *tl;
    property_impl_t *p_impl;
    
    tl = object_create_impl_seor(true, false, true);
    if (!tl) {
        APP_ERROR("%s: create SEO impl failed\r\n", __func__);
        return NULL;
    }
    tl->type = OBJECT_TRENDLOG;

    p_impl = object_impl_extend(tl, PROP_ENABLE, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_ENABLE failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = tl_read_enable;
    p_impl->write_property = tl_write_enable;

    p_impl = object_impl_extend(tl, PROP_STOP_WHEN_FULL, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_STOP_WHEN_FULL failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = tl_read_stop_when_full;
    p_impl->write_property = tl_write_stop_when_full;
    
    p_impl = object_impl_extend(tl, PROP_BUFFER_SIZE, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_BUFFER_SIZE failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = tl_read_buffer_size;
    
    p_impl = object_impl_extend(tl, PROP_LOG_BUFFER, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_LOG_BUFFER failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = tl_read_log_buffer;

    p_impl = object_impl_extend(tl, PROP_RECORD_COUNT, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_RECORD_COUNT failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = tl_read_record_count;
    p_impl->write_property = tl_write_record_count;

    p_impl = object_impl_extend(tl, PROP_TOTAL_RECORD_COUNT, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_TOTAL_RECORD_COUNT failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = tl_read_total_record_count;

    p_impl = object_impl_extend(tl, PROP_LOGGING_TYPE, PROPERTY_TYPE_REQUIRED);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_LOGGING_TYPE failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = tl_read_logging_type;
    p_impl->write_property = tl_write_logging_type;

    p_impl = object_impl_extend(tl, PROP_START_TIME, PROPERTY_TYPE_OPTIONAL);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_START_TIME failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = tl_read_start_time;
    p_impl->write_property = tl_write_start_time;

    p_impl = object_impl_extend(tl, PROP_STOP_TIME, PROPERTY_TYPE_OPTIONAL);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_STOP_TIME failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = tl_read_stop_time;
    p_impl->write_property = tl_write_stop_time;

    p_impl = object_impl_extend(tl, PROP_LOG_DEVICE_OBJECT_PROPERTY, PROPERTY_TYPE_OPTIONAL);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_LOG_DEVICE_OBJECT_PROPERTY failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = tl_read_log_device_object_property;
    p_impl->write_property = tl_write_log_device_object_property;

    p_impl = object_impl_extend(tl, PROP_LOG_INTERVAL, PROPERTY_TYPE_OPTIONAL);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_LOG_INTERVAL failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = tl_read_log_interval;
    p_impl->write_property = tl_write_log_interval;

    p_impl = object_impl_extend(tl, PROP_ALIGN_INTERVALS, PROPERTY_TYPE_OPTIONAL);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_ALIGN_INTERVALS failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = tl_read_align_interval;
    p_impl->write_property = tl_write_align_interval;
    
    p_impl = object_impl_extend(tl, PROP_INTERVAL_OFFSET, PROPERTY_TYPE_OPTIONAL);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_INTERVAL_OFFSET failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = tl_read_interval_offset;
    p_impl->write_property = tl_write_interval_offset;

    p_impl = object_impl_extend(tl, PROP_TRIGGER, PROPERTY_TYPE_OPTIONAL);
    if (!p_impl) {
        APP_ERROR("%s: extend PROP_TRIGGER failed\r\n", __func__);
        goto out;
    }
    p_impl->read_property = tl_read_trigger;
    p_impl->write_property = tl_write_trigger;

    return tl;

out:
    object_impl_destroy(tl);
    
    return NULL;
}

int __attribute__((weak)) trend_log_init(cJSON *object)
{
    object_tl_t *tl;
    object_instance_t *tl_instance;
    object_impl_t *tl_type = NULL;
    cJSON *array, *instance, *tmp;
    struct tm TempTime;
    time_t tClock;
    int i, j;
    
    if (object == NULL) {
        goto end;
    }

    hash_init(monitored_property_table);
    
    array = cJSON_GetObjectItem(object, "Instance_List");
    if ((array == NULL) || (array->type != cJSON_Array)) {
        APP_ERROR("%s: get Instance_List item failed\r\n", __func__);
        goto out;
    }

    i = 0;
    cJSON_ArrayForEach(instance, array) {
        if (instance->type != cJSON_Object) {
            APP_ERROR("%s: invalid Instance_List[%d] item type\r\n", __func__, i);
            goto reclaim;
        }

        tl = (object_tl_t *)malloc(sizeof(object_tl_t));
        if (!tl) {
            APP_ERROR("%s: not enough memory\r\n", __func__);
            goto reclaim;
        }
        memset(tl, 0, sizeof(*tl));
        tl->base.base.instance = i;

        tmp = cJSON_GetObjectItem(instance, "Name");
        if ((tmp == NULL) || (tmp->type != cJSON_String)) {
            APP_ERROR("%s: get Instance_List[%d] Name item failed\r\n", __func__, i);
            free(tl);
            goto reclaim;
        }
        if (!vbuf_fr_str(&tl->base.base.object_name.vbuf, tmp->valuestring, OBJECT_NAME_MAX_LEN)) {
            APP_ERROR("%s: set object name overflow\r\n", __func__);
            free(tl);
            goto reclaim;
        }
        
        tmp = cJSON_GetObjectItem(instance, "Logged_DeviceID");
        if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
            APP_ERROR("%s: get Instance_List[%d] Logged_DeviceID item failed\r\n", __func__, i);
            free(tl);
            goto reclaim;
        }
        tl->Source.deviceIndentifier.type = OBJECT_DEVICE;
        tl->Source.deviceIndentifier.instance = (uint32_t)tmp->valueint;

        tmp = cJSON_GetObjectItem(instance, "Logged_ObjectType");
        if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
            APP_ERROR("%s: get Instance_List[%d] Logged_ObjectType item failed\r\n", __func__, i);
            free(tl);
            goto reclaim;
        }
        tl->Source.objectIdentifier.type = tmp->valueint;

        tmp = cJSON_GetObjectItem(instance, "Logged_ObjectInstance");
        if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
            APP_ERROR("%s: get Instance_List[%d] Logged_ObjectInstance item failed\r\n", __func__, i);
            free(tl);
            goto reclaim;
        }
        tl->Source.objectIdentifier.instance = (uint32_t)tmp->valueint;

        tmp = cJSON_GetObjectItem(instance, "Logged_PropertyID");
        if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
            APP_ERROR("%s: get Instance_List[%d] Logged_PropertyID item failed\r\n", __func__, i);
            free(tl);
            goto reclaim;
        }
        tl->Source.propertyIdentifier = tmp->valueint;

        tmp = cJSON_GetObjectItem(instance, "Logged_PropertyIndex");
        if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
            APP_ERROR("%s: get Instance_List[%d] Logged_PropertyIndex item failed\r\n", __func__, i);
            free(tl);
            goto reclaim;
        }
        
        if (tmp->valueint == -1) {
            tl->Source.arrayIndex = BACNET_ARRAY_ALL;
        } else {
            tl->Source.arrayIndex = tmp->valueint;
        }

        if (!tl_type) {
            tl_type = (object_impl_t *)object_create_impl_tl();
            if (!tl_type) {
                APP_ERROR("%s: create tl type impl failed\r\n", __func__);
                goto out;
            }
        }
        tl->base.base.type = tl_type;

        tl->Logs = (TL_DATA_REC *)malloc(sizeof(TL_DATA_REC) * TL_MAX_ENTRIES);
        if (tl->Logs == NULL) {
            APP_ERROR("%s: malloc Instance_List[%d] Logs failed\r\n", __func__, i);
            free(tl);
            goto reclaim;
        }
        (void)memset(tl->Logs, 0, sizeof(TL_DATA_REC) * TL_MAX_ENTRIES);

        /*
         * Do we need to do anything here?
         * Trend logs are usually assumed to survive over resets
         * and are frequently implemented using Battery Backed RAM
         * If they are implemented using Flash or SD cards or some
         * such mechanism there may be some RAM based setup needed
         * for log management purposes.
         * We probably need to look at inserting LOG_INTERRUPTED
         * entries into any active logs if the power down or reset
         * may have caused us to miss readings.
         */

        /* We will just fill the logs with some entries for testing
         * purposes.
         */

        TempTime.tm_year = 109;
        TempTime.tm_mon = i + 1;    /* Different month for each log */
        TempTime.tm_mday = 1;
        TempTime.tm_hour = 0;
        TempTime.tm_min = 0;
        TempTime.tm_sec = 0;
        tClock = mktime(&TempTime);

        for (j = 0; j < TL_MAX_ENTRIES; j++) {
            tl->Logs[j].tTimeStamp = tClock;
            tl->Logs[j].ucRecType = TL_TYPE_REAL;
            tl->Logs[j].Datum.fReal = (float)(j + (i * TL_MAX_ENTRIES));
            /* Put status flags with every second log */
            if ((i & 1) == 0) {
                tl->Logs[j].ucStatus = 128;
            } else {
                tl->Logs[j].ucStatus = 0;
            }
            
            tClock += 900;  /* advance 15 minutes */
        }

        tl->tLastDataTime = tClock - 900;
        tl->bAlignIntervals = true;
        tl->bEnable = true;
        tl->bStopWhenFull = false;
        tl->bTrigger = false;
        tl->LoggingType = LOGGING_TYPE_POLLED;
        tl->ucTimeFlags = 0;
        tl->ulIntervalOffset = 0;
        tl->iIndex = 0;
        tl->ulLogInterval = 900;
        tl->ulRecordCount = TL_MAX_ENTRIES;
        tl->ulTotalRecordCount = 10000;
        
        datetime_set_values(&tl->StartTime, 2009, 1, 1, 0, 0, 0, 0);
        tl->tStartTime = Trend_Log_BAC_Time_To_Local(&tl->StartTime);
        datetime_set_values(&tl->StopTime, 2020, 12, 22, 23, 59, 59, 99);
        tl->tStopTime = Trend_Log_BAC_Time_To_Local(&tl->StopTime);

        if (!object_add(&tl->base.base)) {
            APP_ERROR("%s: object add failed\r\n", __func__);
            free(tl->Logs);
            free(tl);
            goto reclaim;
        }
        hash_add(monitored_property_table, &tl->node, __property_hash(&(tl->Source)));
        i++;
    }

end:
    return OK;

reclaim:
    for (i = i - 1; i >= 0; i--) {
        tl_instance = object_find(OBJECT_TRENDLOG, i);
        if (!tl_instance) {
            APP_ERROR("%s: reclaim failed\r\n", __func__);
        } else {
            object_detach(tl_instance);
            tl = container_of(tl_instance, object_tl_t, base.base);
            hash_del(&tl->node);
            free(tl->Logs);
            free(tl);
        }
    }

    if (tl_type) {
        object_impl_destroy(tl_type);
    }

out:

    return -EPERM;
}

object_instance_t *trend_log_find_by_property(BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE *property)
{
    object_tl_t *tl;

    hash_for_each_possible(monitored_property_table, tl, node, __property_hash(property)) {
        if (bacapp_device_obj_property_ref_same(&tl->Source, property)) {
            return &tl->base.base;
        }
    }

    return NULL;
}

int trend_log_record(object_instance_t *object, BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE *property,
        BACNET_APPLICATION_DATA_VALUE *value, BACNET_BIT_STRING *status_flags)
{
    object_tl_t *tl;
    BACNET_BIT_STRING *TempBits;
    TL_DATA_REC TempRec;
    uint8_t ucCount;
    
    if (value == NULL) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    tl = container_of(object, object_tl_t, base.base);

    if (!bacapp_device_obj_property_ref_same(&tl->Source, property)) {
        APP_ERROR("%s: invalid property\r\n", __func__);
        return -EPERM;
    }

    TempRec.tTimeStamp = time(NULL);
    tl->tLastDataTime = TempRec.tTimeStamp;
    TempRec.ucStatus = 0;
    if (status_flags) {
        TempRec.ucStatus = 128 | status_flags->value[0];
    }

    switch (value->tag) {
    case BACNET_APPLICATION_TAG_NULL:
        TempRec.ucRecType = TL_TYPE_NULL;
        break;

    case BACNET_APPLICATION_TAG_BOOLEAN:
        TempRec.ucRecType = TL_TYPE_BOOL;
        TempRec.Datum.ucBoolean = (uint8_t)value->type.Boolean;
        break;

    case BACNET_APPLICATION_TAG_UNSIGNED_INT:
        TempRec.ucRecType = TL_TYPE_UNSIGN;
        TempRec.Datum.ulUValue = value->type.Unsigned_Int;
        break;

    case BACNET_APPLICATION_TAG_SIGNED_INT:
        TempRec.ucRecType = TL_TYPE_SIGN;
        TempRec.Datum.lSValue = value->type.Signed_Int;
        break;

    case BACNET_APPLICATION_TAG_REAL:
        TempRec.ucRecType = TL_TYPE_REAL;
        TempRec.Datum.fReal = value->type.Real;
        break;

    case BACNET_APPLICATION_TAG_BIT_STRING:
        TempRec.ucRecType = TL_TYPE_BITS;
        /* We truncate any bitstrings at 32 bits to conserve space */
        TempBits = &(value->type.Bit_String);
        if (bitstring_size(TempBits) < 32) {
            /* Store the bytes used and the bits free in the last byte */
            TempRec.Datum.Bits.ucLen = TempBits->byte_len << 4;
            TempRec.Datum.Bits.ucLen |= TempBits->last_byte_bits_unused & 7;

            /* Fetch the octets with the bits directly */
            for (ucCount = 0; ucCount < TempBits->byte_len; ucCount++) {
                TempRec.Datum.Bits.ucStore[ucCount] = TempBits->value[ucCount];
            }
        } else {
            /* We will only use the first 4 octets to save space */
            TempRec.Datum.Bits.ucLen = 4 << 4;
            for (ucCount = 0; ucCount < 4; ucCount++) {
                TempRec.Datum.Bits.ucStore[ucCount] = TempBits->value[ucCount];
            }
        }
        break;

    case BACNET_APPLICATION_TAG_ENUMERATED:
        TempRec.ucRecType = TL_TYPE_ENUM;
        TempRec.Datum.ulEnum = value->type.Enumerated;
        break;
    
    default:
        TempRec.ucRecType = TL_TYPE_ERROR;
        TempRec.Datum.Error.usClass = ERROR_CLASS_PROPERTY;
        TempRec.Datum.Error.usCode = ERROR_CODE_DATATYPE_NOT_SUPPORTED;
        break;
    }

    tl->Logs[tl->iIndex++] = TempRec;
    if (tl->iIndex >= TL_MAX_ENTRIES) {
        tl->iIndex = 0;
    }
    
    tl->ulTotalRecordCount++;

    if (tl->ulRecordCount < TL_MAX_ENTRIES) {
        tl->ulRecordCount++;
    }

    return OK;
}

