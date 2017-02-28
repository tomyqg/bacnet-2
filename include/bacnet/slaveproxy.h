/*
 * Copyright(C) 2014 SWG. All rights reserved.
 *
 * slaveproxy.h
 * Original Author:  lincheng, 2015-5-28
 *
 * BACnet mstp slave proxy
 *
 * History
 */

#ifndef _SLAVEPROXY_H_
#define _SLAVEPROXY_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "bacnet/bacdef.h"
#include "misc/cJSON.h"
#include "misc/list.h"
#include "misc/rbtree.h"
#include "bacnet/bacenum.h"
#include "bacnet/mstp.h"
#include "bacnet/service/rp.h"
#include "bacnet/tsm.h"
#include "misc/eventloop.h"

#ifdef __cplusplus
extern "C"
{
#endif

extern int slave_proxy_port_number(void);

/*
 * slave_proxy_enable - 启动mstp端口的slave proxy功能及auto discovery功能
 */
extern int slave_proxy_enable(uint32_t mstp_idx, bool enable, bool auto_discovery);

extern int slave_proxy_get(uint32_t mstp_idx, bool *enable, bool *auto_discovery);

extern int read_slave_binding(BACNET_READ_PROPERTY_DATA *rp_data);

extern int slave_binding_set(uint8_t *pdu, uint32_t pdu_len);

extern int read_manual_binding(BACNET_READ_PROPERTY_DATA *rp_data);

extern int manual_binding_set(uint8_t *pdu, uint32_t pdu_len);

/*
 * remote_proxied_whois - 远地的whois查询, 后台发送I-Am应答
 */
extern void remote_proxied_whois(uint16_t src_net, uint16_t in_net,
        uint32_t lowlimit, uint32_t highlimit);

#ifdef __cplusplus
}
#endif

#endif /* _SLAVEPROXY_H_ */

