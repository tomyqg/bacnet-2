/*
 * Copyright(C) 2014 SWG. All rights reserved.
 *
 * slaveproxy.c
 * Original Author:  lincheng, 2015-5-28
 *
 * BACnet mstp slave proxy
 *
 * History
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "bacnet/bacdef.h"
#include "slaveproxy_def.h"
#include "bacnet/mstp.h"
#include "bacnet/service/iam.h"
#include "bacnet/network.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacapp.h"
#include "bacnet/addressbind.h"
#include "bacnet/tsm.h"
#include "bacnet/service/rpm.h"
#include "bacnet/service/error.h"
#include "bacnet/bactext.h"
#include "mstp_def.h"

int sp_dbg_verbos = 0;
int sp_dbg_warn = 1;
int sp_dbg_err = 1;

static void timer_handler(el_timer_t *);
static void rp_handler(tsm_invoker_t *invoker, bacnet_buf_t *apdu, BACNET_PDU_TYPE apdu_type);

static slave_proxy_manager_t proxy_manager = {
        .ports = NULL,
        .port_number = 0,
        .lock = PTHREAD_MUTEX_INITIALIZER,
        .que_head = {&proxy_manager.que_head, &proxy_manager.que_head, },
        .rb_head = {NULL, },
        .iam_timer = NULL,
};

/* return true is there are task on queue */
static bool send_one_iam(void)
{
    bacnet_addr_t addr;
    DECLARE_BACNET_BUF(tx_pdu, MIN_APDU);
    mstp_slave_t *slave;
    uint32_t device_id;
    uint16_t max_apdu;
    uint16_t vendor_id;
    BACNET_SEGMENTATION seg_support;
    uint32_t port_id;
    uint8_t mac;
    bool has_next;
    int rv;
    
    pthread_mutex_lock(&proxy_manager.lock);

    slave = NULL;
    has_next = false;
    if (list_empty(&proxy_manager.que_head)) {
        pthread_mutex_unlock(&proxy_manager.lock);
        return false;
    }

    slave = list_last_entry(&proxy_manager.que_head, mstp_slave_t, que_node);
    mac = slave->mac;
    addr.net = slave->queue[slave->queue_tail];
    node_scan_t *scan = &proxy_manager.ports[slave->port]->nodes[slave->mac];
    device_id = scan->device_id;
    max_apdu = scan->max_apdu;
    seg_support = scan->seg_support;
    vendor_id = scan->vendor_id;
    port_id = proxy_manager.ports[slave->port]->mstp->dl.port_id;

    __list_del_entry(&(slave->que_node));

    slave->queue_tail = (slave->queue_tail + 1) & IAM_QUEUE_MASK;
    if (slave->queue_head != slave->queue_tail) {
        list_add(&(slave->que_node), &proxy_manager.que_head);
        has_next = true;
    } else {
        INIT_LIST_HEAD(&slave->que_node);
        has_next = !list_empty(&proxy_manager.que_head);
    }

    pthread_mutex_unlock(&proxy_manager.lock);

    SP_VERBOS("%s: send iam of mac(%d) device_id(%d) to net(%d)\r\n", __func__,
            mac, device_id, addr.net);

    bacnet_buf_init(&tx_pdu.buf, MIN_APDU);
    Build_I_Am_Service(&tx_pdu.buf, device_id, max_apdu, vendor_id, seg_support);

    addr.len = 0;
    rv = network_mstp_fake(port_id, mac, &addr, &tx_pdu.buf, PRIORITY_NORMAL);
    if (rv < 0) {
        SP_ERROR("%s: send iam failed(%d)\r\n", __func__, rv);
    } else if (rv > 0) { /* 伪装出口，正常自已将收到广播，这里模拟收到iam */
        address_add(device_id, max_apdu, &addr, false);
    }

    return has_next;
}

static void _scan_fail(slave_proxy_port_t *port, uint8_t mac)
{
    node_scan_t *node = &port->nodes[mac];

    node->status = SCAN_NOT_START;

    if (node->slave) {
        SP_WARN("%s: device(%d) on net(%d) mac(%d)\r\n", __func__, node->device_id,
            port->net, mac);

        rb_erase(&node->slave->rb_node, &proxy_manager.rb_head);
        __list_del_entry(&node->slave->que_node);
        free(node->slave);
        node->slave = NULL;
    }

    if (!node->manual && node->next != mac) {
        /* be slave before and not manual, remove from link */
        uint8_t next = port->nodes[mac].next;
        uint8_t prev = port->nodes[mac].prev;

        port->nodes[prev].next = next;
        port->nodes[next].prev = prev;
        port->nodes[mac].next = mac;
        port->nodes[mac].prev = mac;
    }
}

static int send_rp_scan(uint16_t port_idx, uint8_t mac, BACNET_PROPERTY_ID property)
{
    DECLARE_BACNET_BUF(tx_pdu, MIN_APDU);
    bacnet_addr_t dst;
    tsm_invoker_t *invoker;
    uint32_t device_id;
    int rv;

    slave_proxy_port_t *port = proxy_manager.ports[port_idx];
    node_scan_t *node = &port->nodes[mac];

    node->timeout = 0;
    device_id = node->manual ? node->device_id : BACNET_MAX_INSTANCE;

    dst.net = port->net;
    dst.len = 1;
    dst.adr[0] = mac;

    slave_scan_context_t context = { {port_idx, mac} };
    invoker = tsm_alloc_invokeID(&dst, SERVICE_CONFIRMED_READ_PROPERTY,
            rp_handler, (void*)context._u);
    if (invoker == NULL) {
        SP_ERROR("%s: alloc invokeID failed\r\n", __func__);
        _scan_fail(port, mac);
        return -EPERM;
    }

    (void)bacnet_buf_init(&tx_pdu.buf, MIN_APDU);
    rv = rp_encode_apdu(&tx_pdu.buf, invoker->invokeID, OBJECT_DEVICE, device_id,
        property, BACNET_ARRAY_ALL);
    if ((rv < 0) || (rv > MIN_APDU)) {
        tsm_free_invokeID(invoker);
        _scan_fail(port, mac);
        SP_ERROR("%s: encode apdu failed(%d)\r\n", __func__, rv);
        return -EPERM;
    }

    rv = tsm_send_apdu(invoker, &tx_pdu.buf, PRIORITY_NORMAL, 0);
    if (rv < 0) {
        tsm_free_invokeID(invoker);
        _scan_fail(port, mac);
        SP_ERROR("%s: tsm send failed(%d)\r\n", __func__, rv);
        return -EPERM;
    }

    port->request_num++;
    return OK;
}

static void _encode_rpm(bacnet_buf_t *pdu, uint32_t device_id, uint8_t invokeID)
{
    rpm_req_encode_object(pdu, OBJECT_DEVICE, device_id);
    rpm_req_encode_property(pdu, PROP_SEGMENTATION_SUPPORTED, BACNET_ARRAY_ALL);
    rpm_req_encode_property(pdu, PROP_VENDOR_IDENTIFIER, BACNET_ARRAY_ALL);
    rpm_req_encode_property(pdu, PROP_MAX_APDU_LENGTH_ACCEPTED, BACNET_ARRAY_ALL);
    rpm_req_encode_end(pdu, invokeID);
}

