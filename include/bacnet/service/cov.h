/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * cov.h
 * Original Author:  linzhixian, 2015-1-15
 *
 * Changing Of Value service
 *
 * History
 */

#ifndef _COV_H_
#define _COV_H_

#include <stdint.h>

#include "bacnet/bacapp.h"
#include "bacnet/bacnet_buf.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct BACnet_Subscribe_COV_Data {
    uint32_t subscriberProcessIdentifier;
    BACNET_OBJECT_ID monitoredObjectIdentifier;
    bool cancellationRequest;                       /* true if this is a cancellation request */
    bool issueConfirmedNotifications;               /* optional */
    uint32_t lifetime;                              /* seconds, optional */
    BACNET_PROPERTY_REFERENCE monitoredProperty;
    bool covIncrementPresent;                       /* true if present */
    float covIncrement;                             /* optional */
} BACNET_SUBSCRIBE_COV_DATA;

extern void handler_ucov_notification(uint8_t *request, uint16_t request_len, bacnet_addr_t *src);

extern bool ucov_notify_encode_init(bacnet_buf_t *apdu, uint32_t subscriber_id,
                uint32_t timeRemaining, BACNET_OBJECT_TYPE object_type, uint32_t object_instance);

extern bool ucov_notify_encode_property(bacnet_buf_t *apdu, BACNET_PROPERTY_ID property_id, 
                uint32_t array_index);

extern bool ucov_notify_encode_end(bacnet_buf_t *apdu);

extern int Send_COV_Subscribe(uint8_t invoke_id, uint32_t device_id,
            BACNET_SUBSCRIBE_COV_DATA *cov_data);

#ifdef __cplusplus
}
#endif

#endif  /* _COV_H_ */

