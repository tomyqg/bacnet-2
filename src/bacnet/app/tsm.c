/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * tsm.c
 * Original Author:  linzhixian, 2016-7-12
 *
 * BACnet Transaction State Machine
 *
 * History
 */

#include <stdlib.h>
#include <errno.h>

#include "tsm_def.h"
#include "bacnet/app.h"
#include "bacnet/apdu.h"
#include "misc/bits.h"
#include "misc/utils.h"

static tsm_table_t tsm_table;

static uint32_t max_peer = 1024;

static uint32_t max_invoker = 2048;

static uint32_t no_ack_recycle_timeout = 60000;

static uint32_t apdu_timeout = 10000;

static uint32_t apdu_retries = 3;

static bool tsm_init_status = false;

static int __address_hash(const bacnet_addr_t *addr)
{
    const uint8_t *start = (uint8_t *)addr;
    int code = 0;
    const uint8_t *end = &addr->adr[addr->len];
    
    do {
        code = ROTATE_LEFT(code, 8);
        code += *start;
    } while (++start < end);

    return code;
}

static int __invoker_hash(const bacnet_addr_t *addr, uint8_t invokeID)
{
    return ROTATE_LEFT(__address_hash(addr), 8) + invokeID;
}

static tsm_peer_t *__tsm_peer_find(bacnet_addr_t *addr)
{
    tsm_peer_t *info;

    hash_for_each_possible(tsm_table.peer_table, info, node, __address_hash(addr)) {
        if (address_equal(&info->addr, addr)) {
            return info;
        }
    }

    return NULL;
}

static tsm_invoker_impl_t *__tsm_invoker_find(bacnet_addr_t *addr, uint8_t invokeID)
{
    tsm_invoker_impl_t *invoker;

    hash_for_each_possible(tsm_table.invoker_table, invoker, node, __invoker_hash(addr, invokeID)) {
        if ((invoker->base.invokeID == invokeID) && address_equal(&invoker->base.addr, addr)) {
            return invoker;
        }
    }

    return NULL;
}

static int tsm_get_free_invokeID(tsm_peer_t *info)
{
    uint8_t cur_bit;
    int i;

    if (info->free_bits == 0) {
        return -EPERM;
    }

    /* 以下有优化的空间，我们可首先用byte来做判断，非全1后，再找零位 */
    cur_bit = info->next_bit;
    for (i = 0; i < 256; i++, cur_bit++) {
        if (get_bit(info->invokeID_bitmap, cur_bit) == 0) {
            set_bit(info->invokeID_bitmap, cur_bit);
            info->next_bit = cur_bit + 1;
            info->free_bits--;
            return cur_bit;
        }
    }

    info->free_bits = 0;
    
    return -EPERM;
}

static void tsm_set_invokeID_free(tsm_peer_t *info, uint8_t invokeID)
{
    if (get_bit(info->invokeID_bitmap, invokeID)) {
        clear_bit(info->invokeID_bitmap, invokeID);
        info->free_bits++;
    }

    if (info->free_bits == 256) {
        hash_del(&info->node);
        tsm_table.peer_count--;
        free(info);
    }
}

static void __tsm_free_invokeID(tsm_invoker_impl_t *invoker)
{
    tsm_peer_t *peer;
    
    hash_del(&invoker->node);
    tsm_table.invoker_count--;

    peer = invoker->peer_tsm;
    if (peer) {
        tsm_set_invokeID_free(peer, invoker->base.invokeID);
    }
    
    free(invoker);
}

static void tsm_invoker_timer(el_timer_t *timer)
{
    tsm_invoker_impl_t *invoker;

    if ((timer == NULL) || (timer->data == NULL)) {
        return;
    }

    RWLOCK_WRLOCK(&tsm_table.rwlock);

    invoker = (tsm_invoker_impl_t *)timer->data;
    el_timer_destroy(&el_default_loop, invoker->timer);
    invoker->timer = NULL;
    
    __tsm_free_invokeID(invoker);
    
    RWLOCK_UNLOCK(&tsm_table.rwlock);
}