static int send_rpm_scan(uint16_t port_idx, uint8_t mac)
{
    bacnet_addr_t dst;
    tsm_invoker_t *invoker;
    DECLARE_BACNET_BUF(tx_pdu, MIN_APDU);
    int rv;

    (void)bacnet_buf_init(&tx_pdu.buf, MIN_APDU);

    slave_proxy_port_t *port = proxy_manager.ports[port_idx];
    node_scan_t *node = &port->nodes[mac];

    slave_scan_context_t context = { {port_idx, mac} };

    dst.net = port->net;
    dst.len = 1;
    dst.adr[0] = mac;

    invoker = tsm_alloc_invokeID(&dst, SERVICE_CONFIRMED_READ_PROP_MULTIPLE,
            rp_handler, (void*)context._u);
    if (invoker == NULL) {
        _scan_fail(port, mac);
        SP_ERROR("%s: alloc invokeID failed\r\n", __func__);
        return -EPERM;
    }

    _encode_rpm(&tx_pdu.buf, node->device_id, invoker->invokeID);

    rv = tsm_send_apdu(invoker, &tx_pdu.buf, PRIORITY_NORMAL, 0);
    if (rv < 0) {
        SP_ERROR("%s: tsm send failed(%d)\r\n", __func__, rv);
        _scan_fail(port, mac);
        tsm_free_invokeID(invoker);
        return -EPERM;
    }

    port->request_num++;
    return rv;
}

static void timer_handler(el_timer_t *timer)
{
    bool has_next;
    int iam_sent = 0;
    unsigned now;

    do {
        has_next = send_one_iam();
        iam_sent++;
    } while (has_next && iam_sent < IAM_EACH_SECOND);

    now = el_current_second();

    pthread_mutex_lock(&proxy_manager.lock);

    for (int i = 0; i < proxy_manager.port_number; ++i) {
        slave_proxy_port_t *port = proxy_manager.ports[i];
        if (port->request_num && --port->request_num)
            continue;

        if (!port->proxy_enable) continue;

        usb_mstp_t *mstp = (usb_mstp_t*)port->mstp;
        uint8_t mac;

try_next_mac:
        mac = port->next_scan_mac;
        if (mac == 255) {
            if ((uint32_t)(now - port->start_timestamp) < port->scan_interval)
                continue; /* not our time */
            if (mstp->auto_busy)
                continue; /* wait not busy */

            port->start_timestamp = now;
            mac = 0;
            port->next_scan_mac = 1;
        } else
            port->next_scan_mac++;

        if (mac == mstp->base.mac)
            goto try_next_mac;

        node_scan_t *node = &port->nodes[mac];
        if (!(port->auto_discovery && node->auto_scan) && !node->manual)
            goto try_next_mac;

        if (node->status != SCAN_NOT_START) {
            /* 正在scan中，设置timeout，等其完成后再试 */
            node->timeout = 1;
            continue;
        }

        node->status = SCAN_WHOIS_SUPPORT;
        send_rp_scan(i, mac, PROP_PROTOCOL_SERVICES_SUPPORTED);
    }

    pthread_mutex_unlock(&proxy_manager.lock);

    el_timer_mod(&el_default_loop, timer, 1000);
}

static bool decode_data(uint8_t service_choice, uint8_t *service_data,
        uint32_t service_data_len, struct slave_data_s *slave_data)
{
    slave_data->service_support.value = 0;
    BACNET_READ_PROPERTY_DATA rp_data;

    if (service_choice == SERVICE_CONFIRMED_READ_PROPERTY) {
        if (rp_ack_decode(service_data, service_data_len, &rp_data) < 0) {
            SP_WARN("%s: rp ack decode fail\r\n", __func__);
            return false;
        }

        if (rp_data.object_type != OBJECT_DEVICE) {
            SP_WARN("%s: rp ack invalid object_typ(%d)\r\n", __func__, rp_data.object_type);
            return false;
        }

        if (rp_data.array_index != BACNET_ARRAY_ALL) {
            SP_WARN("%s: rp ack invalid array_indx(%d)\r\n", __func__, rp_data.array_index);
            return false;
        }

        slave_data->property_id = rp_data.property_id;
        slave_data->device_id = rp_data.object_instance;

        switch(slave_data->property_id) {
        case PROP_PROTOCOL_SERVICES_SUPPORTED:
            if (decode_application_bitstring(rp_data.application_data, &slave_data->service_support)
                    != rp_data.application_data_len) {
                SP_WARN("%s: decode service support failed\r\n", __func__);
                return false;
            }
            break;
        case PROP_SEGMENTATION_SUPPORTED:
            if (decode_application_enumerated(rp_data.application_data, &slave_data->seg_support)
                    != rp_data.application_data_len 
                    || slave_data->seg_support >= MAX_BACNET_SEGMENTATION) {
                SP_WARN("%s: decode segmentation support failed\r\n", __func__);
                return false;
            }
            break;
        case PROP_VENDOR_IDENTIFIER:
            if (decode_application_unsigned(rp_data.application_data, &slave_data->vendor_id)
                    != rp_data.application_data_len || (slave_data->vendor_id > 0x0ffff)) {
                SP_WARN("%s: decode vendor id failed\r\n", __func__);
                return false;
            }
            break;
        case PROP_MAX_APDU_LENGTH_ACCEPTED:
            if (decode_application_unsigned(rp_data.application_data, &slave_data->max_apdu)
                    != rp_data.application_data_len) {
                SP_WARN("%s: decode max apdu length accepted failed\r\n", __func__);
                return false;
            }

            if (slave_data->max_apdu > 0x0ffff) {
                slave_data->max_apdu = 0x0ffff;
            }
            break;
        default:
            SP_WARN("%s: rp ack invalid property(%d)\r\n", __func__, slave_data->property_id);
            return false;
        }

        slave_data->is_rpm = false;
    } else {
        BACNET_RPM_ACK_DECODER decoder;
        rpm_ack_decode_init(&decoder, service_data, service_data_len);
        int rv = rpm_ack_decode_object(&decoder, &rp_data);
        if (rv <= 0) {
            SP_WARN("%s: rpm ack decode object failed(%d)\r\n", __func__, rv);
            return false;
        }

        if (rp_data.object_type != OBJECT_DEVICE) {
            SP_WARN("%s: rpm ack invalid object_typ(%d)\r\n", __func__, rp_data.object_type);
            return false;
        }
        slave_data->device_id = rp_data.object_instance;

        /* decode segmentation support */
        rv = rpm_ack_decode_property(&decoder, &rp_data);
        if (rv <= 0) {
            SP_WARN("%s: rpm ack decode segmentation support property failed(%d)\r\n", __func__, rv);
            return false;
        }

        if (rp_data.property_id != PROP_SEGMENTATION_SUPPORTED || rp_data.array_index != BACNET_ARRAY_ALL) {
            SP_WARN("%s: rpm ack invalid property, should be segmentation support\r\n", __func__);
            return false;
        }

        if (rp_data.application_data == NULL) {
            SP_WARN("%s: rpm segmentation support report error\r\n", __func__);
            return false;
        }

        if (decode_application_enumerated(rp_data.application_data, &slave_data->seg_support)
                != rp_data.application_data_len
                || slave_data->seg_support >= MAX_BACNET_SEGMENTATION) {
            SP_WARN("%s: decode segmentation support failed\r\n", __func__);
            return false;
        }

        /* decode vendor id */
        rv = rpm_ack_decode_property(&decoder, &rp_data);
        if (rv <= 0) {
            SP_WARN("%s: rpm ack decode vendor id property failed(%d)\r\n", __func__, rv);
            return false;
        }

        if (rp_data.property_id != PROP_VENDOR_IDENTIFIER || rp_data.array_index != BACNET_ARRAY_ALL) {
            SP_WARN("%s: rpm ack invalid property, should be vendor id\r\n", __func__);
            return false;
        }

        if (rp_data.application_data == NULL) {
            SP_WARN("%s: rpm decode vendor id report error\r\n", __func__);
            return false;
        }

        if (decode_application_unsigned(rp_data.application_data, &slave_data->vendor_id)
                != rp_data.application_data_len) {
            SP_WARN("%s: rpm ack decode vendor id failed\r\n", __func__);
            return false;
        }
        if (slave_data->vendor_id > 0xffff) {
            SP_WARN("%s: rpm ack invalid vendor id(%d)\r\n", __func__, slave_data->vendor_id);
            return false;
        }

        /* decode max apdu */
        rv = rpm_ack_decode_property(&decoder, &rp_data);
        if (rv <= 0) {
            SP_WARN("%s: rpm ack decode property failed(%d)\r\n", __func__, rv);
            return false;
        }

        if (rp_data.property_id != PROP_MAX_APDU_LENGTH_ACCEPTED || rp_data.array_index != BACNET_ARRAY_ALL) {
            SP_WARN("%s: rpm ack invalid property, should be max apdu\r\n", __func__);
            return false;
        }

        if (rp_data.application_data == NULL) {
            SP_WARN("%s: rpm max apdu report error\r\n", __func__);
            return false;
        }

        if (decode_application_unsigned(rp_data.application_data, &slave_data->max_apdu)
                != rp_data.application_data_len) {
            SP_WARN("%s: rpm ack decode max apdu failed\r\n", __func__);
            return false;
        }
        if (slave_data->max_apdu > 0xffff)
            slave_data->max_apdu = 0xffff;

        rv = rpm_ack_decode_property(&decoder, &rp_data);
        if (rv != 0) {
            SP_WARN("%s: rpm ack have other property(%d)?\r\n", __func__, rv);
            return false;
        }

        if (rpm_ack_decode_object(&decoder, &rp_data) != 0) {
            SP_WARN("%s: rpm ack have other object?\r\n", __func__);
            return false;
        }

        slave_data->is_rpm = true;
    }

    return true;
}

