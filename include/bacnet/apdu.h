/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * apdu.h
 * Original Author:  linzhixian, 2015-1-12
 *
 * apdu.h
 *
 * History
 */

#ifndef _APDU_H_
#define _APDU_H_

#include <stdint.h>
#include <stdbool.h>

#include "bacnet/bacnet_buf.h"
#include "bacnet/bacdef.h"
#include "bacnet/bacenum.h"
#include "bacnet/bacstr.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct confirmed_service_data {
    bool segmented_message;
    bool more_follows;
    bool segmented_response_accepted;
    unsigned max_segs;
    unsigned max_resp;
    uint8_t invoke_id;
    uint8_t sequence_number;
    uint8_t proposed_window_number;
    uint8_t service_choice;
    uint8_t* service_request;
    uint16_t service_request_len;
    uint16_t pci_len;
} BACNET_CONFIRMED_SERVICE_DATA;

typedef struct confirmed_service_ack_data {
    bool segmented_message;
    bool more_follows;
    uint8_t invoke_id;
    uint8_t sequence_number;
    uint8_t proposed_window_number;
    uint8_t service_choice;
    uint8_t *service_data;
    uint32_t service_data_len;
} BACNET_CONFIRMED_SERVICE_ACK_DATA;

typedef struct segment_ack_data {
    bool negative_ack;
    bool server;
    uint8_t invoke_id;
    uint8_t sequence_number;
    uint8_t actual_window_size;
} BACNET_SEGMENT_ACK_DATA;

typedef void (*unconfirmed_service_handler)(uint8_t *request, uint16_t request_len, 
                bacnet_addr_t *src);

typedef void (*confirmed_service_handler)(BACNET_CONFIRMED_SERVICE_DATA *service_data,
                bacnet_buf_t *reply_apdu, bacnet_addr_t *src);

extern void apdu_set_confirmed_handler(BACNET_CONFIRMED_SERVICE service_choice, 
                confirmed_service_handler pFunction);

extern void apdu_set_unconfirmed_handler(BACNET_UNCONFIRMED_SERVICE service_choice,
                unconfirmed_service_handler pFunction);

extern void apdu_get_service_supported(BACNET_BIT_STRING *services);

extern void apdu_set_default_service_handler(void);

extern confirmed_service_handler apdu_find_confirmed_handler(BACNET_CONFIRMED_SERVICE service_choice);

extern unconfirmed_service_handler apdu_find_unconfirmed_handler(BACNET_UNCONFIRMED_SERVICE service_choice);

extern void apdu_handler(bacnet_buf_t *apdu, bool der, bacnet_buf_t *reply_apdu,
                bacnet_addr_t *src);

extern int apdu_decode_complex_ack(bacnet_buf_t *apdu, BACNET_CONFIRMED_SERVICE_ACK_DATA *ack);

extern int apdu_encode_confirmed_service_request(bacnet_buf_t *apdu, uint8_t invoke_id, uint8_t service_choice);

extern int apdu_send_to_device(uint32_t device_id, bacnet_buf_t *apdu, bacnet_prio_t prio, bool der);

extern int apdu_send(bacnet_addr_t *dst, bacnet_buf_t *apdu, bacnet_prio_t prio, bool der);

extern void apdu_mstp_proxy_handler(bacnet_buf_t *apdu, uint16_t src_net, uint16_t in_net);

#ifdef __cplusplus
}
#endif

#endif  /* _APDU_H_ */