void tsm_free_invokeID(tsm_invoker_t *invoker)
{
    tsm_invoker_impl_t *invoker_impl;
    uint32_t now;
    int rv;

    if (!tsm_init_status) {
        APP_ERROR("%s: TSM is not inited\r\n", __func__);
        return;
    }

    if (invoker == NULL) {
        APP_ERROR("%s: null argument\r\n", __func__);
        return;
    }
    
    now = el_current_millisecond();

    RWLOCK_WRLOCK(&tsm_table.rwlock);

    invoker_impl = (tsm_invoker_impl_t *)invoker;
    if ((invoker_impl->not_acked_count == 0)
            || ((uint32_t)(now - invoker_impl->last_tx_timestamp) >= no_ack_recycle_timeout)) {
        if (invoker_impl->timer != NULL) {
            el_timer_destroy(&el_default_loop, invoker_impl->timer);
            invoker_impl->timer = NULL;
        }
        __tsm_free_invokeID(invoker_impl);
        goto out;
    }

    invoker_impl->canceled = true;
    if (invoker_impl->timer != NULL) {
        rv = el_timer_mod(&el_default_loop, invoker_impl->timer,
            no_ack_recycle_timeout - (uint32_t)(now - invoker_impl->last_tx_timestamp));
        if (rv < 0) {
            APP_ERROR("%s: mod timer failed(%d)\r\n", __func__, rv);
            goto out;
        }
    } else {
        invoker_impl->timer = el_timer_create(&el_default_loop,
            no_ack_recycle_timeout - (uint32_t)(now - invoker_impl->last_tx_timestamp));
        if (invoker_impl->timer == NULL) {
            APP_ERROR("%s: create timer failed\r\n", __func__);
            goto out;
        }
    }
    
    invoker_impl->timer->handler = tsm_invoker_timer;
    invoker_impl->timer->data = (void *)invoker_impl;

out:
    RWLOCK_UNLOCK(&tsm_table.rwlock);

    return;
}

tsm_invoker_t *tsm_alloc_invokeID(bacnet_addr_t *addr, BACNET_CONFIRMED_SERVICE choice,
                invoker_handler handler, void *data)
{
    tsm_peer_t *peer_tsm;
    tsm_invoker_impl_t *invoker;
    int invokeID;

    if (!tsm_init_status) {
        APP_ERROR("%s: TSM is not inited\r\n", __func__);
        return NULL;
    }

    if ((addr == NULL) || (addr->net == BACNET_BROADCAST_NETWORK) || (addr->len == 0)
            || (handler == NULL)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return NULL;
    }

    RWLOCK_WRLOCK(&tsm_table.rwlock);

    if (tsm_table.invoker_count >= max_invoker) {
        APP_ERROR("%s: too many invokers\r\n", __func__);
        RWLOCK_UNLOCK(&tsm_table.rwlock);
        return NULL;
    }

    peer_tsm = __tsm_peer_find(addr);
    if (peer_tsm == NULL) {
        if (tsm_table.peer_count >= max_peer) {
            APP_ERROR("%s: too many tsm peers\r\n", __func__);
            RWLOCK_UNLOCK(&tsm_table.rwlock);
            return NULL;
        }

        peer_tsm = (tsm_peer_t *)malloc(sizeof(tsm_peer_t));
        if (peer_tsm == NULL) {
            APP_ERROR("%s: malloc tsm_peer failed\r\n", __func__);
            RWLOCK_UNLOCK(&tsm_table.rwlock);
            return NULL;
        }

        memset(peer_tsm, 0, sizeof(tsm_peer_t));
        memcpy(&peer_tsm->addr, addr, sizeof(bacnet_addr_t));
        get_random(&peer_tsm->next_bit, sizeof(peer_tsm->next_bit));
        peer_tsm->free_bits = 256;
        hash_add(tsm_table.peer_table, &peer_tsm->node, __address_hash(addr));
        tsm_table.peer_count++;
    }

    invokeID = tsm_get_free_invokeID(peer_tsm);
    if (invokeID < 0) {
        APP_ERROR("%s: get free invokeID failed(%d)\r\n", __func__, invokeID);
        RWLOCK_UNLOCK(&tsm_table.rwlock);
        return NULL;
    }

#ifdef DEBUG
    invoker = __tsm_invoker_find(addr, invokeID);
    if (invoker != NULL) {
        APP_ERROR("%s: invoker(%d) conflict\r\n", __func__, invokeID);
        RWLOCK_UNLOCK(&tsm_table.rwlock);
        return NULL;
    }
#endif

    invoker = (tsm_invoker_impl_t *)malloc(sizeof(tsm_invoker_impl_t));
    if (invoker == NULL) {
        APP_ERROR("%s: malloc invoker info failed\r\n", __func__);
        tsm_set_invokeID_free(peer_tsm, invokeID);
        RWLOCK_UNLOCK(&tsm_table.rwlock);
        return NULL;
    }

    memset(invoker, 0, sizeof(tsm_invoker_impl_t));
    memcpy(&invoker->base.addr, addr, sizeof(bacnet_addr_t));
    invoker->base.invokeID = invokeID;
    invoker->base.choice = choice;
    invoker->base.handler = handler;
    invoker->base.data = data;
    invoker->peer_tsm = peer_tsm;
    hash_add(tsm_table.invoker_table, &invoker->node, __invoker_hash(addr, invokeID));
    tsm_table.invoker_count++;

    RWLOCK_UNLOCK(&tsm_table.rwlock);

    return &(invoker->base);
}

