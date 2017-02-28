/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * address.c
 * Original Author:  linzhixian, 2015-2-13
 *
 * BACnet Address Binding
 *
 * History
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "addressbind_def.h"
#include "bacnet/bacenum.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacstr.h"
#include "bacnet/app.h"
#include "bacnet/service/whois.h"
#include "bacnet/slaveproxy.h"
#include "bacnet/object/device.h"
#include "misc/eventloop.h"
#include "misc/utils.h"

static Address_Manager_t Cache_Manager;
static Whois_Manager_t Whois_Manager;
static uint32_t Address_Cache_TTL = 300;
static uint32_t Whois_Max_Retry;
static uint32_t Max_Address_Cache = 512;
static uint32_t Max_WhoIs_Cache = 512;

static int __address_hash(const bacnet_addr_t *addr)
{
    const uint8_t *start = (uint8_t*)addr;
    int code = 0;
    const uint8_t *end = &addr->adr[addr->len];
    
    do {
        code = ROTATE_LEFT(code, 8);
        code += *start;
    } while (++start < end);

    return code;
}

static Whois_Cache_Entry_t* _whois_remove_oldest(void)
{
    Whois_Cache_Entry_t *entry;

    entry = list_last_entry(&(Whois_Manager.list), Whois_Cache_Entry_t, l_node);
    if (entry == NULL) {
        APP_ERROR("%s: get last entry failed\r\n", __func__);
        return NULL;
    }

    hash_del(&(entry->h_node));
    __list_del_entry(&(entry->l_node));
    (Whois_Manager.count)--;

    return entry;
}

static Whois_Cache_Entry_t* _whois_find(uint32_t device_id)
{
    Whois_Cache_Entry_t *entry;

    entry = NULL;
    hash_for_each_possible(Whois_Manager.table, entry, h_node, device_id) {
        if (entry->device_id == device_id) {
            return entry;
        }
    }

    return NULL;
}

static void whois_remove(uint32_t device_id)
{
    Whois_Cache_Entry_t *entry;
    
    pthread_mutex_lock(&(Whois_Manager.lock));

    entry = _whois_find(device_id);
    if (entry) {
        __hlist_del(&(entry->h_node));
        __list_del_entry(&(entry->l_node));
        free(entry);
        (Whois_Manager.count)--;
    }

    pthread_mutex_unlock(&(Whois_Manager.lock));
}

void whois_destroy(void)
{
    Whois_Cache_Entry_t *entry, *tmp;

    pthread_mutex_lock(&(Whois_Manager.lock));

    list_for_each_entry_safe(entry, tmp, &Whois_Manager.list, l_node) {
        __list_del_entry(&(entry->l_node));
        __hlist_del(&(entry->h_node));
        free(entry);
    }

    hash_init(Whois_Manager.table);
    Whois_Manager.count = 0;
    
    pthread_mutex_unlock(&(Whois_Manager.lock));
}

static int send_whois_cached(uint32_t device_id, uint16_t net)
{
    Whois_Cache_Entry_t *entry;
    unsigned cur_time;;
    unsigned past;
    
    cur_time = el_current_second();
    
    pthread_mutex_lock(&(Whois_Manager.lock));

    entry = _whois_find(device_id);
    if (entry == NULL) {
        if (Whois_Manager.count < Max_WhoIs_Cache) {
            entry = (Whois_Cache_Entry_t *)malloc(sizeof(Whois_Cache_Entry_t));
        } else {
            entry = _whois_remove_oldest();
        }

        if (entry == NULL) {
            APP_ERROR("%s: get a free entry failed\r\n", __func__);
            pthread_mutex_unlock(&(Whois_Manager.lock));
            return -EPERM;
        }

        entry->timeout = WHOIS_MIN_INTERVAL;
        entry->device_id = device_id;
        hash_add(Whois_Manager.table, &entry->h_node, device_id);
        (Whois_Manager.count)++;
    } else {
        past = cur_time - entry->last_time;
        net = 0xffff;

        if (past < entry->timeout) {
            pthread_mutex_unlock(&(Whois_Manager.lock));
            return 0;
        }

        entry->timeout = cal_retry_timeout(entry->timeout, past, WHOIS_MIN_INTERVAL,
            Whois_Max_Retry, entry->timeout * 3 / 4);

        __list_del_entry(&(entry->l_node));
    }

    entry->last_time = cur_time;
    list_add(&(entry->l_node), &(Whois_Manager.list));
    
    pthread_mutex_unlock(&(Whois_Manager.lock));

    Send_WhoIs_Remote(net, device_id, device_id);
    
    return 1;
}

