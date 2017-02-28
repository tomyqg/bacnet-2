/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * network.h
 * Original Author:  linzhixian, 2014-7-7
 *
 * BACnet网络层对外头文件
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
 * network_send_pdu - 网络层发包接口
 *
 * @dst: 目的地址
 * @buf: 发送缓冲区
 * @prio: 报文优先级
 * @der: 是否应答标志，true表示需要应答，false表示不需要应答
 *
 * @return: 成功返回0，失败返回负数
 *
 */
extern int network_send_pdu(bacnet_addr_t *dst, bacnet_buf_t *buf, bacnet_prio_t prio, bool der);

/**
 * network_receive_pdu - 网络层收包处理
 *
 * @in_port_id: 报文入口
 * @npdu: 接收到的网络层报文
 * @src_mac: 接收报文的源mac
 *
 * @return: 成功返回0，失败返回负数
 *
 */
extern int network_receive_pdu(uint32_t in_port_id, bacnet_buf_t *npdu, bacnet_addr_t *src_mac);

/**
 * network_init - 网络层初始化
 *
 * @return: 成功返回0，失败返回负数
 *
 */
extern int network_init(void);

extern int network_startup(void);

extern void network_stop(void);

/**
 * network_exit - 网络层反初始化
 *
 * @return: void
 *
 */
extern void network_exit(void);

/**
 * network_set_dbg_level - 设置调试状态
 *
 * @level: 调试开关
 *
 * @return: void
 *
 */
extern void network_set_dbg_level(uint32_t level);

/**
 * network_show_dbg_status - 查看调试状态信息
 *
 * @return: void
 *
 */
extern void network_show_dbg_status(void);

extern void network_show_route_table(void);

/**
 * network_receive_mstp_proxy_pdu - mstp proxy功能专用的包处理。当mstp层收到广播包时，
 * 除调用network_receive_pdu, 还需调用本接口。当mstp层的send被上层调用时，如目标地址为广播，
 * 则也调用本接口
 *
 * @in_port_id: 报文入口
 * @npdu: 接收到的网络层报文
 * @src_mac: 接收报文的源mac
 *
 * @return: 成功返回0，失败返回负数
 *
 */
extern int network_receive_mstp_proxy_pdu(uint32_t in_port_id, bacnet_buf_t *npdu);

extern int network_mstp_fake(uint32_t port_id, uint8_t mac, bacnet_addr_t *dst, bacnet_buf_t *buf,
            bacnet_prio_t prio);

/**
 * network_number_get - 取得datalink port的网络号
 *
 * @port_id: datalink的id
 * @return: >=0网络号，<0错误
 */
extern int network_number_get(uint32_t port_id);

extern cJSON *network_get_status(connect_info_t *conn, cJSON *request);

extern cJSON *network_get_port_mib(connect_info_t *conn, cJSON *request);

#ifdef __cplusplus
}
#endif

#endif  /* _NETWORK_H_ */

