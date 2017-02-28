/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * web_request.h
 * Original Author:  linzhixian, 2015-9-14
 *
 * Web Request
 *
 * History
 */

#ifndef _WEB_REQUEST_H_
#define _WEB_REQUEST_H_

#include "misc/cJSON.h"
#include "connect_mng.h"

extern cJSON *web_send_who_is(connect_info_t *conn, cJSON *request);

extern cJSON *web_read_device_object_list(connect_info_t *conn, cJSON *request);

extern cJSON *web_read_device_object_property_list(connect_info_t *conn, cJSON *request);

extern cJSON *web_read_device_object_property_value(connect_info_t *conn, cJSON *request);

extern cJSON *web_write_device_object_property_value(connect_info_t *conn, cJSON *request);

extern cJSON *web_read_device_address_binding(connect_info_t *conn, cJSON *request);

#endif /* _WEB_REQUEST_H_ */