void tsm_invoker_callback(bacnet_addr_t *addr, bacnet_buf_t *apdu, BACNET_PDU_TYPE apdu_type)
{
    BACNET_CONFIRMED_SERVICE choice;
    tsm_invoker_impl_t *invoker;
    uint8_t invokeID;

    if (!tsm_init_status) {
        APP_ERROR("%s: TSM is not inited\r\n", __func__);
        return;
    }

    if (addr == NULL) {
        APP_ERROR("%s: null addr\r\n", __func__);
        return;
    }

    if (apdu == NULL) {
        APP_ERROR("%s: null apdu\r\n", __func__);
        return;
    }

    if (apdu->data_len < 3) {
        APP_WARN("%s: too short apdu(%d)\r\n", __func__, apdu->data_len);
        return;
    }

    choice = MAX_BACNET_CONFIRMED_SERVICE;
    switch (apdu_type) {
    case PDU_TYPE_SIMPLE_ACK:
    case PDU_TYPE_COMPLEX_ACK:
    case PDU_TYPE_ERROR:
        choice = apdu->data[2];
        if (choice >= MAX_BACNET_CONFIRMED_SERVICE) {
            APP_ERROR("%s: invalid service choice(%d)\r\n", __func__, choice);
            return;
        }
        break;
    
    case PDU_TYPE_REJECT:
    case PDU_TYPE_ABORT:
        if (apdu->data_len != 3) {
            APP_ERROR("%s: invalid reject/abort pdu len(%d)\r\n", __func__, apdu->data_len);
            return;
        }
        break;
    
    default:
        APP_ERROR("%s: invalid apdu_type(%d)\r\n", __func__, apdu_type);
        return;
    }

    invokeID = apdu->data[1];

    RWLOCK_WRLOCK(&tsm_table.rwlock);

    invoker = __tsm_invoker_find(addr, invokeID);
    if (invoker) {
        if ((choice < MAX_BACNET_CONFIRMED_SERVICE) && (choice != invoker->base.choice)) {
            APP_ERROR("%s: invalid service choice(%d)\r\n", __func__, choice);
            goto out;
        }
    
        if (invoker->not_acked_count == 0) {
            APP_ERROR("%s: duplicated ack\r\n", __func__);
            goto out;
        }
        invoker->not_acked_count--;
        
        if (invoker->canceled) {
            if (invoker->not_acked_count == 0) {
                el_timer_destroy(&el_default_loop, invoker->timer);
                invoker->timer = NULL;
                __tsm_free_invokeID(invoker);
            }
        } else if (invoker->timer == NULL) {
            /* handler is called, do nothing */
        } else if (invoker->base.handler) {
            el_timer_destroy(&el_default_loop, invoker->timer);
            invoker->timer = NULL;
            RWLOCK_UNLOCK(&tsm_table.rwlock);
            invoker->base.handler(&invoker->base, apdu, apdu_type);
            return;
        }
    }

out:
    RWLOCK_UNLOCK(&tsm_table.rwlock);

    return;
}

