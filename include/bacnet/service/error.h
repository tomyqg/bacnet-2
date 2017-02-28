/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * error.h
 * Original Author:  linzhixian, 2015-1-15
 *
 * Bacnet Error Choice
 *
 * History
 */

#ifndef _ERROR_H_
#define _ERROR_H_

#include <stdint.h>

#include "bacnet/bacenum.h"
#include "bacnet/bacdef.h"
#include "bacnet/bacnet_buf.h"

#ifdef __cplusplus
extern "C"
{
#endif

extern int bacerror_encode_apdu(bacnet_buf_t *apdu, uint8_t invoke_id, 
            BACNET_CONFIRMED_SERVICE service, BACNET_ERROR_CLASS error_class, 
            BACNET_ERROR_CODE error_code);

extern int bacerror_decode_apdu(bacnet_buf_t *apdu, uint8_t *invoke_id, 
            BACNET_CONFIRMED_SERVICE *service, BACNET_ERROR_CLASS *error_class, 
            BACNET_ERROR_CODE *error_code);

extern void bacerror_handler(bacnet_buf_t *apdu, bacnet_addr_t *src);

#ifdef __cplusplus
}
#endif

#endif  /* _ERROR_H_ */