static void on_data(uint16_t port_idx, uint8_t mac, struct slave_data_s *slave_data)
{
    slave_proxy_port_t *port;
    node_scan_t *node;
    bool whois_support, send_rp;

    port = proxy_manager.ports[port_idx];
    node = &port->nodes[mac];

    send_rp = false;
    node = &port->nodes[mac];
    switch(node->status) {
    case SCAN_WHOIS_SUPPORT:              /* send rp of service support */
        if (slave_data->property_id != PROP_PROTOCOL_SERVICES_SUPPORTED) {
            SP_ERROR("%s: net(%d) mac(%d) status(%d) response(%d)\r\n", __func__,
                port->net, mac, node->status, slave_data->property_id);
            _scan_fail(port, mac);
            break;
        }

        if (SERVICE_SUPPORTED_WHO_IS < bitstring_size(&slave_data->service_support)
                && bitstring_get_bit(&slave_data->service_support, SERVICE_SUPPORTED_WHO_IS)) {
            whois_support = true;
        } else {
            whois_support = false;
        }

        if (whois_support) { /* not a slave */
            if (node->slave) {
                SP_WARN("%s: binding device(%d) on net(%d) mac(%d) support whois\r\n", __func__, node->device_id,
                    port->net, mac);

                rb_erase(&node->slave->rb_node, &proxy_manager.rb_head);
                __list_del_entry(&node->slave->que_node);
                free(node->slave);
                node->slave = NULL;
            }
            if (!node->manual && node->next != mac) {
                /* be slave before and not manual, remove from link */
                uint8_t next = port->nodes[mac].next;
                uint8_t prev = port->nodes[mac].prev;

                port->nodes[prev].next = next;
                port->nodes[next].prev = prev;
                port->nodes[mac].next = mac;
                port->nodes[mac].prev = mac;
            }
            goto success;
        }

        if (node->slave && slave_data->device_id != node->device_id) {
            SP_WARN("%s: binding device(%d) on net(%d) mac(%d) report different device_id\r\n", __func__,
                    node->device_id, port->net, mac);
            rb_erase(&node->slave->rb_node, &proxy_manager.rb_head);
            __list_del_entry(&node->slave->que_node);
            free(node->slave);
            node->slave = NULL;
        }

        if (node->manual && slave_data->device_id != node->device_id) {
            SP_ERROR("%s: manual binding device(%d) on net(%d) mac(%d) but report device(%d)\r\n", __func__,
                    node->device_id, port->net, mac, slave_data->device_id);
            goto success;
        }

        node->device_id = slave_data->device_id;
        if (node->next == mac) { /* link tail as active binding */
            node->next = 255;
            node->prev = port->nodes[255].prev;
            port->nodes[node->prev].next = mac;
            port->nodes[255].prev = mac;
        }

        node->status = SCAN_SEG_SUPPORT;
        slave_data->property_id = PROP_SEGMENTATION_SUPPORTED;
        send_rp = true;
        break;
    
    case SCAN_SEG_SUPPORT:              /* send rp of segmentation suppot */
        if (slave_data->property_id != PROP_SEGMENTATION_SUPPORTED && !slave_data->is_rpm) {
            SP_ERROR("%s: net(%d) mac(%d) status(%d) response(%d)\r\n", __func__,
                port->net, mac, node->status, slave_data->property_id);
            _scan_fail(port, mac);
            break;
        }

        if (slave_data->device_id != node->device_id) {
            SP_ERROR("%s: device(%d) on net(%d) mac(%d) get seg support report device(%d)\r\n", __func__,
                    node->device_id, port->net, mac, slave_data->device_id);
            _scan_fail(port, mac);
            break;
        }

        if (node->slave && slave_data->seg_support != node->seg_support) {
            /* segmentation support changed */
            SP_WARN("%s: binding device(%d) on net(%d) mac(%d) report different segment support\r\n", __func__,
                    node->device_id, port->net, mac);
            rb_erase(&node->slave->rb_node, &proxy_manager.rb_head);
            __list_del_entry(&node->slave->que_node);
            free(node->slave);
            node->slave = NULL;
        }

        node->seg_support = slave_data->seg_support;

        if (slave_data->is_rpm) {
            node->vendor_id = slave_data->vendor_id;
            goto new_slave_arrived;
        }

        node->status = SCAN_VENDOR_ID;
        slave_data->property_id = PROP_VENDOR_IDENTIFIER;
        send_rp = true;
        break;
    
    case SCAN_VENDOR_ID:                /* send rp of vendor id */
        if (slave_data->property_id != PROP_VENDOR_IDENTIFIER) {
            SP_ERROR("%s: net(%d) mac(%d) status(%d) response(%d)\r\n", __func__,
                port->net, mac, node->status, slave_data->property_id);
            _scan_fail(port, mac);
            break;
        }

        if (slave_data->device_id != node->device_id) {
            SP_ERROR("%s: device(%d) on net(%d) mac(%d) get vendor id report device(%d)\r\n", __func__,
                    node->device_id, port->net, mac, slave_data->device_id);
            _scan_fail(port, mac);
            break;
        }

        if (node->slave && slave_data->vendor_id != node->vendor_id) {
            /* vendor id changed */
            SP_WARN("%s: binding device(%d) on net(%d) mac(%d) report vendor id\r\n", __func__,
                    node->device_id, port->net, mac);
            rb_erase(&node->slave->rb_node, &proxy_manager.rb_head);
            __list_del_entry(&node->slave->que_node);
            free(node->slave);
            node->slave = NULL;
        }

        node->vendor_id = slave_data->vendor_id;
        node->status = SCAN_MAX_APDU;
        slave_data->property_id = PROP_MAX_APDU_LENGTH_ACCEPTED;
        send_rp = true;
        break;
        
    case SCAN_MAX_APDU:                 /* send rp of max_apdu */
        if (slave_data->property_id != PROP_MAX_APDU_LENGTH_ACCEPTED) {
            SP_ERROR("%s: net(%d) mac(%d) status(%d) response(%d)\r\n", __func__,
                port->net, mac, node->status, slave_data->property_id);
            _scan_fail(port, mac);
            break;
        }

        if (slave_data->device_id != node->device_id) {
            SP_ERROR("%s: device(%d) on net(%d) mac(%d) get max apdu report device(%d)\r\n", __func__,
                    node->device_id, port->net, mac, slave_data->device_id);
            _scan_fail(port, mac);
            break;
        }

        if (node->slave && slave_data->max_apdu != node->max_apdu) {
            /* max apdu changed */
            SP_WARN("%s: binding device(%d) on net(%d) mac(%d) report different max apdu\r\n", __func__,
                    node->device_id, port->net, mac);
        }

new_slave_arrived:
        node->max_apdu = slave_data->max_apdu;
        if (!node->slave) {
            struct rb_node **new = &(proxy_manager.rb_head.rb_node), *parent = NULL;

            /* Figure out where to put new node */
            while (*new) {
                mstp_slave_t *this = container_of(*new, mstp_slave_t, rb_node);
                int result = node->device_id
                        - proxy_manager.ports[this->port]->nodes[this->mac].device_id;

                parent = *new;
                if (result < 0) {
                    new = &((*new)->rb_left);
                } else if (result > 0) {
                    new = &((*new)->rb_right);
                } else {    /* find same device id, copy to it */
                    SP_WARN("%s: duplicate device(%d) on net(%d) mac(%d) and net(%d) mac(%d)\r\n", __func__,
                            node->device_id, proxy_manager.ports[this->port]->net, this->mac,
                            port->net, mac);
                    proxy_manager.ports[this->port]->nodes[this->mac].slave = NULL;
                    node->slave = this;
                    this->mac = mac;
                    this->port = port_idx;
                    goto success;
                }
            }

            if (*new == NULL && (node->slave = malloc(sizeof(mstp_slave_t))) != NULL) {
                SP_VERBOS("%s: new binding device(%d) on net(%d) mac(%d)\r\n", __func__, node->device_id,
                    port->net, mac);
                node->slave->mac = mac;
                node->slave->port = port_idx;
                node->slave->queue_head = 0;
                node->slave->queue_tail = 0;
                INIT_LIST_HEAD(&node->slave->que_node);

                /* Add new node and rebalance tree. */
                rb_link_node(&node->slave->rb_node, parent, new);
                rb_insert_color(&node->slave->rb_node, &proxy_manager.rb_head);
            }
        }

success:
        node->status = SCAN_NOT_START;
        break;
    case SCAN_NOT_START:
        SP_ERROR("%s: SCAN_NOT_START should go here\r\n", __func__);
        _scan_fail(port, mac);
        break;
    }

    if (!send_rp) {
        return;
    }

    if (slave_data->property_id != PROP_SEGMENTATION_SUPPORTED
            || bitstring_size(&slave_data->service_support) <= SERVICE_SUPPORTED_READ_PROP_MULTIPLE
            || !bitstring_get_bit(&slave_data->service_support, SERVICE_SUPPORTED_READ_PROP_MULTIPLE)) {
        /* send rp */
        send_rp_scan(port_idx, mac, slave_data->property_id);
    } else {
        send_rpm_scan(port_idx, mac);
    }
}