/* remove the least recently used entry from active address cache */
static Address_Cache_Entry_t* _address_remove_lru(void)
{
    Address_Cache_Entry_t *entry;
    
    entry = list_last_entry(&(Cache_Manager.active_list), Address_Cache_Entry_t, l_node);
    if (entry == NULL) {
        APP_ERROR("%s: get last entry failed\r\n", __func__);
        return NULL;
    }

    hash_del(&(entry->did_node));
    hash_del(&(entry->adr_node));
    __list_del_entry(&(entry->l_node));
    (Cache_Manager.active_count)--;

    return entry;
}

static Address_Cache_Entry_t *_address_find(uint32_t device_id)
{
    Address_Cache_Entry_t *entry;

    entry = NULL;
    hash_for_each_possible(Cache_Manager.d2a_table, entry, did_node, device_id) {
        if (entry->device_id == device_id) {
            return entry;
        }
    }

    return NULL;
}

static Address_Cache_Entry_t *_device_find(const bacnet_addr_t *addr)
{
    Address_Cache_Entry_t *entry;

    entry = NULL;
    hash_for_each_possible(Cache_Manager.a2d_table, entry, adr_node, __address_hash(addr)) {
        if (address_equal(addr, &entry->address)) {
            return entry;
        }
    }

    return NULL;
}

