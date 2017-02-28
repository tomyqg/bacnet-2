/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * mstp.h
 * Original Author:  linzhixian, 2014-6-25
 *
 * mstp对外头文件
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

/* 用户仅读取数据，不得直接操作 */
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
    uint32_t device_id;         /* 所有私有设备的唯一值， 0为无效值，表示本设备不接受私有协议报文 */
    struct slave_proxy_port_s *proxy;
} datalink_mstp_t;

/**
 * mstp_init - 初始化mstp链路层公有部分，必须在创建mstp端口对象前运行
 *
 * @return: 成功返回非负数，失败返回负数
 *
 */
extern int mstp_init(void);

/**
 * mstp_exit - 反初始化mstp链路层公有部分，必须在所有mstp端口对象退出后运行
 *
 * @return: void
 * 
 */
extern void mstp_exit(void);

/**
 * mstp_startup - mstp链路层开始运行，应在所有mstp端口对象创建后运行
 *
 * @return: 成功返回0，失败返回负数
 *
 */
extern int mstp_startup(void);

/**
 * mstp_stop - mstp链路层停止
 *
 * @return: void
 *
 */
extern void mstp_stop(void);

extern void mstp_clean(void);

/**
 * mstp_port_create - 创建mstp端口对象
 *
 * @cfg: 端口配置信息
 *
 * @return: 成功返回端口对象，失败返回NULL
 *
 */
extern datalink_mstp_t *mstp_port_create(cJSON *cfg, cJSON *res);

extern int mstp_port_delete(datalink_mstp_t *mstp_port);

/**
 * mstp_next_port - 取出下一个mstp端口对象
 *
 * @prev: 上一个mstp端口对象，如为NULL，则取出第一个对象
 *
 * @return: 成功返回对象指针，失败返回NULL
 *
 */
extern datalink_mstp_t *mstp_next_port(datalink_mstp_t *prev);

/**
 * mstp_set_dbg_level - 设置调试状态
 *
 * @level: 调试开关
 *
 * @return: void
 *
 */
extern void mstp_set_dbg_level(uint32_t level);

/**
 * mstp_show_dbg_status - 查看调试状态信息
 *
 * @return: void
 *
 */
extern void mstp_show_dbg_status(void);

/**
 * mstp_pty_clear_ispty - 清除远程私有设备的标志
 *
 * @mstp: 端口对象
 * 
 * 每个mac地址，都有一个标志表示其是否为私有设备，如是私有设备则采用根据自身的reply_delay值
 * 采用更短的reply_timeout(目前的内核实现是reply_delay+2ms)，以改进网络性能
 *
 * @return: 成功返回0，失败返回负数
 *
 */
extern int mstp_pty_clear_ispty(datalink_mstp_t *mstp);

/**
 * mstp_pty_set_ispty - 设置远程私有设备的标志
 *
 * @mstp: 端口对象
 * @ispty: 是否私有设备，每个地址1个bit，置位表示为私有设备
 *
 * @return: 成功返回0，失败返回负数
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