static void _scan_timeout(tsm_invoker_t *invoker)
{
    DECLARE_BACNET_BUF(tx_pdu, MIN_APDU);
    uint32_t device_id;

    slave_scan_context_t context;
    context._u = (unsigned)invoker->data;

    slave_proxy_port_t *port = proxy_manager.ports[context.port_idx];
    node_scan_t *node = &port->nodes[context.mac];

    device_id = node->manual ? node->device_id : BACNET_MAX_INSTANCE;

    /* 重试 */
    if (invoker->sent_count <= tsm_get_apdu_retries()) {
        (void)bacnet_buf_init(&tx_pdu.buf, MIN_APDU);

        switch (node->status) {
        case SCAN_WHOIS_SUPPORT:
            rp_encode_apdu(&tx_pdu.buf, invoker->invokeID, OBJECT_DEVICE, device_id,
                    PROP_PROTOCOL_SERVICES_SUPPORTED, BACNET_ARRAY_ALL);
            if (tsm_send_apdu(invoker, &tx_pdu.buf, PRIORITY_NORMAL, 0) >= 0)
                goto re_send_ok;
            break;
        case SCAN_SEG_SUPPORT:
            if (invoker->choice == SERVICE_CONFIRMED_READ_PROPERTY) {
                rp_encode_apdu(&tx_pdu.buf, invoker->invokeID, OBJECT_DEVICE, device_id,
                        PROP_SEGMENTATION_SUPPORTED, BACNET_ARRAY_ALL);
            } else {
                _encode_rpm(&tx_pdu.buf, node->device_id, invoker->invokeID);
            }

            if (tsm_send_apdu(invoker, &tx_pdu.buf, PRIORITY_NORMAL, 0) >= 0)
                goto re_send_ok;
            break;
        case SCAN_VENDOR_ID:
            rp_encode_apdu(&tx_pdu.buf, invoker->invokeID, OBJECT_DEVICE, device_id,
                    PROP_VENDOR_IDENTIFIER, BACNET_ARRAY_ALL);
            if (tsm_send_apdu(invoker, &tx_pdu.buf, PRIORITY_NORMAL, 0) >= 0)
                goto re_send_ok;
            break;
        case SCAN_MAX_APDU:
            rp_encode_apdu(&tx_pdu.buf, invoker->invokeID, OBJECT_DEVICE, device_id,
                    PROP_MAX_APDU_LENGTH_ACCEPTED, BACNET_ARRAY_ALL);
            if (tsm_send_apdu(invoker, &tx_pdu.buf, PRIORITY_NORMAL, 0) >= 0)
                goto re_send_ok;
            break;
        default:
            SP_ERROR("%s: impossible state(%d)\r\n", __func__, node->status);
            break;
        }
    }

    tsm_free_invokeID(invoker);
    _scan_fail(port, context.mac);

    if (node->timeout) {
        node->timeout = 0;
        node->status = SCAN_WHOIS_SUPPORT;
        send_rp_scan(context.port_idx, context.mac, PROP_PROTOCOL_SERVICES_SUPPORTED);
    }
    return;

re_send_ok:
    port->request_num++;
}