/* add an entry to the address cache */
int address_add(uint32_t device_id, uint32_t max_apdu, const bacnet_addr_t *addr, 
        bool is_static)
{
    Address_Cache_Entry_t *entry_did, *entry_adr;
    
    if (addr == NULL) {
        APP_ERROR("%s: invalid addr argument\r\n", __func__);
        return -EINVAL;
    }

    if (device_id >= BACNET_MAX_INSTANCE) {
        APP_ERROR("%s: invalid device id(%u)\r\n", __func__, device_id);
        return -EINVAL;
    }

    if (max_apdu > 65535) {
        max_apdu = 65535;
    }
    
    pthread_mutex_lock(&(Cache_Manager.lock));

    entry_did = _address_find(device_id);
    if (entry_did == NULL) {
        entry_adr = _device_find(addr);
        if (entry_adr == NULL) {    /* 双向映射均未找到，是全新的映射 */
            if (Cache_Manager.active_count < Max_Address_Cache) {
                entry_did = (Address_Cache_Entry_t *)malloc(sizeof(Address_Cache_Entry_t));
            } else {
                entry_did = _address_remove_lru();
            }

            if (entry_did == NULL) {
                APP_ERROR("%s: get a free entry failed\r\n", __func__);
                pthread_mutex_unlock(&(Cache_Manager.lock));
                return -EPERM;
            }

            entry_did->device_id = device_id;
            entry_did->address = *addr;
            hash_add(Cache_Manager.d2a_table, &entry_did->did_node, device_id);
            hash_add(Cache_Manager.a2d_table, &entry_did->adr_node, __address_hash(addr));
        } else if (entry_adr->is_static) {
            APP_ERROR("%s: failed cause the device_id(%d) address entry exists in static cache\r\n",
                __func__, device_id);
            pthread_mutex_unlock(&(Cache_Manager.lock));
            return -EPERM;
        } else {
            APP_WARN("%s: device_id(%d) address exists in a2d table\r\n", __func__, device_id);
            /* because did map not found, so device_id not match,
             * so delete did, reuse adr */
            __hlist_del(&(entry_adr->did_node));
            entry_did = entry_adr;
            entry_did->device_id = device_id;
            hash_add(Cache_Manager.d2a_table, &entry_did->did_node, device_id);
            __list_del_entry(&(entry_did->l_node));
            (Cache_Manager.active_count)--;
        }
    } else if (entry_did->is_static) {
        /* 如果试图修改静态绑定，错误输出 */
        if ((entry_did->max_apdu != max_apdu) || (!address_equal(&entry_did->address, addr))) {
            APP_ERROR("%s: failed cause the device_id(%d) entry exists in static cache\r\n", __func__,
                device_id);
        }
        pthread_mutex_unlock(&(Cache_Manager.lock));
        return -EPERM;
    } else if (address_equal(&entry_did->address, addr)) {    /* already exists */
        __list_del_entry(&(entry_did->l_node));
        (Cache_Manager.active_count)--;
    } else {    /* map changed */
        APP_WARN("%s: device_id(%d) address changed\r\n", __func__, device_id);
        entry_adr = _device_find(addr);
        if (entry_adr == NULL) {
            /* adr map is wrong, delete it and reuse did map */
            __hlist_del(&(entry_did->adr_node));
            hash_add(Cache_Manager.a2d_table, &entry_did->adr_node, __address_hash(addr));
        } else if (entry_adr->is_static) {
            APP_ERROR("%s: failed cause the device_id(%d) address entry exists in static cache\r\n",
                __func__, device_id);
            pthread_mutex_unlock(&(Cache_Manager.lock));
            return -EPERM;
        } else {
            __hlist_del(&(entry_did->adr_node));
            /* 因entry_addr的adr映射正确，可以复用，不用重新hash_add */
            hlist_replace(&(entry_adr->adr_node), &(entry_did->adr_node));
            __hlist_del(&(entry_adr->did_node));
            __list_del_entry(&(entry_adr->l_node));
            free(entry_adr);
            (Cache_Manager.active_count)--;
        }

        entry_did->address = *addr;
        __list_del_entry(&(entry_did->l_node));
        (Cache_Manager.active_count)--;
    }

    /* update entry */
    entry_did->is_static = is_static;
    entry_did->max_apdu = max_apdu;

    if (is_static) {
        list_add(&(entry_did->l_node), &(Cache_Manager.static_list));
        (Cache_Manager.static_count)++;
    } else {
        entry_did->update_time = el_current_second();
        list_add(&(entry_did->l_node), &(Cache_Manager.active_list));
        (Cache_Manager.active_count)++;
    }

    pthread_mutex_unlock(&(Cache_Manager.lock));

    if (!is_static) {
        whois_remove(device_id);
    }
    
    return OK;
}

void address_delete(uint32_t device_id)
{
    Address_Cache_Entry_t *entry;

    if (device_id >= BACNET_MAX_INSTANCE) {
        APP_ERROR("%s: invalid device id(%u)\r\n", __func__, device_id);
        return;
    }

    pthread_mutex_lock(&(Cache_Manager.lock));

    entry = _address_find(device_id);
    if (entry == NULL) {
        APP_ERROR("%s: find device_id(%d) from d2a_table failed\r\n", __func__, device_id);
        return;
    }

    __hlist_del(&(entry->did_node));
    __hlist_del(&(entry->adr_node));
    list_del(&(entry->l_node));

    if (entry->is_static) {
        (Cache_Manager.static_count)--;
    } else {
        (Cache_Manager.active_count)--;
    }
    
    free(entry);

    pthread_mutex_unlock(&(Cache_Manager.lock));
}

void address_destroy(void)
{
    Address_Cache_Entry_t *entry, *tmp;
    struct list_head *head[2];
    int i;

    head[0] = &(Cache_Manager.static_list);
    head[1] = &(Cache_Manager.active_list);

    pthread_mutex_lock(&(Cache_Manager.lock));

    for (i = 0; i < 2; i++) {
        list_for_each_entry_safe(entry, tmp, head[i], l_node) {
            __list_del_entry(&(entry->l_node));
            __hlist_del(&(entry->did_node));
            __hlist_del(&(entry->adr_node));
            free(entry);
        }
    }

    hash_init(Cache_Manager.d2a_table);
    hash_init(Cache_Manager.a2d_table);
    Cache_Manager.active_count = 0;
    Cache_Manager.static_count = 0;
    
    pthread_mutex_unlock(&(Cache_Manager.lock));
}

