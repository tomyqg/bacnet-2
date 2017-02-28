/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * reject.h
 * Original Author:  linzhixian, 2015-2-12
 *
 * BACnet Reject Choice
 *
 * History
 */

#ifndef _REJECT_H_
#define _REJECT_H_

#include <stdint.h>

#include "bacnet/bacenum.h"
#include "bacnet/bacdef.h"
#include "bacnet/bacnet_buf.h"

#ifdef __cplusplus
extern "C"
{
#endif

extern BACNET_REJECT_REASON reject_convert_error_code(BACNET_ERROR_CODE error_code);

extern int reject_encode_apdu(bacnet_buf_t *apdu, uint8_t invoke_id, uint8_t reject_reason);

extern void reject_handler(bacnet_buf_t *apdu, bacnet_addr_t *src);

#ifdef __cplusplus
}
#endif

#endif  /* _REJECT_H_ */