static void rp_handler(tsm_invoker_t *invoker, bacnet_buf_t *apdu, BACNET_PDU_TYPE apdu_type)
{
    BACNET_CONFIRMED_SERVICE_ACK_DATA ack_data;
    struct slave_data_s slave_data;
        
    if (invoker == NULL) {
        SP_ERROR("%s: null invoker\r\n", __func__);
        return;
    }

    slave_scan_context_t context;
    context._u = (unsigned)invoker->data;

    pthread_mutex_lock(&proxy_manager.lock);
    if (proxy_manager.port_number <= context.port_idx) {
        /* slave proxy stopped */
        tsm_free_invokeID(invoker);
        pthread_mutex_unlock(&proxy_manager.lock);
        return;
    }

    slave_proxy_port_t *port = proxy_manager.ports[context.port_idx];
    node_scan_t *node = &port->nodes[context.mac];

    if (!port->proxy_enable) {
        node->cancel = 0;
        node->status = SCAN_NOT_START;
        tsm_free_invokeID(invoker);
        pthread_mutex_unlock(&proxy_manager.lock);
        return;
    }

     if (node->cancel) {
        node->cancel = 0;
        node->status = SCAN_NOT_START;
        tsm_free_invokeID(invoker);

        if (node->timeout
                && (node->manual || (port->auto_discovery && node->auto_scan))) {
            node->timeout = 0;
            node->status = SCAN_WHOIS_SUPPORT;
            send_rp_scan(context.port_idx, context.mac, PROP_PROTOCOL_SERVICES_SUPPORTED);
        }

        pthread_mutex_unlock(&proxy_manager.lock);
        return;
    }

    if (apdu == NULL) {
        _scan_timeout(invoker);
        pthread_mutex_unlock(&proxy_manager.lock);
        return;
    }
    
    tsm_free_invokeID(invoker);

    switch (apdu_type) {
    case PDU_TYPE_ERROR:
        bacerror_handler(apdu, &invoker->addr);
        goto fail;
    
    case PDU_TYPE_COMPLEX_ACK:
        if (apdu_decode_complex_ack(apdu, &ack_data) < 0) {
            SP_WARN("%s: apdu type(%d) decode fail from net(%d) mac(%d)\r\n", __func__,
                    apdu_type, port->net, context.mac);
            goto fail;
        }
        
        if (ack_data.segmented_message) {
            SP_WARN("%s: segmentation ack from net(%d) mac(%d)\r\n", __func__,
                    port->net, context.mac);
            goto fail;
        }
        
        if ((ack_data.service_choice != SERVICE_CONFIRMED_READ_PROPERTY)
                && (ack_data.service_choice != SERVICE_CONFIRMED_READ_PROP_MULTIPLE)) {
            SP_WARN("%s: complex ack choice %d from net(%d) mac(%d)\r\n", __func__,
                    ack_data.service_choice, port->net, context.mac);
            goto fail;
        }

        if (!decode_data(ack_data.service_choice, ack_data.service_data, ack_data.service_data_len,
                &slave_data)) {
            SP_WARN("%s: decode data failed from net(%d) mac(%d)\r\n", __func__,
                    port->net, context.mac);
            goto fail;
        }

        SP_VERBOS("%s: device(%d) ack is_rpm(%d) from net(%d) mac(%d)\r\n", __func__, slave_data.device_id,
            slave_data.is_rpm, port->net, context.mac);
        on_data(context.port_idx, context.mac, &slave_data);
        goto out;
    
    default:
        SP_WARN("%s: apdu type %d from net(%d) mac(%d)\r\n", __func__,
                apdu_type, port->net, context.mac);
        break;
    }

fail:
    _scan_fail(port, context.mac);

out:
    if (node->timeout) {
        node->timeout = 0;
        node->status = SCAN_NOT_START;
        send_rp_scan(context.port_idx, context.mac, PROP_PROTOCOL_SERVICES_SUPPORTED);
    }

    pthread_mutex_unlock(&proxy_manager.lock);
    return;
}

static void _init_node(node_scan_t *node, uint8_t mac)
{
    node->next = mac;
    node->prev = mac;
}

slave_proxy_port_t *slave_proxy_port_create(datalink_mstp_t *mstp, cJSON *cfg)
{
    slave_proxy_port_t *port;
    cJSON *pcfg, *tmp, *item;
    int i;

    if (mstp == NULL || cfg == NULL) {
        SP_ERROR("%s: null argument\r\n", __func__);
        return NULL;
    }

    port = (slave_proxy_port_t *)malloc(sizeof(slave_proxy_port_t));
    if (port == NULL) {
        SP_ERROR("%s: malloc failed\r\n", __func__);
        return NULL;
    }
    memset(port, 0, sizeof(*port));

    port->mstp = mstp;
    port->scan_interval = DEFAULT_SCAN_INTERVAL;

    for (i = 0; i < sizeof(port->nodes)/sizeof(port->nodes[0]); ++i) {
        _init_node(&port->nodes[i], i);
    }

    pcfg = cJSON_GetObjectItem(cfg, "proxy");
    if (pcfg == NULL) {
        return port;
    }

    if (pcfg->type != cJSON_Object) {
        SP_ERROR("%s: item proxy should be object\r\n", __func__);
        goto out0;
    }
    
    tmp = cJSON_GetObjectItem(pcfg, "enable");
    if (tmp) {
        if ((tmp->type != cJSON_True) && (tmp->type != cJSON_False)) {
            SP_ERROR("%s: enable should be boolean\r\n", __func__);
            goto out0;
        }
        port->proxy_enable = tmp->type == cJSON_True;
    }
    
    tmp = cJSON_GetObjectItem(pcfg, "auto_discovery");
    if (tmp) {
        if ((tmp->type != cJSON_True) && (tmp->type != cJSON_False)) {
            SP_ERROR("%s: auto_discovery should be boolean\r\n", __func__);
            goto out0;
        }
        port->auto_discovery = tmp->type == cJSON_True;
    }

    tmp = cJSON_GetObjectItem(pcfg, "discovery_nodes");
    if (tmp == NULL) {
        for (i = 0; i < 255; ++i) {
            port->nodes[i].auto_scan = 1;
        }
    } else {
        if (tmp->type != cJSON_Array) {
            SP_ERROR("%s: discovery_nodes should be array\r\n", __func__);
            goto out0;
        }
        
        cJSON_ArrayForEach(item, tmp) {
            if (item->type != cJSON_Number) {
                SP_ERROR("%s: item in discovery_nodes should be number\r\n", __func__);
                goto out0;
            }

            if (item->valueint < 0 || item->valueint >= 255) {
                SP_ERROR("%s: item(%d) in discovery_nodes should be 0~254\r\n", __func__,
                    item->valueint);
                goto out0;
            }

            if (port->nodes[item->valueint].auto_scan) {
                SP_ERROR("%s: item(%d) in discovery_nodes duplicated\r\n", __func__,
                    item->valueint);
                goto out0;
            }
            port->nodes[item->valueint].auto_scan = 1;
        }
    }

    tmp = cJSON_GetObjectItem(pcfg, "manual_binding");
    if (tmp) {
        if (tmp->type != cJSON_Array) {
            SP_ERROR("%s: manual binding should be array\r\n", __func__);
            goto out0;
        }

        i = 0;
        cJSON_ArrayForEach(item, tmp) {
            if (item->type != cJSON_String) {
                SP_ERROR("%s: manual binding item(%d) should be string format of mac:device_id\r\n",
                    __func__, i);
                goto out0;
            }
            
            unsigned mac, device_id;
            int pos;
            if (sscanf(item->valuestring, "%u:%u%n", &mac, &device_id, &pos) != 2
                    || pos != strlen(item->valuestring)) {
                SP_ERROR("%s: manual binding item(%s) format should be mac:device_id\r\n", __func__,
                    item->valuestring);
                goto out0;
            }

            if (mac >= 255) {
                SP_ERROR("%s: manual binding(%d) invalid mac(%d)\r\n", __func__, i, mac);
                goto out0;
            }

            if (device_id >= BACNET_MAX_INSTANCE) {
                SP_ERROR("%s: manual binding(%d) invalid instance(%d)\r\n", __func__, i, device_id);
                goto out0;
            }

            node_scan_t *node = &port->nodes[mac];
            if (node->next != mac) {
                SP_ERROR("%s: manual binding duplicate mac(%d)\r\n", __func__, mac);
                goto out0;
            }
            
            node->device_id = device_id;
            node->manual = 1;
            node->prev = 255;
            node->next = port->nodes[255].next;
            port->nodes[255].next = mac;
            port->nodes[node->next].prev = mac;
            i++;
        }
    }

    tmp = cJSON_GetObjectItem(pcfg, "scan_interval");
    if (tmp) {
        if (tmp->type != cJSON_Number) {
            SP_ERROR("%s: scan_interval should be integer\r\n", __func__);
            goto out0;
        }
        
        if (tmp->valueint < MIN_SCAN_INTERVAL) {
            port->scan_interval = MIN_SCAN_INTERVAL;
        } else if (tmp->valueint > 65535) {
            port->scan_interval = 65535;
        } else {
            port->scan_interval = tmp->valueint;
        }
    }
    
    cJSON_DeleteItemFromObject(cfg, "proxy");
    return port;

out0:
    free(port);
    
    return NULL;
}