/* returns true and the address and max apdu if device is already bound */
bool query_address_from_device(uint32_t device_id, uint32_t *max_apdu, bacnet_addr_t *addr)
{
    Address_Cache_Entry_t *entry;
    unsigned cur_time;
    uint16_t net;
    bool send_whois;

    if (device_id >= BACNET_MAX_INSTANCE) {
        APP_ERROR("%s: invalid device id(%u)\r\n", __func__, device_id);
        return false;
    }

    if (device_id == device_object_instance_number()) {
        APP_ERROR("%s: the request device_id(%d) cannot be My DeviceID\r\n", __func__, device_id);
        return false;
    }
    
    net = BACNET_BROADCAST_NETWORK;
    send_whois = false;
    device_id &= BACNET_MAX_INSTANCE;
    cur_time = el_current_second();
    
    pthread_mutex_lock(&(Cache_Manager.lock));
    
    entry = _address_find(device_id);
    if (entry) {
        if ((entry->is_static)
                || (cur_time - entry->update_time < Address_Cache_TTL)) {
            if (addr) {
                *addr = entry->address;
            }
            if (max_apdu) {
                *max_apdu = entry->max_apdu;
            }
            
            if (!entry->is_static) {
                __list_del_entry(&(entry->l_node));
                list_add(&(entry->l_node), &(Cache_Manager.active_list));

                if (cur_time - entry->update_time > (Address_Cache_TTL/2)) {
                    send_whois = true;
                    net = entry->address.net;
                }
            }
        } else {
            net = entry->address.net;
            send_whois= true;

            list_del(&(entry->l_node));
            __hlist_del(&(entry->did_node));
            __hlist_del(&(entry->adr_node));
            free(entry);
            entry = NULL;
            (Cache_Manager.active_count)--;
        }
    } else {
        send_whois = true;
    }
    
    pthread_mutex_unlock(&(Cache_Manager.lock));

    if (send_whois) {
        (void)send_whois_cached(device_id, net);
    }

    return (entry != NULL);
}

/* returns true and the address and max apdu if device is already bound */
bool query_device_from_address(const bacnet_addr_t *addr, uint32_t *max_apdu, uint32_t *device_id)
{
    Address_Cache_Entry_t *entry;
    unsigned cur_time;

    if (addr == NULL) {
        APP_ERROR("%s: invalid addr argument\r\n", __func__);
        return -EINVAL;
    }

    cur_time = el_current_second();

    pthread_mutex_lock(&(Cache_Manager.lock));

    entry = _device_find(addr);
    if (entry) {
        if ((entry->is_static)
                || (cur_time - entry->update_time < Address_Cache_TTL)) {
            if (device_id) {
                *device_id = entry->device_id;
            }
            if (max_apdu) {
                *max_apdu = entry->max_apdu;
            }
            
            if (!entry->is_static) {
                __list_del_entry(&(entry->l_node));
                list_add(&(entry->l_node), &(Cache_Manager.active_list));
            }
        } else {
            list_del(&(entry->l_node));
            __hlist_del(&(entry->did_node));
            __hlist_del(&(entry->adr_node));
            free(entry);
            entry = NULL;
            (Cache_Manager.active_count)--;
        }
    }

    pthread_mutex_unlock(&(Cache_Manager.lock));

    return (entry != NULL);
}

/**
 * address_get_by_net - 根据网络号搜索出所有网络地址
 *
 * @net: 匹配网络号，0xFFFF表示匹配所有网络号
 * @addr: 用于保存返回的bacnet address列表
 * @max_apdu: 用于保存返回的max_apdu列表，可为NULL
 * @size: 指出最大可返回的列表数
 *
 * @return: 返回列表数，<0为错误
 *
 */
