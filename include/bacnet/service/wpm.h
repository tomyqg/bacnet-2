/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * wpm.h
 * Original Author:  linzhixian, 2015-1-15
 *
 * Write Property Multiple
 *
 * History
 */

#ifndef _WPM_H_
#define _WPM_H_

#include <stdint.h>

#include "bacnet/service/wp.h"
#include "bacnet/bacnet_buf.h"

/* exmaple of do a wpm request apdu encode:
 *
 * DECLARE_BACNET_BUF(pdu, MAX_APDU);
 * bacnet_buf_init(&pdu.buf, MAX_APDU);
 *
 * if (!wpm_req_encode(&pdu.buf, object_type, object_instance)) {
 *      printf("encode failed, the only reason is buffer overflow\r\n");
 *      return;
 * }
 *
 * if (!wpm_req_encode_property(&pdu.buf, property_id, array_index, priority)) {
 *      printf("encode failed, the only reason is buffer overflow\r\n");
 *      return;
 * }
 *
 * int len = encode_xxxx_xxxx(pdu.buf.data + pdu.buf.data_len, ....);
 * if (pdu.buf.data + pdu.buf.data_len + len >= pdu.buf.end) {
 *      printf("my first property value data overflow\r\n");
 *      return;
 * }
 * pdu.buf.data_len += len;
 *
 * if (!wpm_req_encode_property(&pdu.buf, next_property_id, next_array_index, next_priority)) {
 *      printf("encode failed, the only reason is buffer overflow\r\n");
 *      return;
 * }
 *
 * len = encode_xxxx_xxxx(pdu.buf.data + pdu.buf.data_len, ....);
 * if (pdu.buf.data + pdu.buf.data_len + len >= pdu.buf.end) {
 *      printf("my second property value data overflow\r\n");
 *      return;
 * }
 * pdu.buf.data_len += len;
 *
 * // then you could encode other property inside this object, after all, goto next object
 * if (!wpm_req_encode(&pdu.buf, next_object_type, next_object_instance)) {
 *      printf("encode failed, the only reason is buffer overflow\r\n");
 *      return;
 * }
 * // encode properties under this object
 * // after all object
 * if (!wpm_req_encode_end(&pdu.buf, invoke_id)) {
 *      printf("encode failed, the reason maybe buffer overflow\r\n");
 *      return;
 * }
 *
 * int rv = apdu_send_to_device(device_id, &pdu.buf, PRIORITY_NORMAL, true);
 */

#ifdef __cplusplus
extern "C"
{
#endif

extern bool wpm_req_encode_object(bacnet_buf_t *apdu, BACNET_OBJECT_TYPE object_type,
                uint32_t object_instance);

extern bool wpm_req_encode_property(bacnet_buf_t *apdu, BACNET_PROPERTY_ID property_id,
                uint32_t array_index, uint8_t priority);

extern bool wpm_req_encode_end(bacnet_buf_t *apdu, uint8_t invoke_id);

extern void handler_write_property_multiple(BACNET_CONFIRMED_SERVICE_DATA *service_data, 
                bacnet_buf_t *reply_apdu, bacnet_addr_t *src);

#ifdef __cplusplus
}
#endif

#endif  /* _WPM_H_ */

