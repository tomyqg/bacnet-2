/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * timesync.c
 * Original Author:  linzhixian, 2015-1-29
 *
 * TimeSync
 *
 * History
 */

#include <errno.h>

#include "bacnet/bacenum.h"
#include "bacnet/service/timesync.h"
#include "bacnet/bacdcode.h"
#include "bacnet/service/dcc.h"
#include "bacnet/network.h"
#include "bacnet/app.h"

static int timesync_decode_service_request(uint8_t *request, uint16_t request_len, 
            BACNET_DATE *my_date, BACNET_TIME *my_time)
{
    int len, dec_len;

    if ((request == NULL) || (my_date == NULL) || (my_time == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }
    
    /* date */
    len = decode_application_date(request, my_date);
    if (len < 0) {
        APP_ERROR("%s: invalid date\r\n", __func__);
        return -EPERM;
    }
    
    /* time */
    dec_len = decode_application_time(&request[len], my_time);
    if (dec_len < 0) {
        APP_ERROR("%s: invalid time\r\n", __func__);
        return -EPERM;
    }
    len += dec_len;

    if (len != request_len) {
        APP_ERROR("%s: invalid pdu len(%d)\r\n", __func__, request_len);
        return -EPERM;
    }
    return len;
}

static int timesync_encode_apdu(bacnet_buf_t *apdu, BACNET_DATE *my_date, BACNET_TIME *my_time)
{
    uint8_t *pdu;
    int len;

    pdu = apdu->data;
    pdu[0] = PDU_TYPE_UNCONFIRMED_SERVICE_REQUEST << 4;
    pdu[1] = SERVICE_UNCONFIRMED_TIME_SYNCHRONIZATION;
    
    len = 2;
    len += encode_application_date(&pdu[len], my_date);
    len += encode_application_time(&pdu[len], my_time);

    apdu->data_len = len;

    return len;
}

static int timesync_utc_encode_apdu(bacnet_buf_t *apdu, BACNET_DATE *my_date, BACNET_TIME *my_time)
{
    uint8_t *pdu;
    int len;

    pdu = apdu->data;
    pdu[0] = PDU_TYPE_UNCONFIRMED_SERVICE_REQUEST << 4;
    pdu[1] = SERVICE_UNCONFIRMED_UTC_TIME_SYNCHRONIZATION;
    
    len = 2;
    len += encode_application_date(&pdu[len], my_date);
    len += encode_application_time(&pdu[len], my_time);

    apdu->data_len = len;

    return len;
}

void handler_timesync(uint8_t *request, uint16_t request_len, bacnet_addr_t *src)
{
    BACNET_DATE bdate;
    BACNET_TIME btime;
    int len;
    
    len = timesync_decode_service_request(request, request_len, &bdate, &btime);
    if (len < 0) {
        APP_ERROR("%s: decode service request failed(%d)\r\n", __func__, len);
        return;
    }

    /* show_bacnet_date_time(&bdate, &btime); */

    /* TODO: set the time */

    return;
}

void handler_timesync_utc(uint8_t *request, uint16_t request_len, bacnet_addr_t *src)
{
    BACNET_DATE bdate;
    BACNET_TIME btime;
    int len;
    
    len = timesync_decode_service_request(request, request_len, &bdate, &btime);
    if (len < 0) {
        APP_ERROR("%s: decode service request failed(%d)\r\n", __func__, len);
        return;
    }
    
    /* show_bacnet_date_time(&bdate, &btime); */

    /* TODO: set the time */

    return;
}

int Send_TimeSync_Remote(bacnet_addr_t *dst, BACNET_DATE *bdate, BACNET_TIME *btime)
{
    int len;
    int rv;
    DECLARE_BACNET_BUF(tx_apdu, MIN_APDU);
    
    if (!dcc_communication_enabled()) {
        APP_ERROR("%s: dcc communication disabled\r\n", __func__);
        return -EPERM;
    }
    
    (void)bacnet_buf_init(&tx_apdu.buf, MIN_APDU);
    len = timesync_encode_apdu(&tx_apdu.buf, bdate, btime);
    if (len < 0) {
        APP_ERROR("%s: encode apdu failed(%d)\r\n", __func__, len);
        return len;
    }

    rv = network_send_pdu(dst, &tx_apdu.buf, PRIORITY_NORMAL, false);
    if (rv < 0) {
        APP_ERROR("%s: network send failed(%d)\r\n", __func__, rv);
    }

    return rv;
}

int Send_TimeSync(BACNET_DATE *bdate, BACNET_TIME *btime)
{
    bacnet_addr_t dst;

    dst.net = BACNET_BROADCAST_NETWORK;
    dst.len = 0;
    
    return Send_TimeSync_Remote(&dst, bdate, btime);
}

int Send_TimeSyncUTC(BACNET_DATE *bdate, BACNET_TIME *btime)
{
    bacnet_addr_t dst;
    DECLARE_BACNET_BUF(tx_apdu, MIN_APDU);
    int len;
    int rv;
    
    if (!dcc_communication_enabled()) {
        APP_ERROR("%s: dcc communication disabled\r\n", __func__);
        return -EPERM;
    }
    
    dst.net = BACNET_BROADCAST_NETWORK;
    dst.len = 0;

    (void)bacnet_buf_init(&tx_apdu.buf, MIN_APDU);
    len = timesync_utc_encode_apdu(&tx_apdu.buf, bdate, btime);
    if (len < 0) {
        APP_ERROR("%s: encode apdu failed(%d)\r\n", __func__, len);
        return len;
    }

    rv = network_send_pdu(&dst, &tx_apdu.buf, PRIORITY_NORMAL, false);
    if (rv < 0) {
        APP_ERROR("%s: network send failed(%d)\r\n", __func__, rv);
    }

    return rv;    
}

