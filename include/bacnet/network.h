/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * network.h
 * Original Author:  linzhixian, 2014-7-7
 *
 * BACnet��������ͷ�ļ�
 *
 * History
 */

#ifndef _NETWORK_H_
#define _NETWORK_H_

#include "bacnet/bacdef.h"
#include "bacnet/bacnet_buf.h"
#include "misc/cJSON.h"
#include "connect_mng.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * network_send_pdu - ����㷢���ӿ�
 *
 * @dst: Ŀ�ĵ�ַ
 * @buf: ���ͻ�����
 * @prio: �������ȼ�
 * @der: �Ƿ�Ӧ���־��true��ʾ��ҪӦ��false��ʾ����ҪӦ��
 *
 * @return: �ɹ�����0��ʧ�ܷ��ظ���
 *
 */
extern int network_send_pdu(bacnet_addr_t *dst, bacnet_buf_t *buf, bacnet_prio_t prio, bool der);

/**
 * network_receive_pdu - ������հ�����
 *
 * @in_port_id: �������
 * @npdu: ���յ�������㱨��
 * @src_mac: ���ձ��ĵ�Դmac
 *
 * @return: �ɹ�����0��ʧ�ܷ��ظ���
 *
 */
extern int network_receive_pdu(uint32_t in_port_id, bacnet_buf_t *npdu, bacnet_addr_t *src_mac);

/**
 * network_init - ������ʼ��
 *
 * @return: �ɹ�����0��ʧ�ܷ��ظ���
 *
 */
extern int network_init(void);

extern int network_startup(void);

extern void network_stop(void);

/**
 * network_exit - ����㷴��ʼ��
 *
 * @return: void
 *
 */
extern void network_exit(void);

/**
 * network_set_dbg_level - ���õ���״̬
 *
 * @level: ���Կ���
 *
 * @return: void
 *
 */
extern void network_set_dbg_level(uint32_t level);

/**
 * network_show_dbg_status - �鿴����״̬��Ϣ
 *
 * @return: void
 *
 */
extern void network_show_dbg_status(void);

extern void network_show_route_table(void);

/**
 * network_receive_mstp_proxy_pdu - mstp proxy����ר�õİ�������mstp���յ��㲥��ʱ��
 * ������network_receive_pdu, ������ñ��ӿڡ���mstp���send���ϲ����ʱ����Ŀ���ַΪ�㲥��
 * ��Ҳ���ñ��ӿ�
 *
 * @in_port_id: �������
 * @npdu: ���յ�������㱨��
 * @src_mac: ���ձ��ĵ�Դmac
 *
 * @return: �ɹ�����0��ʧ�ܷ��ظ���
 *
 */
extern int network_receive_mstp_proxy_pdu(uint32_t in_port_id, bacnet_buf_t *npdu);

extern int network_mstp_fake(uint32_t port_id, uint8_t mac, bacnet_addr_t *dst, bacnet_buf_t *buf,
            bacnet_prio_t prio);

/**
 * network_number_get - ȡ��datalink port�������
 *
 * @port_id: datalink��id
 * @return: >=0����ţ�<0����
 */
extern int network_number_get(uint32_t port_id);

extern cJSON *network_get_status(connect_info_t *conn, cJSON *request);

extern cJSON *network_get_port_mib(connect_info_t *conn, cJSON *request);

#ifdef __cplusplus
}
#endif

#endif  /* _NETWORK_H_ */

