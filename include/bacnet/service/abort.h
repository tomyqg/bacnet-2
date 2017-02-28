/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * abort.h
 * Original Author:  linzhixian, 2015-2-12
 *
 * BACnet Abort Choice
 *
 * History
 */

#ifndef _ABORT_H_
#define _ABORT_H_

#include <stdint.h>
#include <stdbool.h>

#include "bacnet/bacenum.h"
#include "bacnet/bacnet_buf.h"

#ifdef __cplusplus
extern "C"
{
#endif

extern BACNET_ABORT_REASON abort_convert_error_code(BACNET_ERROR_CODE error_code);

extern int abort_encode_apdu(bacnet_buf_t *apdu, uint8_t invoke_id, uint8_t abort_reason, 
            bool server);

#ifdef __cplusplus
}
#endif

#endif  /* _ABORT_H_ */

