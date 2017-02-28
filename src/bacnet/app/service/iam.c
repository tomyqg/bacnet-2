/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * iam.c
 * Original Author:  linzhixian, 2015-2-11
 *
 * I Am Request Service
 *
 * History
 */

#include "bacnet/service/iam.h"

#include "bacnet/addressbind.h"
#include "bacnet/bacenum.h"
#include "bacnet/bacdcode.h"
#include "bacnet/object/device.h"
#include "bacnet/network.h"
#include "bacnet/app.h"

static int iam_decode_service_request(uint8_t *pdu, uint16_t pdu_len, BACNET_I_AM_DATA *data)
{
    BACNET_OBJECT_TYPE object_type;
    uint32_t decoded_value;
    int len, dec_len;

    /* OBJECT ID - object id */
    len = decode_application_object_id(pdu, &object_type, &data->device_id);
    if (len < 0) {
        APP_ERROR("%s: invalid object_id\r\n", __func__);
        return BACNET_STATUS_ERROR;
    }
    
    /* MAX APDU - unsigned */
    dec_len = decode_application_unsigned(&pdu[len], &data->max_apdu);
    if (dec_len < 0) {
        APP_ERROR("%s: invalid maxAPDU\r\n", __func__);
        return BACNET_STATUS_ERROR;
    }
    len += dec_len;
    
    /* Segmentation - enumerated */
    dec_len = decode_application_enumerated(&pdu[len], &decoded_value);
    if (dec_len < 0) {
        APP_ERROR("%s: invalid Segmentation support\r\n", __func__);
        return BACNET_STATUS_ERROR;
    }
    len += dec_len;
    data->segmentation = decoded_value;
    
    /* Vendor ID - unsigned16 */
    dec_len = decode_application_unsigned(&pdu[len], &decoded_value);
    if (dec_len < 0) {
        APP_ERROR("%s: invalid vendor_id\r\n", __func__);
        return BACNET_STATUS_ERROR;
    }
    len += dec_len;
    data->vendor_id = decoded_value;

    if (len != pdu_len) {
        APP_ERROR("%s: invalid pdu_len(%d)\r\n", __func__, pdu_len);
        return BACNET_STATUS_ERROR;
    }
    
    return len;
}

static int iam_encode_apdu(bacnet_buf_t *apdu, BACNET_I_AM_DATA *data)
{
    uint8_t *pdu;
    int len;

    pdu = apdu->data;
    pdu[0] = PDU_TYPE_UNCONFIRMED_SERVICE_REQUEST << 4;
    pdu[1] = SERVICE_UNCONFIRMED_I_AM;
    len = 2;

    /* DeviceIdentifier */
    len += encode_application_object_id(&pdu[len], OBJECT_DEVICE, data->device_id);

    /* maxAPDULengthAccepted */
    len += encode_application_unsigned(&pdu[len], data->max_apdu);

    /* segmentationSupported */
    len += encode_application_enumerated(&pdu[len], (uint32_t)(data->segmentation));

    /* vendorID */
    len += encode_application_unsigned(&pdu[len], data->vendor_id);

    apdu->data_len = len;

    return len;
}

/** handler_i_am_add - Handler for I-Am responses.
 *
 * @pdu: [in] The received message to be handled.
 * @src: [in] The BACNET_ADDRESS of the message's source.
 *
 * Will add the responder to our cache, or update its binding.
 *
 */
void handler_i_am(uint8_t *request, uint16_t request_len, bacnet_addr_t *src)
{
    BACNET_I_AM_DATA data;
    int len;

    len = iam_decode_service_request(request, request_len, &data);
    if (len < 0) {
        APP_ERROR("%s: decode service request failed(%d)\r\n", __func__, len);
        return;
    }

    (void)address_add(data.device_id, data.max_apdu, src, false);
    
    APP_VERBOS("%s: receive i am from device(%lu), SNET = %d, MAC = %02X.%02X.%02X.%02X.%02X.%02X\n", __func__,
        (unsigned long)data.device_id, src->net, src->adr[0], src->adr[1], src->adr[2], src->adr[3], 
        src->adr[4], src->adr[5]);

    return;
}

void Send_I_Am(bacnet_addr_t *dst)
{
    BACNET_I_AM_DATA data;
    bacnet_addr_t bcast;
    DECLARE_BACNET_BUF(tx_apdu, MIN_APDU);
    int rv;

    if (client_device) {
        APP_WARN("%s: failed cause device client enabled\r\n", __func__);
        return;
    }

    if (dst == NULL) {
        bcast.net = BACNET_BROADCAST_NETWORK;
        bcast.len = 0;
        dst = &bcast;
    }
    
    data.device_id = device_object_instance_number();
    data.max_apdu = MAX_APDU;
    data.segmentation = SEGMENTATION_NONE;
    data.vendor_id = device_vendor_identifier();
    
    (void)bacnet_buf_init(&tx_apdu.buf, MIN_APDU);
    (void)iam_encode_apdu(&tx_apdu.buf, &data);

    rv = apdu_send(dst, &tx_apdu.buf, PRIORITY_NORMAL, false);
    if (rv < 0) {
        APP_ERROR("%s: apdu send failed(%d)\r\n", __func__, rv);
    }

    return;
}

void Send_I_Am_Remote(uint16_t net)
{
    bacnet_addr_t addr;

    if (client_device) {
        APP_WARN("%s: failed cause device client enabled\r\n", __func__);
        return;
    }
    
    addr.len = 0;
    addr.net = net;

    Send_I_Am(&addr);
}

void Build_I_Am_Service(bacnet_buf_t *buf, uint32_t device_id, uint32_t max_apdu, uint16_t vendor_id,
        BACNET_SEGMENTATION seg_support)
{
    BACNET_I_AM_DATA data;

    if ((buf == NULL) || (buf->data == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return;
    }

    data.device_id = device_id;
    data.max_apdu = max_apdu;
    data.segmentation = seg_support;
    data.vendor_id = vendor_id;

    (void)iam_encode_apdu(buf, &data);
}

