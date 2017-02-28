/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * apdu.c
 * Original Author:  linzhixian, 2015-1-12
 *
 * APDU
 *
 * History
 */

#include <errno.h>

#include "bacnet/apdu.h"
#include "bacnet/service/rp.h"
#include "bacnet/service/wp.h"
#include "bacnet/service/rpm.h"
#include "bacnet/service/wpm.h"
#include "bacnet/service/rd.h"
#include "bacnet/service/dcc.h"
#include "bacnet/service/cov.h"
#include "bacnet/service/whohas.h"
#include "bacnet/service/whois.h"
#include "bacnet/service/ihave.h"
#include "bacnet/service/iam.h"
#include "bacnet/service/timesync.h"
#include "bacnet/object/device.h"
#include "bacnet/bacdcode.h"
#include "bacnet/datetime.h"
#include "bacnet/service/error.h"
#include "bacnet/service/abort.h"
#include "bacnet/service/reject.h"
#include "bacnet/bacdef.h"
#include "bacnet/tsm.h"
#include "bacnet/app.h"
#include "bacnet/addressbind.h"
#include "bacnet/network.h"
#include "bacnet/slaveproxy.h"

extern bool is_app_exist;

extern bool client_device;

static BACNET_SERVICES_SUPPORTED confirmed_service_supported[MAX_BACNET_CONFIRMED_SERVICE] = {
    SERVICE_SUPPORTED_ACKNOWLEDGE_ALARM,
    SERVICE_SUPPORTED_CONFIRMED_COV_NOTIFICATION,
    SERVICE_SUPPORTED_CONFIRMED_EVENT_NOTIFICATION,
    SERVICE_SUPPORTED_GET_ALARM_SUMMARY,
    SERVICE_SUPPORTED_GET_ENROLLMENT_SUMMARY,
    SERVICE_SUPPORTED_SUBSCRIBE_COV,
    SERVICE_SUPPORTED_ATOMIC_READ_FILE,
    SERVICE_SUPPORTED_ATOMIC_WRITE_FILE,
    SERVICE_SUPPORTED_ADD_LIST_ELEMENT,
    SERVICE_SUPPORTED_REMOVE_LIST_ELEMENT,
    SERVICE_SUPPORTED_CREATE_OBJECT,
    SERVICE_SUPPORTED_DELETE_OBJECT,
    SERVICE_SUPPORTED_READ_PROPERTY,
    SERVICE_SUPPORTED_READ_PROP_CONDITIONAL,
    SERVICE_SUPPORTED_READ_PROP_MULTIPLE,
    SERVICE_SUPPORTED_WRITE_PROPERTY,
    SERVICE_SUPPORTED_WRITE_PROP_MULTIPLE,
    SERVICE_SUPPORTED_DEVICE_COMMUNICATION_CONTROL,
    SERVICE_SUPPORTED_PRIVATE_TRANSFER,
    SERVICE_SUPPORTED_TEXT_MESSAGE,
    SERVICE_SUPPORTED_REINITIALIZE_DEVICE,
    SERVICE_SUPPORTED_VT_OPEN,
    SERVICE_SUPPORTED_VT_CLOSE,
    SERVICE_SUPPORTED_VT_DATA,
    SERVICE_SUPPORTED_AUTHENTICATE,
    SERVICE_SUPPORTED_REQUEST_KEY,
    SERVICE_SUPPORTED_READ_RANGE,
    SERVICE_SUPPORTED_LIFE_SAFETY_OPERATION,
    SERVICE_SUPPORTED_SUBSCRIBE_COV_PROPERTY,
    SERVICE_SUPPORTED_GET_EVENT_INFORMATION
};

