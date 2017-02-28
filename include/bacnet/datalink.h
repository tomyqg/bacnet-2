/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * datalink.h
 * Original Author:  linzhixian, 2014-7-9
 *
 * BACnet������·�����ͷ�ļ�
 *
 * History
 */

#ifndef _DATALINK_H_
#define _DATALINK_H_

#include <stdbool.h>

#include "misc/cJSON.h"
#include "bacnet/bacdef.h"
#include "bacnet/bacnet_buf.h"
#include "connect_mng.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum dl_type_e {
    DL_PTP = 0,
    DL_BIP = 1,
    DL_MSTP = 2,
    DL_ARCNET = 3,
    DL_LONTALK = 4,
    DL_ETHERNET = 5
} dl_type_t;

typedef struct datalink_base_s {
    uint32_t port_id;                       /* ����������ã���·�����network_receive_pduʱʹ�� */
    dl_type_t type;                         /* ��datalink_port_create���� */
    uint16_t max_npdu_len;                  /* �ɵײ�ʵ������ */
    uint32_t tx_all;
    uint32_t tx_ok;
    uint32_t rx_all;
    uint32_t rx_ok;
    
    /**
     * @dl: ������·�������Ϣ
     * @dst_mac: Ŀ��MAC��ַ
     * @npdu: Ҫ���͵�����㱨��
     * @prio: �������ȼ�
     * @der: �Ƿ�Ӧ���־��true��ʾ��ҪӦ��false��ʾ����ҪӦ��
     *
     * ���ϲ��ڷ���ʱ���ã������޶���
     *
     * @return: �ɹ�����0��ʧ�ܷ��ظ���
     */
    int (*send_pdu)(struct datalink_base_s *dl, bacnet_addr_t *dst_mac, bacnet_buf_t *npdu, 
            bacnet_prio_t prio, bool der);
    cJSON *(*get_port_mib)(struct datalink_base_s *dl);
} datalink_base_t;

/**
 * ��ʼ����·�㣬������datalink_port_createǰ����
 */
extern int datalink_init(void);

/**
 * datalink_port_create - ������·��˿ڶ���
 *
 * @cfg: ������Ϣ
 *
 * @return: �ɹ����ض���ָ�룬ʧ�ܷ���NULL��
 *
 */
extern datalink_base_t* datalink_port_create(cJSON *cfg, cJSON *res);

extern int datalink_port_delete(datalink_base_t *dl_port);

/**
 * ��������·��
 */
extern int datalink_startup(void);

/*
 * ֹͣ����·��
 */
extern void datalink_stop(void);

extern void datalink_clean(void);

/**
 * ֹͣ����·�㣬��·��˿ڶ���ע����
 */
extern void datalink_exit(void);

/**
 * datalink_set_dbg_level - ���õ���״̬
 *
 * @level: ���Կ���
 *
 * @return: void
 *
 */
extern void datalink_set_dbg_level(uint32_t level);

/**
 * datalink_show_dbg_status - �鿴����״̬��Ϣ
 *
 * @return: void
 *
 */
extern void datalink_show_dbg_status(void);

extern const char *datalink_get_type_by_resource_name(cJSON *res, char *name);

extern const char *datalink_get_ifname_by_resource_name(cJSON *res, char *name);

extern cJSON *datalink_get_status(connect_info_t *conn, cJSON *request);

extern cJSON *datalink_get_mib(datalink_base_t *dl_port);

#ifdef __cplusplus
}
#endif

#endif  /* _DATALINK_H_ */