void slave_proxy_port_delete(datalink_mstp_t *mstp_port)
{
    if ((mstp_port == NULL) || (mstp_port->proxy == NULL)) {
        return;
    }

    free(mstp_port->proxy);
}

static void _proxy_port_disable(slave_proxy_port_t *port)
{
    for(unsigned mac = port->nodes[255].next; mac != 255; mac = port->nodes[mac].next) {
        mstp_slave_t *slave = port->nodes[mac].slave;
        if (slave) {
            rb_erase(&slave->rb_node, &proxy_manager.rb_head);
            __list_del_entry(&slave->que_node);
            free(slave);
            port->nodes[mac].slave = NULL;
        }
        port->nodes[mac].timeout = 0;
        if (port->nodes[mac].status != SCAN_NOT_START)
            port->nodes[mac].cancel = 1;
    }
}

int slave_proxy_startup(void)
{
    datalink_mstp_t *mstp;
    
    pthread_mutex_lock(&proxy_manager.lock);

    if (proxy_manager.port_number) {
        SP_ERROR("%s: already initialized\r\n", __func__);
        goto out0;
    }

    mstp = NULL;
    for (;;) {
        mstp = mstp_next_port(mstp);
        if (mstp == NULL) break;

        proxy_manager.port_number++;
    }

    if (proxy_manager.port_number == 0) {
        SP_WARN("%s: no mstp port present, slave proxy not working\r\n", __func__);
        pthread_mutex_unlock(&proxy_manager.lock);
        return OK;
    }

    proxy_manager.ports = (slave_proxy_port_t**)malloc(sizeof(slave_proxy_port_t*) * proxy_manager.port_number);
    if (proxy_manager.ports == NULL) {
        SP_ERROR("%s: malloc fail\r\n", __func__);
        proxy_manager.port_number = 0;
        goto out0;
    }

    mstp = NULL;
    for(int i=0; i<proxy_manager.port_number; ++i) {
        mstp = mstp_next_port(mstp);
        proxy_manager.ports[i] = mstp->proxy;

        int net = network_number_get(mstp->dl.port_id);
        if (net < 0) {
            SP_ERROR("%s: no net number find for mstp port_id(%d)\r\n", __func__, 
                mstp->dl.port_id);
            goto out1;
        }
        mstp->proxy->net = net;
        mstp->proxy->next_scan_mac = 255;
        mstp->proxy->start_timestamp = el_current_second() - mstp->proxy->scan_interval;
    }

    el_sync(&el_default_loop);

    proxy_manager.iam_timer = el_timer_create(&el_default_loop, 1000);
    if (proxy_manager.iam_timer == NULL) {
        el_unsync(&el_default_loop);
        SP_ERROR("%s: create background timer fail\r\n", __func__);
        goto out1;
    }
    proxy_manager.iam_timer->handler = timer_handler;

    el_unsync(&el_default_loop);

    pthread_mutex_unlock(&proxy_manager.lock);
    return OK;

out1:
    free(proxy_manager.ports);
    proxy_manager.ports = NULL;
    proxy_manager.port_number = 0;

out0:
    pthread_mutex_unlock(&proxy_manager.lock);
    return -EPERM;
}

/*
 * slave_proxy_exit - 反初始化slave proxy功能，必须在mstp链路层反初始化之前执行
 */
void slave_proxy_stop(void)
{
    mstp_slave_t *slave, *tmp;
    pthread_mutex_lock(&proxy_manager.lock);

    if (proxy_manager.port_number == 0) {
        SP_WARN("%s: slave proxy not working, needn't stop\r\n", __func__);
        pthread_mutex_unlock(&proxy_manager.lock);
        return;
    }

    el_timer_destroy(&el_default_loop, proxy_manager.iam_timer);
    proxy_manager.iam_timer = NULL;

    free(proxy_manager.ports);
    proxy_manager.ports = NULL;
    proxy_manager.port_number = 0;

    rbtree_postorder_for_each_entry_safe(slave, tmp, &proxy_manager.rb_head, rb_node) {
        proxy_manager.ports[slave->port]->nodes[slave->mac].slave = NULL;
        free(slave);
    }

    proxy_manager.rb_head = RB_ROOT;
    INIT_LIST_HEAD(&proxy_manager.que_head);

    pthread_mutex_unlock(&proxy_manager.lock);
}

int slave_proxy_port_number()
{
    return proxy_manager.port_number;
}

int slave_proxy_enable(uint32_t mstp_idx, bool enable, bool auto_discovery)
{
    pthread_mutex_lock(&proxy_manager.lock);

    if (mstp_idx >= proxy_manager.port_number) {
        SP_ERROR("%s: invalid mstp id(%d)\r\n", __func__, mstp_idx);
        pthread_mutex_unlock(&proxy_manager.lock);
        return -EPERM;
    }

    slave_proxy_port_t *port = proxy_manager.ports[mstp_idx];
    if (port->proxy_enable && !enable) {
        _proxy_port_disable(port);
    }
    port->proxy_enable = enable;
    port->auto_discovery = auto_discovery;

    pthread_mutex_unlock(&proxy_manager.lock);

    return OK;
}

int slave_proxy_get(uint32_t mstp_idx, bool *enable, bool *auto_discovery)
{
    slave_proxy_port_t *port;
    
    pthread_mutex_lock(&proxy_manager.lock);

    if (mstp_idx >= proxy_manager.port_number) {
        SP_ERROR("%s: invalid mstp id(%d)\r\n", __func__, mstp_idx);
        pthread_mutex_unlock(&proxy_manager.lock);
        return -EPERM;
    }

    port = proxy_manager.ports[mstp_idx];
    if (enable) {
        *enable = port->proxy_enable;
    }
    
    if (auto_discovery) {
        *auto_discovery = port->auto_discovery;
    }
    
    pthread_mutex_unlock(&proxy_manager.lock);

    return OK;
}

