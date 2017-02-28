/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * whois.c
 * Original Author:  linzhixian, 2015-2-10
 *
 * Who Is Request Service
 *
 * History
 */

#include "bacnet/service/whois.h"
#include "bacnet/bacdcode.h"
#include "bacnet/object/device.h"
#include "bacnet/service/iam.h"
#include "bacnet/service/dcc.h"
#include "bacnet/network.h"
#include "bacnet/app.h"
#include "bacnet/slaveproxy.h"

int whois_decode_service_request(uint8_t *pdu, uint16_t pdu_len, uint32_t *pLow_limit,
            uint32_t *pHigh_limit)
{
    int len, dec_len;
    unsigned low, high;

    if (pdu == NULL) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return BACNET_STATUS_ERROR;
    }

    if (pdu_len == 0) {
        len = 0;
        low = 0;
        high = BACNET_MAX_INSTANCE;
    } else {
        /* optional limits - must be used as a pair */
        len = decode_context_unsigned(pdu, 0, &low);
        if (len < 0) {
            APP_ERROR("%s: invalid low limit\r\n", __func__);
            return BACNET_STATUS_ERROR;
        }
        
        dec_len = decode_context_unsigned(&pdu[len], 1, &high);
        if (len < 0) {
            APP_ERROR("%s: invalid high limit\r\n", __func__);
            return BACNET_STATUS_ERROR;
        }
        len += dec_len;

        if (pdu_len != len) {
            APP_ERROR("%s: invalid pdu_len(%d)\r\n", __func__, pdu_len);
            return BACNET_STATUS_ERROR;
        }

        if (high > BACNET_MAX_INSTANCE) {
            APP_ERROR("%s: too large high limit(%d)\r\n", __func__, high);
            return BACNET_STATUS_ERROR;
        } else if (low > high) {
            APP_ERROR("%s: low limit(%d) > high limit(%d)\r\n", __func__,
                    low, high);
            return BACNET_STATUS_ERROR;
        }
    }

    
    if (pLow_limit) {
        *pLow_limit = low;
    }

    if (pHigh_limit) {
        *pHigh_limit = high;
    }

    return len;
}