/* a simple table for crossing the services supported */
static BACNET_SERVICES_SUPPORTED unconfirmed_service_supported[MAX_BACNET_UNCONFIRMED_SERVICE] = {
    SERVICE_SUPPORTED_I_AM,
    SERVICE_SUPPORTED_I_HAVE,
    SERVICE_SUPPORTED_UNCONFIRMED_COV_NOTIFICATION,
    SERVICE_SUPPORTED_UNCONFIRMED_EVENT_NOTIFICATION,
    SERVICE_SUPPORTED_UNCONFIRMED_PRIVATE_TRANSFER,
    SERVICE_SUPPORTED_UNCONFIRMED_TEXT_MESSAGE,
    SERVICE_SUPPORTED_TIME_SYNCHRONIZATION,
    SERVICE_SUPPORTED_WHO_HAS,
    SERVICE_SUPPORTED_WHO_IS,
    SERVICE_SUPPORTED_UTC_TIME_SYNCHRONIZATION,
    SERVICE_SUPPORTED_WRITE_GROUP
};

/* Confirmed Function Handlers */
/* If they are not set, they are handled by a reject message */
static confirmed_service_handler Confirmed_Function[MAX_BACNET_CONFIRMED_SERVICE] = {NULL, };

static unconfirmed_service_handler Unconfirmed_Function[MAX_BACNET_UNCONFIRMED_SERVICE] = {NULL, };

static uint8_t __supported_service_bits[(MAX_BACNET_SERVICE_SUPPORTED + 7) >> 3] = {0,};

void apdu_get_service_supported(BACNET_BIT_STRING *services)
{
    bitstring_init(services, __supported_service_bits, MAX_BACNET_SERVICE_SUPPORTED);
}

void apdu_set_confirmed_handler(BACNET_CONFIRMED_SERVICE service_choice,
        confirmed_service_handler pFunction)
{
    BACNET_BIT_STRING services;
    
    if (service_choice < MAX_BACNET_CONFIRMED_SERVICE) {
        Confirmed_Function[service_choice] = pFunction;
        bitstring_init(&services, __supported_service_bits, MAX_BACNET_SERVICE_SUPPORTED);
        bitstring_set_bit(&services, confirmed_service_supported[service_choice], pFunction != NULL);
    }
}

void apdu_set_unconfirmed_handler(BACNET_UNCONFIRMED_SERVICE service_choice,
        unconfirmed_service_handler pFunction)
{
    BACNET_BIT_STRING services;
    
    if (service_choice < MAX_BACNET_UNCONFIRMED_SERVICE) {
        Unconfirmed_Function[service_choice] = pFunction;
        bitstring_init(&services, __supported_service_bits, MAX_BACNET_SERVICE_SUPPORTED);
        bitstring_set_bit(&services, unconfirmed_service_supported[service_choice], pFunction != NULL);
    }
}

confirmed_service_handler apdu_find_confirmed_handler(BACNET_CONFIRMED_SERVICE service_choice)
{
    if (service_choice < MAX_BACNET_CONFIRMED_SERVICE) {
        return Confirmed_Function[service_choice];
    }
    
    return NULL;
}

unconfirmed_service_handler apdu_find_unconfirmed_handler(BACNET_UNCONFIRMED_SERVICE service_choice)
{
    if (service_choice < MAX_BACNET_UNCONFIRMED_SERVICE) {
        return Unconfirmed_Function[service_choice];
    }
    
    return NULL;
}

/**
 * set all we supported service handler
 */
void apdu_set_default_service_handler(void)
{
    if (client_device) {
        apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_I_AM, handler_i_am);
        apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_I_HAVE, handler_i_have);
        return;
    }

    apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_PROPERTY, handler_read_property);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_WRITE_PROPERTY, handler_write_property);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_PROP_MULTIPLE, handler_read_property_multiple);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_WRITE_PROP_MULTIPLE, handler_write_property_multiple);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_REINITIALIZE_DEVICE, handler_reinitialize_device);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_RANGE, handler_read_range);
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_DEVICE_COMMUNICATION_CONTROL, 
        handler_device_communication_control);

    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_I_AM, handler_i_am);
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_I_HAVE, handler_i_have);
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_IS, handler_who_is);
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_HAS, handler_who_has);
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_TIME_SYNCHRONIZATION, handler_timesync);
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_COV_NOTIFICATION, handler_ucov_notification);
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_UTC_TIME_SYNCHRONIZATION, handler_timesync_utc);
}

