/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * addressbind.h
 * Original Author:  linzhixian, 2015-2-13
 *
 * BACnet Address Binding
 *
 * History
 */

#ifndef _ADDRESSBIND_H_
#define _ADDRESSBIND_H_

#include <stdint.h>
#include <stdbool.h>

#include "bacnet/bacdef.h"
#include "bacnet/service/rp.h"
#include "bacnet/service/rr.h"
#include "misc/cJSON.h"

#ifdef __cplusplus
extern "C"
{
#endif

extern int address_add(uint32_t device_id, uint32_t max_apdu, const bacnet_addr_t *addr, 
            bool is_static);

extern void address_delete(uint32_t device_id);

extern void address_destroy(void);

/*
 * query_address_from_device - 从device_id找到address，LRU排序
 * @device_id: 要查询的device_id
 * @addr: bacnet_addr_t*，返回的地址，可为NULL
 * @max_apdu: 返回的max_apdu，可为NULL
 * @return: 查到返回True
 */
extern bool query_address_from_device(uint32_t device_id, uint32_t *max_apdu, bacnet_addr_t *addr);

/*
 * query_device_from_address - 从bacnet address找到device id，LRU排序
 * @addr: const bacnet_addr_t*，要查询的地址
 * @device_id: 返回的device_id，可为NULL
 * @max_apdu: 返回的max_apdu，可为NULL
 * @return: 查到返回True
 */
extern bool query_device_from_address(const bacnet_addr_t *addr, uint32_t *max_apdu,
                uint32_t *device_id);

/**
 * address_get_by_net - 根据网络号搜索出所有网络地址，不触发LRU排序
 *
 * @net: 匹配网络号，0xFFFF表示匹配所有网络号
 * @addr: 用于保存返回的bacnet address列表, 可为NULL
 * @max_apdu: 用于保存返回的max_apdu列表，可为NULL
 * @size: 指出最大可返回的列表数
 * @return: 返回列表数，<0为错误
 */
extern int address_get_by_net(uint16_t net, uint32_t *max_apdu, bacnet_addr_t *addr, unsigned size);

/*
 * return encode length, BACNET_STATUS_ERROR, BACNET_STATUS_ABORT
 */
extern int read_address_binding(BACNET_READ_PROPERTY_DATA *rp_data, RR_RANGE *range);

extern int address_init(cJSON *cfg);

extern void address_exit(void);

extern void whois_destroy(void);

extern cJSON *get_address_binding(void);

#ifdef __cplusplus
}
#endif

#endif  /* _ADDRESSBIND_H_ */

