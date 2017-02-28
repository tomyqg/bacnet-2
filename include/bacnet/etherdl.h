/*
 * etherdl.h
 *
 *  Created on: Jun 30, 2015
 *      Author: lin
 */

#ifndef INCLUDE_ETHERDL_H_
#define INCLUDE_ETHERDL_H_

#include <stdint.h>
#include <pthread.h>
#include <net/if.h>
#include <linux/if_packet.h>

#include "bacnet/bacdef.h"
#include "misc/cJSON.h"
#include "misc/list.h"
#include "misc/eventloop.h"
#include "bacnet/datalink.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct datalink_ether_s {
    datalink_base_t dl;
    struct list_head ether_list;
    int fd;
    uint8_t mac[6];
    el_watch_t *watch;
} datalink_ether_t;

extern int ether_init(void);

extern void ether_exit(void);

extern int ether_startup(void);

extern void ether_stop(void);

extern void ether_clean(void);

/**
 * ether_port_create - ����bip�ڵ���·�����
 *
 * @cfg: �˿�������Ϣ
 *
 * @return: �ɹ����ض˿ڵ���·�����ָ�룬ʧ�ܷ���NULL
 *
 */
extern datalink_ether_t *ether_port_create(cJSON *cfg, cJSON *res);

extern int ether_port_delete(datalink_ether_t *ether_port);

/**
 * ȡ����һ��ether�˿ڶ���
 *
 * @prev: ��һ��ether�˿ڶ�����ΪNULL��ȡ����һ������
 *
 * @return: �ɹ����ض���ָ�룬ʧ�ܷ���NULL
 */
extern datalink_ether_t *ether_next_port(datalink_ether_t *prev);

extern void ether_set_dbg_level(uint32_t level);

extern cJSON *ether_get_status(cJSON *request);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_ETHERDL_H_ */

