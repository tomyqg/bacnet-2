/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * web_service.h
 * Original Author:  linzhixian, 2015-9-14
 *
 * Web Service
 *
 * History
 */

#ifndef _WEB_SERVICE_H_
#define _WEB_SERVICE_H_

#include <stdint.h>

#include "misc/cJSON.h"
#include "connect_mng.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define WEB_LIST_METHOD                             "list_method"
#define WEB_READ_DEVICE_OBJECT_LIST                 "read device object list"
#define WEB_READ_DEVICE_OBJECT_PROPERTY_LIST        "read device object property list"
#define WEB_READ_DEVICE_OBJECT_PROPERTY_VALUE       "read device object property value"
#define WEB_WRITE_DEVICE_OBJECT_PROPERTY_VALUE      "write device object property value"
#define WEB_READ_DEVICE_ADDRESS_BINDING             "read device address binding"
#define WEB_SEND_WHO_IS                             "send who is"
#define WEB_GET_APP_STATUS                          "get app status"
#define WEB_GET_NETWORK_STATUS                      "get network status"
#define WEB_GET_DATALINK_STATUS                     "get datalink status"
#define WEB_GET_PORT_MIB                            "get port mib"

/*
 * return cJSON with "result" or "error" item
 * return NULL if delayed reply or dropped connection
 */
typedef cJSON *(*web_service_handler)(connect_info_t *conn, cJSON *request);

extern int web_service_init(void);

extern void web_service_exit(void);

extern int web_service_register(const char *type, web_service_handler handler);

extern void web_service_unregister(char *type);

extern void web_service_set_dbg_level(uint32_t level);

#ifdef __cplusplus
}
#endif

#endif /* _WEB_SERVICE_H_ */