/* When network communications are completely disabled,
   only DeviceCommunicationControl and ReinitializeDevice APDUs
   shall be processed and no messages shall be initiated.
   When the initiation of communications is disabled, 
   all APDUs shall be processed and responses returned as 
   required... */
static bool apdu_confirmed_dcc_disabled(uint8_t service_choice)
{
    bool status;

    status = false;
    
    if (dcc_communication_disabled()) {
        switch (service_choice) {
        case SERVICE_CONFIRMED_DEVICE_COMMUNICATION_CONTROL:
        case SERVICE_CONFIRMED_REINITIALIZE_DEVICE:
            break;

        default:
            status = true;
            break;
        }
    }

    return status;
}

/* When network communications are completely disabled,
   only DeviceCommunicationControl and ReinitializeDevice APDUs
   shall be processed and no messages shall be initiated. */
/* If the request is valid and the 'Enable/Disable' parameter is
   DISABLE_INITIATION, the responding BACnet-user shall
   discontinue the initiation of messages except for I-Am
   requests issued in accordance with the Who-Is service procedure.*/
static bool apdu_unconfirmed_dcc_disabled(uint8_t service_choice)
{
    bool status;

    status = false;
    
    if (dcc_communication_disabled()) {
        status = true;
    } else if (dcc_communication_initiation_disabled()) {
        /* WhoIs will be processed and I-Am initiated as response. */
        switch (service_choice) {
        case SERVICE_UNCONFIRMED_WHO_IS:
            break;

        default:
            status = true;
            break;
        }
    }

    return status;
}

static int apdu_decode_confirmed_service_request(bacnet_buf_t *apdu, 
            BACNET_CONFIRMED_SERVICE_DATA *service_data)
{
    uint8_t *pdu;
    uint16_t len;
    
    pdu = apdu->data;
    service_data->segmented_message = pdu[0] & BIT3? true: false;
    service_data->more_follows = pdu[0] & BIT2? true: false;
    service_data->segmented_response_accepted = pdu[0] & BIT1? true: false;
    service_data->max_segs = decode_max_segs(pdu[1]);
    service_data->max_resp = decode_max_apdu(pdu[1]);
    service_data->invoke_id = pdu[2];

    len = 3;
    if (service_data->segmented_message) {
        service_data->sequence_number = pdu[len++];
        service_data->proposed_window_number = pdu[len++];
    }

    service_data->service_choice = pdu[len++];

    if (len > apdu->data_len) {
        APP_WARN("%s: buffer overflow\r\n", __func__);
        return -EPERM;
    }

    service_data->service_request = &pdu[len];
    service_data->service_request_len = apdu->data_len - len;
    
    service_data->pci_len = len;

    return OK;
}

int apdu_encode_confirmed_service_request(bacnet_buf_t *apdu, uint8_t invoke_id, uint8_t service_choice)
{
    uint8_t *pdu;

    if (!apdu) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return -EPERM;
    }

    if (apdu->data + apdu->data_len + 4 > apdu->end) {
        APP_ERROR("%s: buffer overflow\r\n", __func__);
        return -EINVAL;
    }

    pdu = apdu->data;
    pdu[0] = PDU_TYPE_CONFIRMED_SERVICE_REQUEST;
    pdu[1] = encode_max_segs_max_apdu(0, MAX_APDU);
    pdu[2] = invoke_id;
    pdu[3] = service_choice;

    apdu->data_len += 4;
    
    return 4;
}

int apdu_decode_complex_ack(bacnet_buf_t *apdu, BACNET_CONFIRMED_SERVICE_ACK_DATA *ack)
{
    uint8_t *pdu;
    int len;

    if (!apdu || !ack) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return -EPERM;
    }

    pdu = apdu->data;

    ack->segmented_message = (pdu[0] & BIT3)? true: false;
    ack->more_follows = (pdu[0] & BIT2)? true: false;
    ack->invoke_id = pdu[1];
    len = 2;

    if (ack->segmented_message) {
        ack->sequence_number = pdu[len++];
        ack->proposed_window_number = pdu[len++];
    }

    ack->service_choice = pdu[len++];

    if (len > apdu->data_len) {
        APP_WARN("%s: buffer overflow\r\n", __func__);
        return -EPERM;
    }

    ack->service_data = apdu->data + len;
    ack->service_data_len = apdu->data_len - len;

    return OK;
}

