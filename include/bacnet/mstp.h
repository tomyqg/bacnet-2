/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * mstp.h
 * Original Author:  linzhixian, 2014-6-25
 *
 * mstp����ͷ�ļ�
 *
 * History
 */

#ifndef _MSTP_H_
#define _MSTP_H_

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#include "bacnet/bacdef.h"
#include "misc/cJSON.h"
#include "misc/list.h"
#include "misc/eventloop.h"
#include "bacnet/datalink.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum {
    MSTP_B9600 = 0,
    MSTP_B19200,
    MSTP_B38400,
    MSTP_B57600,
    MSTP_B76800,
    MSTP_B115200,
    MSTP_B_MAX,
} MSTP_BAUDRATE;

struct datalink_mstp_s;

struct slave_proxy_port_s;

/* �û�����ȡ���ݣ�����ֱ�Ӳ��� */
typedef struct datalink_mstp_s {
    datalink_base_t dl;
    struct list_head mstp_list;
    uint8_t mac;
    uint8_t max_master;
    uint8_t max_info_frames;
    uint16_t reply_timeout;
    uint8_t usage_timeout;
    MSTP_BAUDRATE baud;
    uint32_t rx_buf_size;
    uint32_t tx_buf_size;
    uint32_t device_id;         /* ����˽���豸��Ψһֵ�� 0Ϊ��Чֵ����ʾ���豸������˽��Э�鱨�� */
    struct slave_proxy_port_s *proxy;
} datalink_mstp_t;

/**
 * mstp_init - ��ʼ��mstp��·�㹫�в��֣������ڴ���mstp�˿ڶ���ǰ����
 *
 * @return: �ɹ����طǸ�����ʧ�ܷ��ظ���
 *
 */
extern int mstp_init(void);

/**
 * mstp_exit - ����ʼ��mstp��·�㹫�в��֣�����������mstp�˿ڶ����˳�������
 *
 * @return: void
 * 
 */
extern void mstp_exit(void);

/**
 * mstp_startup - mstp��·�㿪ʼ���У�Ӧ������mstp�˿ڶ��󴴽�������
 *
 * @return: �ɹ�����0��ʧ�ܷ��ظ���
 *
 */
extern int mstp_startup(void);

/**
 * mstp_stop - mstp��·��ֹͣ
 *
 * @return: void
 *
 */
extern void mstp_stop(void);

extern void mstp_clean(void);

/**
 * mstp_port_create - ����mstp�˿ڶ���
 *
 * @cfg: �˿�������Ϣ
 *
 * @return: �ɹ����ض˿ڶ���ʧ�ܷ���NULL
 *
 */
extern datalink_mstp_t *mstp_port_create(cJSON *cfg, cJSON *res);

extern int mstp_port_delete(datalink_mstp_t *mstp_port);

/**
 * mstp_next_port - ȡ����һ��mstp�˿ڶ���
 *
 * @prev: ��һ��mstp�˿ڶ�����ΪNULL����ȡ����һ������
 *
 * @return: �ɹ����ض���ָ�룬ʧ�ܷ���NULL
 *
 */
extern datalink_mstp_t *mstp_next_port(datalink_mstp_t *prev);

/**
 * mstp_set_dbg_level - ���õ���״̬
 *
 * @level: ���Կ���
 *
 * @return: void
 *
 */
extern void mstp_set_dbg_level(uint32_t level);

/**
 * mstp_show_dbg_status - �鿴����״̬��Ϣ
 *
 * @return: void
 *
 */
extern void mstp_show_dbg_status(void);

/**
 * mstp_pty_clear_ispty - ���Զ��˽���豸�ı�־
 *
 * @mstp: �˿ڶ���
 * 
 * ÿ��mac��ַ������һ����־��ʾ���Ƿ�Ϊ˽���豸������˽���豸����ø��������reply_delayֵ
 * ���ø��̵�reply_timeout(Ŀǰ���ں�ʵ����reply_delay+2ms)���ԸĽ���������
 *
 * @return: �ɹ�����0��ʧ�ܷ��ظ���
 *
 */
extern int mstp_pty_clear_ispty(datalink_mstp_t *mstp);

/**
 * mstp_pty_set_ispty - ����Զ��˽���豸�ı�־
 *
 * @mstp: �˿ڶ���
 * @ispty: �Ƿ�˽���豸��ÿ����ַ1��bit����λ��ʾΪ˽���豸
 *
 * @return: �ɹ�����0��ʧ�ܷ��ظ���
 *
 */
extern int mstp_pty_set_ispty(datalink_mstp_t *mstp, uint8_t *ispty);

extern int mstp_fake_pdu(datalink_mstp_t *mstp, bacnet_addr_t *dst_mac, uint8_t src_mac, 
            bacnet_buf_t *npdu, bacnet_prio_t prio);

/**
 * return MSTP_B_MAX if invalid baudrate
 */
extern MSTP_BAUDRATE mstp_baudrate2enum(uint32_t baudrate);
/*
 * return -1 if invalid baud
 */
extern int mstp_enum2baudrate(MSTP_BAUDRATE baud);

extern cJSON *mstp_get_status(cJSON *request);

/* return <0 if no result, 0 = fail, 1 = success */
extern int mstp_get_test_result(datalink_mstp_t *mstp, uint8_t *remote);

typedef union mstp_test_result_s {
    struct {
        uint8_t remote_mac;
        bool    success;
    };
    unsigned _u;
} mstp_test_result_t;

/* return <0 if fail */
extern int mstp_test_remote(datalink_mstp_t *mstp, uint8_t remote, uint8_t *data,
        size_t len, void(*callback)(void*, mstp_test_result_t), void *context);

#ifdef __cplusplus
}
#endif

#endif  /* _MSTP_H_ */