static int binding_encode(BACNET_READ_PROPERTY_DATA *rp_data, bool is_manual)
{
    uint8_t *pdu;
    uint32_t pdu_len;
    int len;
    slave_proxy_port_t *port;

    if (rp_data == NULL) {
        SP_ERROR("%s: invalid pdu\r\n", __func__);
        return BACNET_STATUS_ABORT;
    }
    
    pdu = rp_data->application_data;
    pdu_len = rp_data->application_data_len;

    pthread_mutex_lock(&proxy_manager.lock);

    len = 0;
    for (int i = 0; i < proxy_manager.port_number; ++i) {
        port = proxy_manager.ports[i];

        for (uint8_t mac = port->nodes[255].next; mac != 255; mac = port->nodes[mac].next) {
            if (is_manual && !port->nodes[mac].manual) {
                break;
            }
            
            len += encode_application_object_id(&pdu[len], OBJECT_DEVICE,
                    port->nodes[mac].device_id);
            len += encode_application_unsigned(&pdu[len], port->net);
            len += encode_application_raw_octet_string(&pdu[len], &mac, 1);
            if (len > pdu_len) {
                SP_WARN("%s: not space for encode\r\n", __func__);
                pthread_mutex_unlock(&proxy_manager.lock);
                rp_data->abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
                return BACNET_STATUS_ABORT;
            }
        }
    }

    pthread_mutex_unlock(&proxy_manager.lock);

    return len;
}

int read_slave_binding(BACNET_READ_PROPERTY_DATA *rp_data)
{
    return binding_encode(rp_data, 0);
}

/* return < 0 when error */
static int _decode_address_binding(uint8_t *pdu, uint32_t pdu_len, uint32_t *device_id, 
            uint16_t *net, uint8_t *mac)
{
    BACNET_OBJECT_TYPE object_type;
    BACNET_OCTET_STRING oct_string;
    uint32_t long_net;
    int dec_len;
    int len;

    len = 0;
    dec_len = decode_application_object_id(&pdu[len], &object_type, device_id);
    if (dec_len < 0 || len + dec_len >= pdu_len) {
        SP_ERROR("%s: decode object id failed\r\n", __func__);
        return BACNET_STATUS_REJECT;
    }
    len += dec_len;

    dec_len = decode_application_unsigned(&pdu[len], &long_net);
    if (dec_len < 0 || len + dec_len >= pdu_len) {
        SP_ERROR("%s: decode network failed\r\n", __func__);
        return BACNET_STATUS_REJECT;
    }
    len += dec_len;

    dec_len = decode_application_octet_string(&pdu[len], &oct_string);
    if (dec_len < 0 || len + dec_len > pdu_len) {
        SP_ERROR("%s: decode mac failed\r\n", __func__);
        return BACNET_STATUS_REJECT;
    }
    len += dec_len;

    if (object_type != OBJECT_DEVICE) {
        SP_ERROR("%s: object type not device(%d)\r\n", __func__, object_type);
        return BACNET_STATUS_ERROR;
    } else if (*device_id >= BACNET_MAX_INSTANCE) {
        SP_ERROR("%s: invalid object instance(%d)\r\n", __func__, *device_id);
        return BACNET_STATUS_ERROR;
    } else if (long_net >= BACNET_BROADCAST_NETWORK) {
        SP_ERROR("%s: invalid net number(%d)\r\n", __func__, long_net);
        return BACNET_STATUS_ERROR;
    } else if (oct_string.length != 1) {
        SP_ERROR("%s: wrong mstp mac length(%d)\r\n", __func__, oct_string.length);
        return BACNET_STATUS_ERROR;
    } else {
        if (oct_string.value[0] == 255) {
            SP_ERROR("%s: invalid mac address of 255\r\n", __func__);
        }
        
        return BACNET_STATUS_ERROR;
    }

    *net = long_net;
    *mac = oct_string.value[0];
    
    return len;
}

int slave_binding_set(uint8_t *pdu, uint32_t pdu_len)
{
    slave_proxy_port_t *port;
    int port_idx;
    int len;
    int i;
    
    if (pdu == NULL) {
        SP_ERROR("%s: invalid pdu\r\n", __func__);
        return BACNET_STATUS_ABORT;
    }

    pthread_mutex_lock(&proxy_manager.lock);

    uint8_t push_mac[proxy_manager.port_number][255];
    int push_len[proxy_manager.port_number];

    for (i = 0; i < proxy_manager.port_number; ++i) {
        push_len[i] = 0;
    }
    
    len = 0;
    while(len < pdu_len) {
        uint32_t device_id = 0;
        uint8_t mac = 0;
        uint16_t net = 0;
        int dec_len = _decode_address_binding(&pdu[len], pdu_len - len, &device_id, &net, &mac);
        if (dec_len < 0) {
            SP_ERROR("%s: decode address binding failed\r\n", __func__);
            pthread_mutex_unlock(&proxy_manager.lock);
            return BACNET_STATUS_ERROR;
        }

        port = NULL;
        for (i = 0; i < proxy_manager.port_number; ++i) {
            if (proxy_manager.ports[i]->net == net) {
                port = proxy_manager.ports[i];
                port_idx = i;
                break;
            }
        }

        if (port == NULL) continue;

        if (port->nodes[mac].manual) continue;

        if (port->nodes[mac].next != mac) {
            /* if bind already, unlink it */
            uint8_t next = port->nodes[mac].next;
            uint8_t prev = port->nodes[mac].prev;

            port->nodes[prev].next = next;
            port->nodes[next].prev = prev;
            port->nodes[mac].next = mac;
            port->nodes[mac].prev = mac;
        }

        /* pretend it manual to avoid write again */
        port->nodes[mac].manual = 1;
        /* save it to push_mac */
        push_mac[port_idx][push_len[port_idx]++] = mac;

        if (port->nodes[mac].device_id != device_id) {
            mstp_slave_t *slave = port->nodes[mac].slave;
            if (slave != NULL) {
                rb_erase(&slave->rb_node, &proxy_manager.rb_head);
                __list_del_entry(&slave->que_node);
                free(slave);
                port->nodes[mac].slave = NULL;
            }
            port->nodes[mac].device_id = device_id;
            port->nodes[mac].timeout = 0;
            if (port->nodes[mac].status != SCAN_NOT_START)
                port->nodes[mac].cancel = 1;
        }
    }

    for (port_idx = 0; port_idx < proxy_manager.port_number; ++port_idx) {
        port = proxy_manager.ports[port_idx];

        /* remove all dynamic bind linked still */
        uint8_t mac = port->nodes[255].prev;
        for (;;) {
            if (mac == 255 || port->nodes[mac].manual)
                break;

            uint8_t prev = port->nodes[mac].prev;
            port->nodes[mac].next = mac;
            port->nodes[mac].prev = mac;

            mstp_slave_t *slave = port->nodes[mac].slave;
            if (slave != NULL) {
                rb_erase(&slave->rb_node, &proxy_manager.rb_head);
                __list_del_entry(&slave->que_node);
                free(slave);
                port->nodes[mac].slave = NULL;
            }

            mac = prev;
        }
        port->nodes[255].prev = mac;
        port->nodes[mac].next = 255;

        for (i = 0; i < push_len[port_idx]; ++i) {
            mac = push_mac[port_idx][i];
            port->nodes[mac].next = 255;
            port->nodes[mac].prev = port->nodes[255].prev;
            port->nodes[port->nodes[255].prev].next = mac;
            port->nodes[255].prev = mac;
            /* restore manual */
            port->nodes[mac].manual = 0;
        }
    }

    pthread_mutex_unlock(&proxy_manager.lock);
    
    return len;
}

