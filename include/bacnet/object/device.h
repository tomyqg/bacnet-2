/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * device.h
 * Original Author:  linzhixian, 2015-1-15
 *
 * Bacnet Device Object
 *
 * History
 */

#ifndef _DEVICE_H_
#define _DEVICE_H_

#include <stdint.h>
#include <stdbool.h>

#include "bacnet/object/object.h"
#include "bacnet/bacstr.h"
#include "bacnet/service/rd.h"
#include "bacnet/service/rp.h"
#include "bacnet/service/rpm.h"
#include "bacnet/service/wp.h"
#include "misc/cJSON.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define BACNET_VENDOR_NAME                  "SWG Automation"

#define BACNET_VENDOR_ID                    (844)

#define DEVICE_MODEL_NAME                   "BACNET-GW"

#define DEVICE_FIRMWARE_REVISION            "1.0"

#define DEVICE_APPLICATION_SOFTWARE_VERSION "1.0"

#define DEVICE_REINITIALIZATION_PASSWORD    "swgtest"

#define MAX_DEV_LOC_LEN                     (64)

extern uint32_t device_object_instance_number(void);

extern object_instance_t* device_get_instance(void);

extern void device_database_revision_increasee(void);

extern property_impl_t* device_impl_extend(BACNET_PROPERTY_ID object_property, property_type_t property_type);

extern bool device_enable_mstp_slave_proxy_support(void);

extern bool device_reinitialize(BACNET_REINITIALIZE_DEVICE_DATA *rd_data);

extern uint16_t device_vendor_identifier(void);

extern int device_init(cJSON *object);

#ifdef __cplusplus
}
#endif

#endif  /* _DEVICE_H_ */

