/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bacnet.h
 * Original Author:  linzhixian, 2014-10-21
 *
 *
 * History
 */

#ifndef _BACNET_H_
#define _BACNET_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "misc/cJSON.h"

#define BACNET_APP_CONFIG_FILE              "app.conf"
#define BACNET_NETWORK_CONFIG_FILE          "network.conf"
#define BACNET_RESOURCE_CONFIG_FILE         "resource.conf"

extern cJSON *bacnet_get_app_cfg(void);

extern cJSON *bacnet_get_network_cfg(void);

extern cJSON *bacnet_get_resource_cfg(void);

extern int bacnet_init(void);

extern void bacnet_exit(void);

#ifdef __cplusplus
}
#endif
#endif  /* _BACNET_H_ */