int address_get_by_net(uint16_t net, uint32_t *max_apdu, bacnet_addr_t *addr, unsigned size)
{
    Address_Cache_Entry_t *entry, *tmp;
    unsigned copied;
    unsigned cur_seconds;

    if (!size) {
        APP_ERROR("%s: invalid size(%d)\r\n", __func__, size);
        return -EINVAL;
    }

    copied = 0;
    
    pthread_mutex_lock(&(Cache_Manager.lock));

    list_for_each_entry(entry, &(Cache_Manager.static_list), l_node) {
        if ((net == 0xFFFF) || (entry->address.net == net)) {
            if (addr) {
                *(addr++) = entry->address;
            }
            if (max_apdu) {
                *(max_apdu++) = entry->max_apdu;
            }
            
            if (++copied >= size) {
                goto out;
            }
        }
    }

    cur_seconds = el_current_second();

    list_for_each_entry_safe(entry, tmp, &(Cache_Manager.static_list), l_node) {
        if (net == 0xFFFF || entry->address.net == net) {
            if (cur_seconds - entry->update_time >= Address_Cache_TTL) {
                hash_del(&(entry->did_node));
                list_del(&(entry->l_node));
                (Cache_Manager.active_count)--;
                free(entry);
                continue;
            }

            if (addr) {
                *(addr++) = entry->address;
            }
            if (max_apdu) {
                *(max_apdu++) = entry->max_apdu;
            }
            
            if (++copied >= size) {
                break;
            }
        }
    }

out:
    pthread_mutex_unlock(&(Cache_Manager.lock));

    return copied;
}

static int _encode_address_binding(uint8_t *pdu, Address_Cache_Entry_t *entry)
{
    int len;
    
    len = encode_application_object_id(pdu, OBJECT_DEVICE, entry->device_id);
    len += encode_application_unsigned(&pdu[len], entry->address.net);
    len += encode_application_raw_octet_string(&pdu[len], entry->address.adr, entry->address.len);

    return len;
}

static inline Address_Cache_Entry_t *_address_find_index(uint32_t index)
{
    Address_Cache_Entry_t *entry;
    struct list_head *head;

    if (index == 0) {
        return NULL;
    }

    if (index <= Cache_Manager.static_count) {
        head = &(Cache_Manager.static_list);
    } else {
        index -= Cache_Manager.static_count;
        if (index > Cache_Manager.active_count) {
            return NULL;
        }
        head = &(Cache_Manager.active_list);
    }

    entry = list_first_entry_or_null(head, Address_Cache_Entry_t, l_node);
    while (entry && (--index)) {
        if (entry->l_node.next == head) {
            return NULL;
        }
        entry = list_next_entry(entry, l_node);
    }

    return entry;
}

static inline Address_Cache_Entry_t *_address_find_next(Address_Cache_Entry_t *entry)
{
    struct list_head *next = entry->l_node.next;

    if (entry->is_static) {
        if (entry->l_node.next != &Cache_Manager.static_list) {
            return list_entry(next, Address_Cache_Entry_t, l_node);
        }
        next = Cache_Manager.active_list.next;
    }

    if (next == &Cache_Manager.active_list)
        return NULL;

    return list_entry(next, Address_Cache_Entry_t, l_node);
}