int read_manual_binding(BACNET_READ_PROPERTY_DATA *rp_data)
{
    return binding_encode(rp_data, 1);
}

int manual_binding_set(uint8_t *pdu, uint32_t pdu_len)
{
    slave_proxy_port_t *port;
    int len;
    int i;
    
    if (pdu == NULL) {
        SP_ERROR("%s: invalid pdu\r\n", __func__);
        return BACNET_STATUS_ABORT;
    }

    pthread_mutex_lock(&proxy_manager.lock);

    for (i = 0; i < proxy_manager.port_number; ++i) {
        /* set all previous manual binding to dynamic */
        port = proxy_manager.ports[i];
        for (uint8_t mac = port->nodes[255].next; mac != 255 && port->nodes[mac].manual;
                mac = port->nodes[mac].next) {
            port->nodes[mac].manual = 0;
        }
    }

    len = 0;
    while(len < pdu_len) {
        uint32_t device_id = 0;
        uint8_t mac = 0;
        uint16_t net = 0;
        int dec_len = _decode_address_binding(&pdu[len], pdu_len - len, &device_id, &net, &mac);
        if (dec_len < 0) {
            SP_ERROR("%s: decode address binding failed\r\n", __func__);
            goto err;
        }

        port = NULL;
        for (i = 0; i < proxy_manager.port_number; ++i) {
            if (proxy_manager.ports[i]->net == net) {
                port = proxy_manager.ports[i];
                break;
            }
        }

        if (port == NULL) continue;

        if (port->nodes[mac].next != mac) { /* already binding */
            if (port->nodes[mac].manual) /* duplicated */
                continue;

            uint8_t next = port->nodes[mac].next;
            uint8_t prev = port->nodes[mac].prev;

            port->nodes[prev].next = next;
            port->nodes[next].prev = prev;
        }

        port->nodes[mac].next = port->nodes[255].next;
        port->nodes[mac].prev = 255;
        port->nodes[port->nodes[255].next].prev = mac;
        port->nodes[255].next = mac;

        port->nodes[mac].manual = 1;

        if (port->nodes[mac].device_id != device_id) {
            mstp_slave_t *slave = port->nodes[mac].slave;
            if (slave != NULL) {
                rb_erase(&slave->rb_node, &proxy_manager.rb_head);
                __list_del_entry(&slave->que_node);
                free(slave);
                port->nodes[mac].slave = NULL;
            }
            port->nodes[mac].device_id = device_id;
            if (port->nodes[mac].status != SCAN_NOT_START)
                port->nodes[mac].cancel = 1;
            port->nodes[mac].timeout = 0;
        }
    }

    pthread_mutex_unlock(&proxy_manager.lock);
    
    return len;

err:
    pthread_mutex_unlock(&proxy_manager.lock);
    
    return BACNET_STATUS_ERROR;
}

static mstp_slave_t* _find_device_id_inside(uint32_t lowlimit, uint32_t highlimit)
{
    struct rb_node *node = proxy_manager.rb_head.rb_node;

    while (node) {
        mstp_slave_t *slave = container_of(node, mstp_slave_t, rb_node);

        uint32_t did = proxy_manager.ports[slave->port]->nodes[slave->mac].device_id;

        if (lowlimit > did) {
            node = node->rb_right;
        } else if (highlimit >= did) {
            return slave;
        } else {
            node = node->rb_left;
        }
    }

    return NULL;
}

static void _queue_slave(mstp_slave_t *slave, uint16_t net)
{
    if (slave->queue_tail == ((slave->queue_head + 1) & IAM_QUEUE_MASK))
        slave->queue_tail = (slave->queue_tail + 1) & IAM_QUEUE_MASK;

    slave->queue[slave->queue_head] = net;
    slave->queue_head = (slave->queue_head + 1) & IAM_QUEUE_MASK;

    if (list_empty(&slave->que_node)) {
        list_add(&slave->que_node, &proxy_manager.que_head);
    }
    
    SP_VERBOS("%s: queue a iam task on port(%d), mac(%d) to net(%d)\r\n", __func__, slave->port, 
        slave->mac, net);
}

void remote_proxied_whois(uint16_t src_net, uint16_t in_net,
        uint32_t lowlimit, uint32_t highlimit)
{
    struct rb_node *node, *start_node;
    mstp_slave_t *slave;
    uint32_t device_id;

    if (src_net == BACNET_BROADCAST_NETWORK) {
        SP_ERROR("%s: invalid argument src_net\r\n", __func__);
        return;
    }

    if (in_net == BACNET_BROADCAST_NETWORK) {
        SP_ERROR("%s: invalid argument in_net\r\n", __func__);
        return;
    }

    if (lowlimit > highlimit) {
        SP_WARN("%s: lowlimit > highlimit\r\n", __func__);
        return;
    }

    pthread_mutex_lock(&proxy_manager.lock);

    slave = _find_device_id_inside(lowlimit, highlimit);
    if (slave == NULL) {
        pthread_mutex_unlock(&proxy_manager.lock);
        return;
    }

    start_node = &slave->rb_node;
    node = start_node;
    do {
        /* only response slave in in_net */
        if (proxy_manager.ports[slave->port]->net == in_net)
            _queue_slave(slave, src_net);

        node = rb_prev(node);
        if (!node)
            break;
        slave = container_of(node, mstp_slave_t, rb_node);
        device_id = proxy_manager.ports[slave->port]->nodes[slave->mac].device_id;
    } while (device_id >= lowlimit);

    for (node = rb_next(start_node); node; node = rb_next(node)) {
        slave = container_of(node, mstp_slave_t, rb_node);
        device_id = proxy_manager.ports[slave->port]->nodes[slave->mac].device_id;

        if (device_id > highlimit)
            break;
        /* only response slave in in_net */
        if (proxy_manager.ports[slave->port]->net == in_net)
            _queue_slave(slave, src_net);
   }

    pthread_mutex_unlock(&proxy_manager.lock);
    
    return;
}

cJSON* slave_proxy_get_mib (slave_proxy_port_t *port)
{
    if (port == NULL) {
        SP_ERROR("%s: null argument\r\n", __func__);
        return NULL;
    }

    cJSON *mib = cJSON_CreateObject();

    pthread_mutex_lock(&proxy_manager.lock);

    if (!port->proxy_enable) {
        goto out;
    }

    cJSON *map = cJSON_CreateArray();

    for (uint8_t mac = port->nodes[255].next; mac != 255; mac = port->nodes[mac].next) {
        if (!port->nodes[mac].slave)
            continue;

        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "mac", mac);
        cJSON_AddNumberToObject(item, "segmentation_support", port->nodes[mac].seg_support);
        cJSON_AddNumberToObject(item, "max_apdu", port->nodes[mac].max_apdu);
        cJSON_AddNumberToObject(item, "device_id", port->nodes[mac].device_id);
        cJSON_AddNumberToObject(item, "vendor_id", port->nodes[mac].vendor_id);

        const char *vendor_name = bactext_vendor_name(port->nodes[mac].vendor_id);
        if (vendor_name == NULL)
            vendor_name = "";
        cJSON_AddStringToObject(item, "vendor_name", vendor_name);

        cJSON_AddItemToArray(map, item);
    }

    cJSON_AddItemToObject(mib, "proxing", map);

out:
    pthread_mutex_unlock(&proxy_manager.lock);
    return mib;
}