static int apdu_decode_segment_ack(bacnet_buf_t *apdu, BACNET_SEGMENT_ACK_DATA *ack)
{
    uint8_t *pdu;

    if (!apdu || !ack) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return -EPERM;
    }

    if (apdu->data_len != 4) {
        APP_WARN("%s: invalid data len(%d)\r\n", __func__, apdu->data_len);
        return -EPERM;
    }

    pdu = apdu->data;
    ack->negative_ack = (pdu[0] & BIT1) ? true : false;
    ack->server = (pdu[0] & BIT0) ? true : false;

    ack->invoke_id = pdu[1];
    ack->sequence_number = pdu[2];
    ack->actual_window_size = pdu[3];

    return OK;
}

/**
 * apdu_handler - 应用层收帧处理
 *
 * @apdu: 应用层报文
 * @der: 报文应答标识
 * @reply_apdu: 如果der为true则返回应答报文，否则返回NULL
 *
 * @return: void
 *
 */
void apdu_handler(bacnet_buf_t *apdu, bool der, bacnet_buf_t *reply_apdu, bacnet_addr_t *src)
{
    BACNET_CONFIRMED_SERVICE_DATA service_data;
    BACNET_SEGMENT_ACK_DATA seg_ack;
    confirmed_service_handler confirmed_handler;
    unconfirmed_service_handler unconfirmed_handler;
    uint8_t service_choice;
    BACNET_PDU_TYPE apdu_type;
    int rv;
    
    if ((apdu == NULL) || (apdu->data == NULL) || (apdu->data_len == 0) || (reply_apdu == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return;
    }

    if (!is_app_exist) {
        return;
    }

    if (app_dbg_verbos) {
        printf("\r\napdu_handler: receive apdu from SMAC: ");
        PRINT_BACNET_ADDRESS(src);
    }

    apdu_type = apdu->data[0] >> 4;
    
    switch (apdu_type) {
    case PDU_TYPE_CONFIRMED_SERVICE_REQUEST:
        rv = apdu_decode_confirmed_service_request(apdu, &service_data);
        if (rv < 0) {
            APP_WARN("%s: decode confirmed_service_request failed(%d)\r\n", __func__, rv);
            return;
        }

        if (apdu_confirmed_dcc_disabled(service_data.service_choice)) {
            APP_WARN("%s: network communications are disabled\r\n", __func__);
            break;
        }

        if (service_data.segmented_message) {
            /* send an abort cause we don't support segmentation */
            APP_WARN("%s: not support segmented message\r\n", __func__);
            abort_encode_apdu(reply_apdu, service_data.invoke_id,
                ABORT_REASON_SEGMENTATION_NOT_SUPPORTED, true);
            break;
        }

        confirmed_handler = apdu_find_confirmed_handler(service_data.service_choice);
        if (confirmed_handler) {
            if (service_data.max_resp < MAX_APDU) {
                bacnet_buf_resize(reply_apdu, service_data.max_resp);
            }
            confirmed_handler(&service_data, reply_apdu, src);
        } else {
            /* send a reject cause we don't support this choice */
            APP_WARN("%s: unsupported confirmed service choice(%d)\r\n", __func__,
                service_data.service_choice);
            (void)reject_encode_apdu(reply_apdu, service_data.invoke_id,
                REJECT_REASON_UNRECOGNIZED_SERVICE);
        }
        break;

    case PDU_TYPE_UNCONFIRMED_SERVICE_REQUEST:
        if (apdu->data_len < 2) {
            APP_WARN("%s: invalid unconfirmed request pdu len(%d)\r\n", __func__, apdu->data_len);
            break;
        }
        
        service_choice = apdu->data[1];
        if (apdu_unconfirmed_dcc_disabled(service_choice)) {
            APP_WARN("%s: network communications are disabled\r\n", __func__);
            break;
        }
        
        unconfirmed_handler = apdu_find_unconfirmed_handler(service_choice);
        if (unconfirmed_handler) {
            unconfirmed_handler(&apdu->data[2], apdu->data_len - 2, src);
        } else {
            APP_WARN("%s: unsupported unconfirmed service choice(%d)\r\n", __func__, service_choice);
        }
        break;

    case PDU_TYPE_SIMPLE_ACK:
    case PDU_TYPE_COMPLEX_ACK:
        if (apdu->data[0] & BIT3) {
            APP_WARN("%s: receive an unexpected segment complex-ack\r\n", __func__);
            break;
        }
    case PDU_TYPE_ERROR:
    case PDU_TYPE_REJECT:
    case PDU_TYPE_ABORT:
        tsm_invoker_callback(src, apdu, apdu_type);
        break;

    case PDU_TYPE_SEGMENT_ACK:
        rv = apdu_decode_segment_ack(apdu, &seg_ack);
        if (rv < 0) {
            APP_WARN("%s: decode segment_ack failed(%d)\r\n", __func__, rv);
            break;
        }

        APP_WARN("%s: segment ack received, but we don't support segment\r\n", __func__);
        abort_encode_apdu(reply_apdu, seg_ack.invoke_id, ABORT_REASON_SEGMENTATION_NOT_SUPPORTED,
            !seg_ack.server);
        break;
    
    default:
        APP_WARN("%s: unsupported apdu type(%d)\r\n", __func__, apdu_type);
        break;
    }

    return;
}

int apdu_send(bacnet_addr_t *dst, bacnet_buf_t *apdu, bacnet_prio_t prio, bool der)
{
    int rv;

    if ((apdu == NULL) || (dst == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    if (!is_app_exist) {
        APP_ERROR("%s: application layer do not exist\r\n", __func__);
        return -EPERM;
    }

    if (!dcc_communication_enabled()) {
        APP_ERROR("%s: dcc communication disabled\r\n", __func__);
        return -EPERM;
    }

    if (der && (dst->net == BACNET_BROADCAST_NETWORK || dst->len == 0)) {
        APP_ERROR("%s: der apdu should not broadcast\r\n", __func__);
        return -EINVAL;
    }

    rv = network_send_pdu(dst, apdu, prio, der);
    if (rv < 0) {
        APP_ERROR("%s: network send failed(%d)\r\n", __func__, rv);
    }

    return rv;
}

int apdu_send_to_device(uint32_t device_id, bacnet_buf_t *apdu, bacnet_prio_t prio, bool der)
{
    bacnet_addr_t dst;
    uint32_t max_apdu;

    if (apdu == NULL) {
        APP_ERROR("%s: null apdu\r\n", __func__);
        return -EINVAL;
    }

    if (!query_address_from_device(device_id, &max_apdu, &dst)) {
        APP_ERROR("%s: get address from device(%d) failed\r\n", __func__, device_id);
        return -EPERM;
    }

    if (apdu->data_len > max_apdu) {
        APP_ERROR("%s: too long apdu to send\r\n", __func__);
        return -EINVAL;
    }

    return apdu_send(&dst, apdu, prio, der);
}

void apdu_mstp_proxy_handler(bacnet_buf_t *apdu, uint16_t src_net, uint16_t in_net)
{
    BACNET_PDU_TYPE apdu_type;
    uint8_t service_choice;
    unsigned low_limit, high_limit;

    if ((apdu == NULL) || (apdu->data == NULL) || (apdu->data_len == 0)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return;
    }

    apdu_type = apdu->data[0] >> 4;

    if (apdu_type != PDU_TYPE_UNCONFIRMED_SERVICE_REQUEST) {
        return;
    }

    if (apdu->data_len < 2) {
        return;
    }

    service_choice = apdu->data[1];
    if (service_choice != SERVICE_UNCONFIRMED_WHO_IS) {
        return;
    }

    if (whois_decode_service_request(&apdu->data[2], apdu->data_len - 2,
            &low_limit, &high_limit) < 0) {
        return;
    }

    remote_proxied_whois(src_net, in_net, low_limit, high_limit);
}
