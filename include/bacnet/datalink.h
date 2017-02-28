/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * datalink.h
 * Original Author:  linzhixian, 2014-7-9
 *
 * BACnet数据链路层对外头文件
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
    uint32_t port_id;                       /* 由网络层设置，链路层调用network_receive_pdu时使用 */
    dl_type_t type;                         /* 由datalink_port_create设置 */
    uint16_t max_npdu_len;                  /* 由底层实现设置 */
    uint32_t tx_all;
    uint32_t tx_ok;
    uint32_t rx_all;
    uint32_t rx_ok;
    
    /**
     * @dl: 数据链路层出口信息
     * @dst_mac: 目的MAC地址
     * @npdu: 要发送的网络层报文
     * @prio: 报文优先级
     * @der: 是否应答标志，true表示需要应答，false表示不需要应答
     *
     * 由上层在发包时调用，调用无堵塞
     *
     * @return: 成功返回0，失败返回负数
     */
    int (*send_pdu)(struct datalink_base_s *dl, bacnet_addr_t *dst_mac, bacnet_buf_t *npdu, 
            bacnet_prio_t prio, bool der);
    cJSON *(*get_port_mib)(struct datalink_base_s *dl);
} datalink_base_t;

/**
 * 初始化链路层，必须在datalink_port_create前调用
 */
extern int datalink_init(void);

/**
 * datalink_port_create - 创建链路层端口对象
 *
 * @cfg: 配置信息
 *
 * @return: 成功返回对象指针，失败返回NULL；
 *
 */
extern datalink_base_t* datalink_port_create(cJSON *cfg, cJSON *res);

extern int datalink_port_delete(datalink_base_t *dl_port);

/**
 * 启动各链路层
 */
extern int datalink_startup(void);

/*
 * 停止各链路层
 */
extern void datalink_stop(void);

extern void datalink_clean(void);

/**
 * 停止各链路层，链路层端口对象注销。
 */
extern void datalink_exit(void);

/**
 * datalink_set_dbg_level - 设置调试状态
 *
 * @level: 调试开关
 *
 * @return: void
 *
 */
extern void datalink_set_dbg_level(uint32_t level);

/**
 * datalink_show_dbg_status - 查看调试状态信息
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