static int read_address_binding_RR(BACNET_READ_PROPERTY_DATA *rp_data, RR_RANGE *range)
{
    Address_Cache_Entry_t *entry;
    BACNET_BIT_STRING ResultFlags;
    uint8_t *pdu;
    int index;
    int item_count;
    int pdu_len, item_data_len;
    uint8_t item_data[MAX_APDU];
    uint8_t value;

    if ((range->RequestType != RR_BY_POSITION) && (range->RequestType != RR_READ_ALL)) {
        APP_ERROR("%s: invalid RR_RequestType(%d)\r\n", __func__, range->RequestType);
        rp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
        return BACNET_STATUS_ERROR;
    }

    pthread_mutex_lock(&(Cache_Manager.lock));

    item_count = Cache_Manager.static_count + Cache_Manager.active_count;
    
    if (range->RequestType == RR_BY_POSITION) {
        if (range->Count < 0) {
            index = range->Range.RefIndex;
            index += range->Count + 1;
            if (index < 1) {
                range->Count = range->Range.RefIndex;
                range->Range.RefIndex = 1;
            } else {
                range->Count = -range->Count;
                range->Range.RefIndex = index;
            }
        }
    } else {
        range->Range.RefIndex = 1;
        range->Count = item_count;
    }

    if (range->Range.RefIndex > item_count) {
        APP_ERROR("%s: invalid RR RefIndex(%d)\r\n", __func__, range->Range.RefIndex);
        pthread_mutex_unlock(&(Cache_Manager.lock));
        rp_data->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
        return BACNET_STATUS_ERROR;
    }

    if (range->Range.RefIndex == 0) {
        range->Range.RefIndex = 1;
        range->Count = range->Count - 1;
    }

    if (range->Count == 0) {
        APP_ERROR("%s: invalid RR Count(%d)\r\n", __func__, range->Count);
        pthread_mutex_unlock(&(Cache_Manager.lock));
        rp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
        return BACNET_STATUS_ERROR;
    }
    
    if ((range->Range.RefIndex + range->Count - 1) > item_count) {
        range->Count = item_count + 1 - range->Range.RefIndex;
    }
    
    item_count = 0;
    item_data_len = 0;
    value = 0;
    (void)bitstring_init(&ResultFlags, &value, 3);

    entry = _address_find_index(range->Range.RefIndex);
    while (entry) {
        if ((item_data_len + 18) > sizeof(item_data)) {
            bitstring_set_bit(&ResultFlags, RESULT_FLAG_MORE_ITEMS, true);
            break;
        }

        item_data_len += _encode_address_binding(&item_data[item_data_len], entry);
        item_count++;
        if (item_count >= range->Count) {
            bitstring_set_bit(&ResultFlags, RESULT_FLAG_LAST_ITEM, true);
            break;
        }

        entry = _address_find_next(entry);
    }

    pthread_mutex_unlock(&(Cache_Manager.lock));
    
    pdu = rp_data->application_data;
    if (item_count != 0) {
        bitstring_set_bit(&ResultFlags, RESULT_FLAG_FIRST_ITEM, true);
    }

    /* Context 3 BACnet Result Flags */
    pdu_len = encode_context_bitstring(pdu, 3, &ResultFlags);
    
    /* Context 4 Item Count */
    pdu_len += encode_context_unsigned(&pdu[pdu_len], 4, item_count);

    /* Context 5 Property list */
    pdu_len += encode_opening_tag(&pdu[pdu_len], 5);
    if (item_count != 0) {
        if (pdu_len + item_data_len >= rp_data->application_data_len) {
            rp_data->abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
            return BACNET_STATUS_ABORT;
        }
        
        memcpy(&pdu[pdu_len], item_data, item_data_len);
        pdu_len += item_data_len;
    }
    pdu_len += encode_closing_tag(&pdu[pdu_len], 5);

    return pdu_len;
}

int read_address_binding(BACNET_READ_PROPERTY_DATA *rp_data, RR_RANGE *range)
{
    uint8_t *pdu;
    uint32_t pdu_len;
    Address_Cache_Entry_t *entry, *tmp;
    unsigned cur_seconds;
    int len;
    
    if (rp_data == NULL) {
        APP_ERROR("%s: invalid argument\r\n", __func__);
        return BACNET_STATUS_ABORT;
    }

    if (range != NULL) {
        return read_address_binding_RR(rp_data, range);
    }

    pdu = rp_data->application_data;
    pdu_len = rp_data->application_data_len;
    len = 0;
    
    pthread_mutex_lock(&(Cache_Manager.lock));

    list_for_each_entry(entry, &(Cache_Manager.static_list), l_node) {
        len += _encode_address_binding(&pdu[len], entry);
        if (len > pdu_len) {
            APP_ERROR("%s: no spacing for encoding\r\n", __func__);
            pthread_mutex_unlock(&(Cache_Manager.lock));
            rp_data->abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
            return BACNET_STATUS_ABORT;
        }
    }
    
    cur_seconds = el_current_second();

    list_for_each_entry_safe(entry, tmp, &(Cache_Manager.active_list), l_node) {
        if (cur_seconds - entry->update_time >= Address_Cache_TTL) {
            hash_del(&(entry->did_node));
            hash_del(&(entry->adr_node));
            list_del(&(entry->l_node));
            free(entry);
            (Cache_Manager.active_count)--;
            continue;
        }

        len += _encode_address_binding(&pdu[len], entry);
        if (len > pdu_len) {
            APP_ERROR("%s: no spacing for encoding\r\n", __func__);
            pthread_mutex_unlock(&(Cache_Manager.lock));
            rp_data->abort_reason = ABORT_REASON_SEGMENTATION_NOT_SUPPORTED;
            return BACNET_STATUS_ABORT;
        }
    }

    pthread_mutex_unlock(&(Cache_Manager.lock));
    
    return len;
}

