/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bactext.h
 * Original Author:  linzhixian, 2015-1-27
 *
 * BACnet TEXT
 *
 * History
 */

#ifndef _BACTEXT_H_
#define _BACTEXT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

extern const char *bactext_object_type_name(uint32_t index);

extern const char *bactext_property_name(uint32_t index);

extern const char *bactext_event_state_name(uint32_t index);

extern const char *bactext_engineering_unit_name(uint32_t index);

extern const char *bactext_binary_polarity_name(uint32_t index);

extern const char *bactext_binary_present_value_name(uint32_t index);

extern const char *bactext_reliability_name(uint32_t index);

extern const char *bactext_device_status_name(uint32_t index);

extern const char *bactext_segmentation_name(uint32_t index);

extern const char *bactext_node_type_name(uint32_t index);

extern const char *bactext_day_of_week_name(uint32_t index);

extern const char *bactext_month_name(uint32_t index);

extern const char *bactext_error_class_name(uint32_t index);

extern const char *bactext_error_code_name(uint32_t index);

extern int bactext_tolower(const char *src, char dst[], size_t dst_size);

extern int bactext_get_object_type_from_name(const char *name);

extern const char *bactext_vendor_name(uint16_t vendor_id);

#ifdef __cplusplus
}
#endif

#endif  /* _BACTEXT_H_ */

