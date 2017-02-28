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
 * query_address_from_device - ��device_id�ҵ�address��LRU����
 * @device_id: Ҫ��ѯ��device_id
 * @addr: bacnet_addr_t*�����صĵ�ַ����ΪNULL
 * @max_apdu: ���ص�max_apdu����ΪNULL
 * @return: �鵽����True
 */
extern bool query_address_from_device(uint32_t device_id, uint32_t *max_apdu, bacnet_addr_t *addr);

/*
 * query_device_from_address - ��bacnet address�ҵ�device id��LRU����
 * @addr: const bacnet_addr_t*��Ҫ��ѯ�ĵ�ַ
 * @device_id: ���ص�device_id����ΪNULL
 * @max_apdu: ���ص�max_apdu����ΪNULL
 * @return: �鵽����True
 */
extern bool query_device_from_address(const bacnet_addr_t *addr, uint32_t *max_apdu,
                uint32_t *device_id);

/**
 * address_get_by_net - ������������������������ַ��������LRU����
 *
 * @net: ƥ������ţ�0xFFFF��ʾƥ�����������
 * @addr: ���ڱ��淵�ص�bacnet address�б�, ��ΪNULL
 * @max_apdu: ���ڱ��淵�ص�max_apdu�б���ΪNULL
 * @size: ָ�����ɷ��ص��б���
 * @return: �����б�����<0Ϊ����
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