int address_init(cJSON *cfg)
{
    cJSON *tmp;
    int rv;
    
    if (cfg == NULL) {
        APP_ERROR("%s: null cfg\r\n", __func__);
        return -EINVAL;
    }

    tmp = cJSON_GetObjectItem(cfg, "Address_Cache_TTL");
    if ((tmp != NULL) && (tmp->type != cJSON_Number)) {
        APP_ERROR("%s: invalid Address_Cache_TTL item\r\n", __func__);
        return -EPERM;
    } else if (tmp != NULL){
        if (tmp->valueint < (WHOIS_MIN_INTERVAL * 2)) {
            APP_WARN("%s: too small Address_Cache_TTL(%d), use %d\r\n", __func__,
                    tmp->valueint, WHOIS_MIN_INTERVAL * 2);
            Address_Cache_TTL = WHOIS_MIN_INTERVAL * 2;
        } else
            Address_Cache_TTL = tmp->valueint;
    }

    Whois_Max_Retry = Address_Cache_TTL / 2;
    if (Whois_Max_Retry > 65535) {
        Whois_Max_Retry = 65535;
    }

    tmp = cJSON_GetObjectItem(cfg, "Max_Address_Cache");
    if ((tmp != NULL) && (tmp->type != cJSON_Number)) {
        APP_ERROR("%s: invalid Max_Address_Cache item\r\n", __func__);
        return -EPERM;
    } else if (tmp != NULL) {
        if (tmp->valueint < MIN_CACHE_SIZE) {
            APP_WARN("%s: too small Max_Address_Cache(%d), use %d\r\n", __func__,
                    tmp->valueint, MIN_CACHE_SIZE);
            Max_Address_Cache = MIN_CACHE_SIZE;
        } else
            Max_Address_Cache = (uint32_t)tmp->valueint;
    }

    tmp = cJSON_GetObjectItem(cfg, "Max_WhoIs_Cache");
    if ((tmp != NULL) && (tmp->type != cJSON_Number)) {
        APP_ERROR("%s: invalid Max_WhoIs_Cache item\r\n", __func__);
        return -EPERM;
    } else if (tmp != NULL) {
        if (tmp->valueint < MIN_CACHE_SIZE) {
            APP_ERROR("%s: too small Max_WhoIs_Cache(%d), use %d\r\n", __func__,
                    tmp->valueint, MIN_CACHE_SIZE);
            Max_WhoIs_Cache = MIN_CACHE_SIZE;
        } else
            Max_WhoIs_Cache = (uint32_t)tmp->valueint;
    }

    Cache_Manager.active_count = 0;
    Cache_Manager.static_count = 0;
    hash_init(Cache_Manager.d2a_table);
    hash_init(Cache_Manager.a2d_table);
    INIT_LIST_HEAD(&(Cache_Manager.static_list));
    INIT_LIST_HEAD(&(Cache_Manager.active_list));

    Whois_Manager.count = 0;
    hash_init(Whois_Manager.table);
    INIT_LIST_HEAD(&(Whois_Manager.list));

    rv = pthread_mutex_init(&(Cache_Manager.lock), NULL);
    if (rv) {
        APP_ERROR("%s: init Cache_Manager lock failed cause %s\r\n", __func__, strerror(rv));
        return -EPERM;
    }

    rv = pthread_mutex_init(&(Whois_Manager.lock), NULL);
    if (rv) {
        APP_ERROR("%s: init Whois_Manager lock failed cause %s\r\n", __func__, strerror(rv));
        (void)pthread_mutex_destroy(&(Cache_Manager.lock));
        return -EPERM;
    }

    APP_VERBOS("%s: ok\r\n", __func__);

    return OK;
}