static void apdu_timeout_handler(el_timer_t *timer)
{
    tsm_invoker_impl_t *invoker;

    if ((timer == NULL) || (timer->data == NULL)) {
        APP_ERROR("%s: null argument\r\n", __func__);
        return;
    }

    invoker = (tsm_invoker_impl_t *)timer->data;
    el_timer_destroy(&el_default_loop, invoker->timer);
    invoker->timer = NULL;

    if (invoker->base.handler != NULL) {
        invoker->base.handler(&invoker->base, NULL, MAX_PDU_TYPE);
    }
}

int tsm_send_apdu(tsm_invoker_t *invoker, bacnet_buf_t *apdu, bacnet_prio_t prio, uint32_t timeout)
{
    tsm_invoker_impl_t *impl_invoker;
    int rv;

    if (!tsm_init_status) {
        APP_ERROR("%s: TSM is not inited\r\n", __func__);
        return -EPERM;
    }

    if (invoker == NULL) {
        APP_ERROR("%s: null invoker\r\n", __func__);
        return -EINVAL;
    }

    if ((apdu == NULL) || (apdu->data == NULL) || (apdu->data_len == 0)) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }
    
    if (timeout == 0) {
        timeout = apdu_timeout;
    }
    
    impl_invoker = (tsm_invoker_impl_t *)invoker;
    if (impl_invoker->timer != NULL) {
        APP_ERROR("%s: apdu re-send before ack\r\n", __func__);
        return -EPERM;
    }

    rv = apdu_send(&invoker->addr, apdu, prio, true);
    if (rv < 0) {
        APP_ERROR("%s: apdu send failed(%d)\r\n", __func__, rv);
        return rv;
    }

    impl_invoker->not_acked_count++;
    impl_invoker->last_tx_timestamp = el_current_millisecond();
    impl_invoker->base.sent_count++;

    impl_invoker->timer = el_timer_create(&el_default_loop, timeout);
    if (impl_invoker->timer == NULL) {
        APP_ERROR("%s: create timer failed\r\n", __func__);
        return -EPERM;
    }
    impl_invoker->timer->handler = apdu_timeout_handler;
    impl_invoker->timer->data = (void *)impl_invoker;
    
    return OK;
}

uint32_t tsm_get_apdu_timeout(void)
{
    return apdu_timeout;
}

uint32_t tsm_set_apdu_timeout(uint32_t new_timeout)
{
    if (new_timeout < MIN_APDU_TIMEOUT) {
        new_timeout = MIN_APDU_TIMEOUT;
    } else if (new_timeout > no_ack_recycle_timeout) {
        new_timeout = no_ack_recycle_timeout;
    }
    
    apdu_timeout = new_timeout;

    return new_timeout;
}

uint32_t tsm_get_apdu_retries(void)
{
    return apdu_retries;
}

uint32_t tsm_set_apdu_retries(uint32_t new_retries)
{
    if (new_retries > MAX_APDU_RETRIES) {
        new_retries = MAX_APDU_RETRIES;
    }
    
    apdu_retries = new_retries;

    return new_retries;
}

