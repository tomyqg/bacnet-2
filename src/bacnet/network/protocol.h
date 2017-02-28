/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * protocol.h
 * Original Author:  linzhixian, 2014-8-20
 *
 * 路由协议模块头文件
 *
 * History
 */

#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include "route.h"
#include "npdu.h"

extern int send_I_Am_Router_To_Network(uint32_t port_id, const uint16_t dnet_list[],
            uint32_t list_num);

extern int send_Who_Is_Router_To_Network(uint32_t port_id, uint16_t dnet);

extern int bcast_Who_Is_Router_To_Network(bacnet_port_t *ex_port, uint16_t dnet);

extern int send_Reject_Message_To_Network(uint32_t in_port, bacnet_addr_t *dst, uint16_t dnet, 
            uint8_t reject_reason);

extern int bcast_Router_Busy_or_Available_To_Network(bacnet_port_t *ex_port,
            bool busy, const uint16_t dnet_list[], uint32_t list_num);

extern int receive_I_Am_Router_To_Network(bacnet_port_t *in_port, bacnet_addr_t *src_mac, 
            bacnet_buf_t *npdu, npci_info_t *npci_info);

extern int receive_Who_Is_Router_To_Network(bacnet_port_t *in_port, bacnet_addr_t *src_addr,
	        bacnet_buf_t *npdu, npci_info_t *npci_info);

extern int receive_Reject_Message_To_Network(bacnet_port_t *in_port, bacnet_addr_t *src_addr,
	        bacnet_buf_t *npdu, npci_info_t *npci_info);

extern int receive_Router_Busy_or_Available_To_Network(bacnet_port_t *in_port, bool busy,
            bacnet_addr_t *src_addr, bacnet_buf_t *npdu, npci_info_t *npci_info);

#endif  /* _PROTOCOL_H_ */