void address_exit(void)
{
    whois_destroy();
    address_destroy();

    (void)pthread_mutex_destroy(&(Whois_Manager.lock));
    (void)pthread_mutex_destroy(&(Cache_Manager.lock));
    
    return;
}

cJSON *get_address_binding(void)
{
    Address_Cache_Entry_t *entry, *safe;
    cJSON *tmp, *array, *result;
    unsigned cur_seconds;
    char mac[MAX_MAC_STR_LEN];
    int rv;
    
    result = cJSON_CreateObject();
    if (result == NULL) {
        APP_ERROR("%s: create result object failed\r\n", __func__);
        return NULL;
    }

    array = cJSON_CreateArray();
    if (array == NULL) {
        APP_ERROR("%s: create array item failed\r\n", __func__);
        cJSON_AddNumberToObject(result, "error_code", -1);
        cJSON_AddStringToObject(result, "reason", "create array item failed");
        return result;
    }

    pthread_mutex_lock(&(Cache_Manager.lock));

    list_for_each_entry(entry, &(Cache_Manager.static_list), l_node) {
        rv = bacnet_array_to_macstr(entry->address.adr, entry->address.len, mac, sizeof(mac));
        if (rv < 0) {
            APP_ERROR("%s: array to macstr failed(%d)\r\n", __func__, rv);
            pthread_mutex_unlock(&(Cache_Manager.lock));
            cJSON_Delete(array);
            cJSON_AddNumberToObject(result, "error_code", -1);
            cJSON_AddStringToObject(result, "reason", "array to macstr failed");
            return result;
        }

        tmp = cJSON_CreateObject();
        if (tmp == NULL) {
            APP_ERROR("%s: create cache entry item failed\r\n", __func__);
            pthread_mutex_unlock(&(Cache_Manager.lock));
            cJSON_Delete(array);
            cJSON_AddNumberToObject(result, "error_code", -1);
            cJSON_AddStringToObject(result, "reason", "create cache entry item failed");
            return result;
        }
        
        cJSON_AddItemToArray(array, tmp);
        cJSON_AddNumberToObject(tmp, "device_id", entry->device_id);
        cJSON_AddNumberToObject(tmp, "net_num", entry->address.net);
        cJSON_AddStringToObject(tmp, "mac", mac);
    }

    cur_seconds = el_current_second();

    list_for_each_entry_safe(entry, safe, &(Cache_Manager.active_list), l_node) {
        if (cur_seconds - entry->update_time >= Address_Cache_TTL) {
            hash_del(&(entry->did_node));
            hash_del(&(entry->adr_node));
            list_del(&(entry->l_node));
            free(entry);
            (Cache_Manager.active_count)--;
            continue;
        }

        rv = bacnet_array_to_macstr(entry->address.adr, entry->address.len, mac, sizeof(mac));
        if (rv < 0) {
            APP_ERROR("%s: array to macstr failed(%d)\r\n", __func__, rv);
            pthread_mutex_unlock(&(Cache_Manager.lock));
            cJSON_Delete(array);
            cJSON_AddNumberToObject(result, "error_code", -1);
            cJSON_AddStringToObject(result, "reason", "array to macstr failed");
            return result;
        }

        tmp = cJSON_CreateObject();
        if (tmp == NULL) {
            APP_ERROR("%s: create cache entry item failed\r\n", __func__);
            pthread_mutex_unlock(&(Cache_Manager.lock));
            cJSON_Delete(array);
            cJSON_AddNumberToObject(result, "error_code", -1);
            cJSON_AddStringToObject(result, "reason", "create cache entry item failed");
            return result;
        }
        
        cJSON_AddItemToArray(array, tmp);
        cJSON_AddNumberToObject(tmp, "device_id", entry->device_id);
        cJSON_AddNumberToObject(tmp, "net_num", entry->address.net);
        cJSON_AddStringToObject(tmp, "mac", mac);
    }
    
    pthread_mutex_unlock(&(Cache_Manager.lock));
    
    cJSON_AddItemToObject(result, "result", array);

    return result;
}