int tsm_init(cJSON *cfg)
{
    cJSON *tmp;
    int rv;

    if (tsm_init_status) {
        APP_WARN("%s: TSM is already inited\r\n", __func__);
        return OK;
    }
    
    if (cfg == NULL) {
        APP_ERROR("%s: null cfg\r\n", __func__);
        return -EINVAL;
    }

    tmp = cJSON_GetObjectItem(cfg, "Max_Peer");
    if (tmp) {
        if (tmp->type != cJSON_Number) {
            APP_ERROR("%s: invalid Max_Peer item type\r\n", __func__);
            return -EPERM;
        }

        if (tmp->valueint < MIN_MAX_PEER) {
            APP_WARN("%s: too small Max_Peer(%d), use %d\r\n", __func__, tmp->valueint,
                MIN_MAX_PEER);
            max_peer = MIN_MAX_PEER;
        } else {
            max_peer = (uint32_t)tmp->valueint;
        }
    }

    tmp = cJSON_GetObjectItem(cfg, "Max_Invoker");
    if (tmp) {
        if (tmp->type != cJSON_Number) {
            APP_ERROR("%s: invalid Max_Invoker item type\r\n", __func__);
            return -EPERM;
        }

        if (tmp->valueint < MIN_MAX_INVOKER) {
            APP_WARN("%s: too small Max_Invoker(%d), use %d\r\n", __func__, tmp->valueint,
                MIN_MAX_INVOKER);
            max_invoker = MIN_MAX_INVOKER;
        } else {
            max_invoker = (uint32_t)tmp->valueint;
        }
    }

    tmp = cJSON_GetObjectItem(cfg, "No_Ack_Recycle_Timeout");
    if (tmp) {
        if (tmp->type != cJSON_Number) {
            APP_ERROR("%s: invalid No_Ack_Recycle_Timeout item type\r\n", __func__);
            return -EPERM;
        }

        if (tmp->valueint < MIN_APDU_TIMEOUT) {
            APP_WARN("%s: too small No_Ack_Recycle_Timeout(%d), use %d\r\n", __func__, tmp->valueint,
                MIN_APDU_TIMEOUT);
            no_ack_recycle_timeout = MIN_APDU_TIMEOUT;
        } else {
            no_ack_recycle_timeout = (uint32_t)tmp->valueint;
        }
    }
    
    tmp = cJSON_GetObjectItem(cfg, "APDU_Timeout");
    if (tmp) {
        if (tmp->type != cJSON_Number) {
            APP_ERROR("%s: invalid APDU_Timeout item type\r\n", __func__);
            return -EPERM;
        }

        if (tsm_set_apdu_timeout((uint32_t)tmp->valueint) != tmp->valueint) {
            APP_WARN("%s: invalid APDU_Timeout(%d), use %d\r\n", __func__, tmp->valueint,
                apdu_timeout);
        }
    }
    
    tmp = cJSON_GetObjectItem(cfg, "APDU_Retries");
    if (tmp) {
        if (tmp->type != cJSON_Number) {
            APP_ERROR("%s: invalid APDU_Retries item type\r\n", __func__);
            return -EPERM;
        }

        if (tsm_set_apdu_retries((uint32_t)tmp->valueint) != tmp->valueint) {
            APP_WARN("%s: invalid APDU_Retries(%d), use %d\r\n", __func__, tmp->valueint,
                apdu_retries);
        }
    }
    
    rv = pthread_rwlock_init(&tsm_table.rwlock, NULL);
    if (rv) {
        APP_ERROR("%s: rwlock init failed(%d)\r\n", __func__, rv);
        return -EPERM;
    }
    
    tsm_table.peer_count = 0;
    hash_init(tsm_table.peer_table);
    
    tsm_table.invoker_count = 0;
    hash_init(tsm_table.invoker_table);

    tsm_init_status = true;
    
    return OK;
}

void tsm_exit(void)
{
    tsm_invoker_impl_t *invoker;
    tsm_peer_t *peer;
    struct hlist_node *tmp;
    int bkt;

    if (!tsm_init_status) {
        return;
    }
    
    RWLOCK_WRLOCK(&tsm_table.rwlock);

    hash_for_each_safe(tsm_table.invoker_table, bkt, invoker, tmp, node) {
        if (invoker->timer) {
            el_timer_destroy(&el_default_loop, invoker->timer);
            invoker->timer = NULL;
        }
        __tsm_free_invokeID(invoker);
    }
    tsm_table.invoker_count = 0;

    hash_for_each_safe(tsm_table.peer_table, bkt, peer, tmp, node) {
        hash_del(&peer->node);
        free(peer);
    }
    tsm_table.peer_count = 0;

    RWLOCK_UNLOCK(&tsm_table.rwlock);

    (void)pthread_rwlock_destroy(&tsm_table.rwlock);

    tsm_init_status = false;
}