void handler_who_is(uint8_t *request, uint16_t request_len, bacnet_addr_t *src)
{
    uint32_t low_limit;
    uint32_t high_limit;
    int len;

    if ((request == NULL) || (src == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
    }
    
    len = whois_decode_service_request(request, request_len, &low_limit, &high_limit);    
    if (len < 0) {
        APP_ERROR("%s: decode service request failed(%d)\r\n", __func__, len);
    }

    if ((device_object_instance_number() >= low_limit)
            && (device_object_instance_number() <= high_limit)) {
        Send_I_Am_Remote(src->net);
    }
}

/* encode who_is service:  use -1 for limit if you want unlimited */
static int whois_encode_apdu(bacnet_buf_t *apdu, int32_t low_limit, int32_t high_limit)
{
    uint8_t *pdu;
    int len;

    pdu = apdu->data;
    pdu[0] = PDU_TYPE_UNCONFIRMED_SERVICE_REQUEST << 4;
    pdu[1] = SERVICE_UNCONFIRMED_WHO_IS;
    len = 2;

    /* optional limits - must be used as a pair */
    if ((low_limit >= 0) && (low_limit <= BACNET_MAX_INSTANCE) && (high_limit >= 0) 
            && (high_limit <= BACNET_MAX_INSTANCE)) {
        len += encode_context_unsigned(&pdu[len], 0, low_limit);
        len += encode_context_unsigned(&pdu[len], 1, high_limit);
    }
    
    apdu->data_len = len;
    
    return len;
}

/** Send_WhoIs_To_Network - Send a Who-Is request to a remote network for a specific device, a range,
 * or any device.
 * 
 * @dst: [in] BACnet address of target router
 * @low_limit: [in] Device Instance Low Range, 0 - 4,194,303 or -1
 * @high_limit: [in] Device Instance High Range, 0 - 4,194,303 or -1
 *
 * If low_limit and high_limit both are -1, then the range is unlimited.
 * If low_limit and high_limit have the same non-negative value, then only
 * that device will respond.
 * Otherwise, low_limit must be less than high_limit.
 *
 */
static void Send_WhoIs_To_Network(bacnet_addr_t *dst, int32_t low_limit, int32_t high_limit)
{
    DECLARE_BACNET_BUF(tx_apdu, MIN_APDU);
    int rv;

    (void)bacnet_buf_init(&tx_apdu.buf, MIN_APDU);
    (void)whois_encode_apdu(&tx_apdu.buf, low_limit, high_limit);

    rv = network_send_pdu(dst, &tx_apdu.buf, PRIORITY_NORMAL, false);
    if (rv < 0) {
        APP_ERROR("%s: network send failed(%d)\r\n", __func__, rv);
    }

    return;
}

/** Send_WhoIs_Local - Send a local Who-Is request for a specific device, a range, or any device.
 *
 * @low_limit: [in] Device Instance Low Range, 0 - 4,194,303 or -1
 * @high_limit: [in] Device Instance High Range, 0 - 4,194,303 or -1
 * 
 * If low_limit and high_limit both are -1, then the range is unlimited.
 * If low_limit and high_limit have the same non-negative value, then only
 * that device will respond.
 * Otherwise, low_limit must be less than high_limit.
 *
 */
void Send_WhoIs_Local(int32_t low_limit, int32_t high_limit)
{
    bacnet_addr_t dst;

    if ((low_limit > high_limit) || (low_limit < -1) || (high_limit > BACNET_MAX_INSTANCE)
            || ((low_limit < 0) && (high_limit >= 0))) {
        APP_ERROR("%s: invalid arguments low_limit(%d) high_limit(%d)\r\n", __func__,
            low_limit, high_limit);
        return;
    }

    if (!dcc_communication_enabled()) {
        APP_ERROR("%s: dcc communication disabled\r\n", __func__);
        return;
    }

    dst.net = 0;
    dst.len = 0;

    Send_WhoIs_To_Network(&dst, low_limit, high_limit);
}

/** Send_WhoIs_Global - Send a global Who-Is request for a specific device, a range, or any device.
 *
 * @low_limit: [in] Device Instance Low Range, 0 - 4,194,303 or -1
 * @high_limit: [in] Device Instance High Range, 0 - 4,194,303 or -1
 *
 * If low_limit and high_limit both are -1, then the range is unlimited.
 * If low_limit and high_limit have the same non-negative value, then only
 * that device will respond.
 * Otherwise, low_limit must be less than high_limit.
 *
 */
void Send_WhoIs_Global(int32_t low_limit, int32_t high_limit)
{
    bacnet_addr_t dst;

    if ((low_limit > high_limit) || (low_limit < -1) || (high_limit > BACNET_MAX_INSTANCE)
            || ((low_limit < 0) && (high_limit >= 0))) {
        APP_ERROR("%s: invalid arguments low_limit(%d) high_limit(%d)\r\n", __func__,
            low_limit, high_limit);
        return;
    }
    
    if (!dcc_communication_enabled()) {
        APP_ERROR("%s: dcc communication disabled\r\n", __func__);
        return;
    }

    /* Who-Is is a global broadcast */
    dst.net = BACNET_BROADCAST_NETWORK;
    dst.len = 0;

    Send_WhoIs_To_Network(&dst, low_limit, high_limit);
}

/** Send_WhoIs_Remote - Send a Who-Is request to a remote network for a specific device, a range,
 * or any device.
 *
 * @net: [in] uint16_t, remote network to broadcast
 * @low_limit: [in] Device Instance Low Range, 0 - 4,194,303 or -1
 * @high_limit: [in] Device Instance High Range, 0 - 4,194,303 or -1
 *
 * If low_limit and high_limit both are -1, then the range is unlimited.
 * If low_limit and high_limit have the same non-negative value, then only
 * that device will respond.
 * Otherwise, low_limit must be less than high_limit.
 *
 */
void Send_WhoIs_Remote(uint16_t net, int32_t low_limit, int32_t high_limit)
{
    bacnet_addr_t dst;

    if ((low_limit > high_limit) || (low_limit < -1) || (high_limit > BACNET_MAX_INSTANCE)
            || ((low_limit < 0) && (high_limit >= 0))) {
        APP_ERROR("%s: invalid arguments low_limit(%d) high_limit(%d)\r\n", __func__,
            low_limit, high_limit);
        return;
    }
    
    if (!dcc_communication_enabled()) {
        APP_ERROR("%s: dcc communication disabled\r\n", __func__);
        return;
    }
    
    dst.net = net;
    dst.len = 0;
    
    Send_WhoIs_To_Network(&dst, low_limit, high_limit);
}

/** Send a global Who-Is request for a specific device, a range, or any device.
 * 
 * @low_limit: [in] Device Instance Low Range, 0 - 4,194,303 or -1
 * @high_limit: [in] Device Instance High Range, 0 - 4,194,303 or -1
 *
 * This was the original Who-Is broadcast but the code was moved to the more
 * descriptive Send_WhoIs_Global when Send_WhoIs_Local and Send_WhoIsRemote was added.
 * If low_limit and high_limit both are -1, then the range is unlimited.
 * If low_limit and high_limit have the same non-negative value, then only
 * that device will respond.
 * Otherwise, low_limit must be less than high_limit.
 *
 */
void Send_WhoIs(int32_t low_limit, int32_t high_limit)
{
    Send_WhoIs_Global(low_limit, high_limit);
}

