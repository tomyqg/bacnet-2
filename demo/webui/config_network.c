/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * config_network.c
 * Original Author:  linzhixian, 2016-5-16
 *
 * Config Network
 *
 * History
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>

#include "bacnet/bacnet.h"
#include "bacnet/bacdef.h"
#include "bacnet/datalink.h"
#include "bacnet/mstp.h"
#include "misc/utils.h"
#include "misc/bits.h"
#include "webui.h"
#include "translate.h"
#include "web_service.h"
#include "config_network.h"
#include "bacnet/bip.h"

static pthread_rwlock_t network_cfg_rwlock;

static cJSON *resource_cfg = NULL;
static cJSON *port_array = NULL;
static bool netcfg_modify = false;

static bool conf_network_find_net_num(uint32_t net_num)
{
    cJSON *port, *tmp;

    cJSON_ArrayForEach(port, port_array) {
        tmp = cJSON_GetObjectItem(port, "net_num");
        if (tmp && (tmp->valueint == net_num)) {
            tmp = cJSON_GetObjectItem(port, "enable");
            if (tmp && (tmp->type == cJSON_True)) {
                return true;
            }
        }
    }
    
    return false;
}

static bool conf_network_find_udp_port(char *resource_name, uint32_t udp_port)
{
    cJSON *port, *tmp;
    
    cJSON_ArrayForEach(port, port_array) {
        tmp = cJSON_GetObjectItem(port, "resource_name");
        if (tmp && (strcmp(tmp->valuestring, resource_name) == 0)) {
            tmp = cJSON_GetObjectItem(port, "udp_port");
            if (tmp && (tmp->valueint == udp_port)) {
                tmp = cJSON_GetObjectItem(port, "enable");
                if (tmp && (tmp->type == cJSON_True)) {
                    return true;
                }
            }
        }
    }
    
    return false;
}

static const char *conf_network_init_port(cJSON *cfg, cJSON *port, const char *locale)
{
    cJSON *tmp;
    const char *res_type;
    char *resource_name;
    
    /* add enable item */
    cJSON_AddFalseToObject(port, "enable");

    /* add net_num item */
    cJSON_AddNumberToObject(port, "net_num", 1);

    /* add resource_name item */
    tmp = cJSON_GetObjectItem(cfg, "resource_name");
    if ((tmp == NULL) || (tmp->type != cJSON_String)) {
        return "Invalid resource_name item type";
    }

    res_type = datalink_get_type_by_resource_name(resource_cfg, tmp->valuestring);
    if (res_type == NULL) {
        return "Invalid resource name";
    }
    resource_name = tmp->valuestring;

    /* add dl_type item */
    tmp = cJSON_GetObjectItem(cfg, "dl_type");
    if ((tmp == NULL) || (tmp->type != cJSON_String)) {
        return "Invalid dl_type item type";
    }
    
    if (strcmp(tmp->valuestring, "MSTP") == 0) {
        if (strcmp(res_type, "USB")) {
            return "Resource type is not USB";
        }

        cJSON_AddStringToObject(port, "dl_type", tmp->valuestring);
        cJSON_AddStringToObject(port, "resource_name", resource_name);
        cJSON_AddNumberToObject(port, "baudrate", 9600);
        cJSON_AddNumberToObject(port, "this_station", 127);
        cJSON_AddNumberToObject(port, "max_master", 127);
        cJSON_AddNumberToObject(port, "max_info_frames", 1);
        cJSON_AddNumberToObject(port, "tx_buf_size", 4096);
        cJSON_AddNumberToObject(port, "rx_buf_size", 4096);
        cJSON_AddFalseToObject(port, "inv_polarity");
        cJSON_AddFalseToObject(port, "auto_baudrate");
        cJSON_AddFalseToObject(port, "auto_polarity");
        cJSON_AddFalseToObject(port, "auto_mac");
        cJSON_AddNumberToObject(port, "reply_timeout", 255);
        cJSON_AddNumberToObject(port, "usage_timeout", 20);
        cJSON_AddNumberToObject(port, "reply_fast_timeout", 4);
        cJSON_AddNumberToObject(port, "usage_fast_timeout", 1);
        cJSON_AddItemToObject(port, "fast_nodes", cJSON_CreateArray());
        cJSON *proxy = cJSON_CreateObject();
        cJSON_AddItemToObject(port, "proxy", proxy);
        cJSON_AddFalseToObject(proxy, "enable");
        cJSON_AddFalseToObject(proxy, "auto_discovery");
        cJSON_AddItemToObject(proxy, "discovery_nodes", cJSON_CreateArray());
        cJSON_AddNumberToObject(proxy, "scan_interval", 300);
        cJSON_AddItemToObject(proxy, "manual_binding", cJSON_CreateArray());
    } else if (strcmp(tmp->valuestring, "BIP") == 0) {
        if (strcmp(res_type, "ETH")) {
            return "Resource type is not ETH";
        }

        cJSON_AddStringToObject(port, "dl_type", tmp->valuestring);
        cJSON_AddStringToObject(port, "resource_name", resource_name);
        cJSON_AddNumberToObject(port, "udp_port", 47808);
    } else if (strcmp(tmp->valuestring, "ETH") == 0) {
        if (strcmp(res_type, "ETH")) {
            return "Resource type is not ETH";
        }

        cJSON_AddStringToObject(port, "dl_type", tmp->valuestring);
        cJSON_AddStringToObject(port, "resource_name", resource_name);
    } else {
        return "Unknown dl_type item type";
    }

    return NULL;
}

static void conf_network_add_port(const struct mg_request_info *ri, struct mg_connection *conn, 
                const char *locale)
{
    char post_data[8192];
    int post_data_len;
    cJSON *cfg, *new_port;
    const char *msg;
    
    if (strcmp(ri->request_method, "POST")) {
        errResp(conn, "This entity only support POST method");
        return;
    }

    post_data_len = mg_read(conn, post_data, sizeof(post_data));
    if (post_data_len == sizeof(post_data)) {
        errResp(conn, "Too long post");
        return;
    }
    post_data[post_data_len] = 0;
    
    cfg = cJSON_Parse(post_data);
    if (cfg == NULL) {
        errResp(conn, "Invalid json string");
        return;
    }

    new_port = cJSON_CreateObject();
    if (new_port == NULL) {
        errResp(conn, "Create object failed");
        goto out;
    }
    
    msg = conf_network_init_port(cfg, new_port, locale);
    if (msg) {
        errResp(conn, msg);
        goto out;
    }

    jsonResp(conn, new_port);
    
    RWLOCK_WRLOCK(&network_cfg_rwlock);
    cJSON_AddItemToArray(port_array, new_port);
    netcfg_modify = true;
    RWLOCK_UNLOCK(&network_cfg_rwlock);
    
out:
    cJSON_Delete(cfg);
    
    return;
}

static void conf_network_delete_port(const struct mg_request_info *ri, struct mg_connection *conn,
                const char *locale)
{
    cJSON *cfg, *tmp;
    char post_data[8192];
    int post_data_len;
    
    if (strcmp(ri->request_method, "POST")) {
        errResp(conn, "This entity only support POST method");
        return;
    }
    
    post_data_len = mg_read(conn, post_data, sizeof(post_data));
    if (post_data_len == sizeof(post_data)) {
        errResp(conn, "Too long post");
        return;
    }
    post_data[post_data_len] = 0;

    cfg = cJSON_Parse(post_data);
    if (cfg == NULL) {
        errResp(conn, "Invalid json string");
        return;
    }

    tmp = cJSON_GetObjectItem(cfg, "port_id");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        errResp(conn, "Invalid port_id item type");
        goto out;
    }

    if (tmp->valueint < 0) {
        errResp(conn, "Invalid port_id");
        goto out;
    }

    RWLOCK_WRLOCK(&network_cfg_rwlock);
    
    if (cJSON_GetArrayItem(port_array, tmp->valueint) == NULL) {
        RWLOCK_UNLOCK(&network_cfg_rwlock);
        jsonErrResp(conn, TLT("The port_id is not present"));
        goto out;
    }
    cJSON_DeleteItemFromArray(port_array, tmp->valueint);
    netcfg_modify = true;
    
    RWLOCK_UNLOCK(&network_cfg_rwlock);
    
    jsonErrResp(conn, NULL);

out:
    cJSON_Delete(cfg);

    return;
}

static const char *__conf_network_set_port(cJSON *cfg, cJSON *port, const char *locale)
{
    cJSON *tmp, *tmp_2, *ori_enable;
    char *dl_type;
    bool enable;
    
    /* check dl_type item */
    tmp = cJSON_GetObjectItem(cfg, "dl_type");
    if ((tmp == NULL) || (tmp->type != cJSON_String)) {
        return "Invalid dl_type item type";
    }

    tmp_2 = cJSON_GetObjectItem(port, "dl_type");
    if ((tmp_2 == NULL) || (tmp_2->type != cJSON_String)) {
        return "Invalid original port dl_type item type";
    }

    if (strcmp(tmp->valuestring, tmp_2->valuestring)) {
        return TLT("The dl_type item conflict");
    }
    dl_type = tmp->valuestring;

    /* set enable item */
    tmp = cJSON_GetObjectItem(cfg, "enable");
    if (!tmp || ((tmp->type != cJSON_True) && (tmp->type != cJSON_False))) {
        return "Invalid enable item type";
    }
    
    tmp_2 = cJSON_GetObjectItem(port, "enable");
    if ((tmp_2 == NULL) || ((tmp_2->type != cJSON_True) && (tmp_2->type != cJSON_False))) {
        return "Invalid original port enable item type";
    }
    
    ori_enable = tmp_2;
    enable = (tmp->type == cJSON_True)? true: false;

    /* set net_num item */
    tmp = cJSON_GetObjectItem(cfg, "net_num");
    if (!tmp || tmp->type != cJSON_Number) {
        return "Invalid net_num item type";
    }

    if ((tmp->valueint <= 0) || (tmp->valueint >= BACNET_BROADCAST_NETWORK)) {
        return "Invalid net_num";
    }

    tmp_2 = cJSON_GetObjectItem(port, "net_num");
    if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Number)) {
        return "Invalid original net_num item type";
    }

    if (enable && (ori_enable->type != cJSON_True || tmp->valueint != tmp_2->valueint)) {
        if (conf_network_find_net_num(tmp->valueint))
            return TLT("The net_num is already present");
    }

    if (!enable) {
        /* If not enable, it's should be set here.
         * then net_num could be safely set */
        ori_enable->type = cJSON_False;
    }
    tmp_2->valueint = tmp->valueint;
    tmp_2->valuedouble = tmp->valueint;

    /* set resource_name item */
    char *resource_name;
    tmp = cJSON_GetObjectItem(cfg, "resource_name");
    if (!tmp || tmp->type != cJSON_String) {
        return "Invalid resource_name item type";
    }

    tmp_2 = cJSON_GetObjectItem(port, "resource_name");
    if ((tmp_2 == NULL) || (tmp_2->type != cJSON_String)) {
        return "Invalid original port resource_name item type";
    }

    if (strcmp(tmp->valuestring, tmp_2->valuestring)) {
        return TLT("Resource_name item conflict");
    }
    resource_name = tmp->valuestring;

    if (strcmp(dl_type, "MSTP") == 0) {
        /* set baudrate item */
        tmp = cJSON_GetObjectItem(cfg, "baudrate");
        if (!tmp || tmp->type != cJSON_Number) {
            return "Invalid baudrate item type";
        }

        /* check baudrate */
        if ((tmp->valueint < 0) || (mstp_baudrate2enum(tmp->valueint) == MSTP_B_MAX)) {
            return TLT("Invalid baudrate");
        }

        tmp_2 = cJSON_GetObjectItem(port, "baudrate");
        if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Number)) {
            return "Invalid original port baudrate item type";
        }

        tmp_2->valueint = tmp->valueint;
        tmp_2->valuedouble = tmp->valueint;

        /* set this_station item */
        tmp = cJSON_GetObjectItem(cfg, "this_station");
        if (!tmp || tmp->type != cJSON_Number) {
            return "Invalid this_station item type";
         }

        if ((tmp->valueint < 0) || (tmp->valueint > 127)) {
            return TLT("Invalid this_station");
        }

        tmp_2 = cJSON_GetObjectItem(port, "this_station");
        if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Number)) {
            return "Invalid original port this_station item type";
        }

        tmp_2->valueint = tmp->valueint;
        tmp_2->valuedouble = tmp->valueint;

        /* set max_master item */
        tmp = cJSON_GetObjectItem(cfg, "max_master");
        if (tmp) {
            if (tmp->type != cJSON_Number) {
                return "Invalid max_master item type";
            }

            /* max_master should >= this_station and from 1~127 */
            if ((tmp->valueint <= 0) || (tmp->valueint > 127)
                    || (tmp->valueint < tmp_2->valueint)) {
                return TLT("Invalid max_master");
            }

            tmp_2 = cJSON_GetObjectItem(port, "max_master");
            if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Number)) {
                return "Invalid original port max_master item type";
            }

            tmp_2->valueint = tmp->valueint;
            tmp_2->valuedouble = tmp->valueint;
        }
        
        /* set max_info_frames item */
        tmp = cJSON_GetObjectItem(cfg, "max_info_frames");
        if (tmp) {
            if (tmp->type != cJSON_Number) {
                return "Invalid max_info_frames item type";
            }

            if (tmp->valueint <= 0 || tmp->valueint > 255) {
                return TLT("Invalid max_info_frames");
            }

            tmp_2 = cJSON_GetObjectItem(port, "max_info_frames");
            if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Number)) {
                return "Invalid original port max_info_frames item type";
            }

            tmp_2->valueint = tmp->valueint;
            tmp_2->valuedouble = tmp->valueint;
        }

        /* set tx_buf_size item */
        tmp = cJSON_GetObjectItem(cfg, "tx_buf_size");
        if (tmp) {
            if (tmp->type != cJSON_Number) {
                return "Invalid tx_buf_size item type";
            }

            if (tmp->valueint < 4096 || tmp->valueint > 65536) {
                return TLT("Invalid max_info_frames");
            }

            tmp_2 = cJSON_GetObjectItem(port, "tx_buf_size");
            if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Number)) {
                return "Invalid original port tx_buf_size item type";
            }

            tmp_2->valueint = tmp->valueint;
            tmp_2->valuedouble = tmp->valueint;
        }

        tmp = cJSON_GetObjectItem(cfg, "inv_polarity");
        if (tmp) {
            if ((tmp->type != cJSON_True) && (tmp->type != cJSON_False)) {
                return "Invalid inv_polarity item type";
            }

            tmp_2 = cJSON_GetObjectItem(port, "inv_polarity");
            if (!tmp_2 || (tmp_2->type != cJSON_True && tmp_2->type != cJSON_False)) {
                return "Invalid original port inv_polarity item type";
            }

            tmp_2->type = tmp->type;
        }
        
        tmp = cJSON_GetObjectItem(cfg, "auto_baudrate");
        if (tmp) {
            if ((tmp->type != cJSON_True) && (tmp->type != cJSON_False)) {
                return "Invalid auto_baudrate item type";
            }

            tmp_2 = cJSON_GetObjectItem(port, "auto_baudrate");
            if (!tmp_2 || (tmp_2->type != cJSON_True && tmp_2->type != cJSON_False)) {
                return "Invalid original port auto_baudrate item type";
            }

            tmp_2->type = tmp->type;
        }

        tmp = cJSON_GetObjectItem(cfg, "auto_polarity");
        if (tmp) {
            if ((tmp->type != cJSON_True) && (tmp->type != cJSON_False)) {
                return "Invalid auto_polarity item type";
            }

            tmp_2 = cJSON_GetObjectItem(port, "auto_polarity");
            if (!tmp_2 || (tmp_2->type != cJSON_True && tmp_2->type != cJSON_False)) {
                return "Invalid original port auto_polarity item type";
            }

            tmp_2->type = tmp->type;
        }

        tmp = cJSON_GetObjectItem(cfg, "auto_mac");
        if (tmp) {
            if ((tmp->type != cJSON_True) && (tmp->type != cJSON_False)) {
                return "Invalid auto_mac item type";
            }

            tmp_2 = cJSON_GetObjectItem(port, "auto_mac");
            if (!tmp_2 || (tmp_2->type != cJSON_True && tmp_2->type != cJSON_False)) {
                return "Invalid original port auto_mac item type";
            }

            tmp_2->type = tmp->type;
        }
        
        /* set reply_timeout */
        tmp = cJSON_GetObjectItem(cfg, "reply_timeout");
        if (tmp) {
            if (tmp->type != cJSON_Number) {
                return "Invalid reply_timeout item type";
            }

            if ((tmp->valueint < 255) || (tmp->valueint > 300)) {
                return TLT("Invalid reply_timeout");
            }

            tmp_2 = cJSON_GetObjectItem(port, "reply_timeout");
            if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Number)) {
                return "Invalid original port reply_timeout item type";
            }

            tmp_2->valueint = tmp->valueint;
            tmp_2->valuedouble = tmp->valueint;
        }

        /* set reply_fast_timeout */
        tmp = cJSON_GetObjectItem(cfg, "reply_fast_timeout");
        if (tmp) {
            if (tmp->type != cJSON_Number) {
                return "Invalid reply_fast_timeout item type";
            }

            if ((tmp->valueint < 1) || (tmp->valueint >= tmp_2->valueint)) {
                return TLT("Invalid reply_fast_timeout");
            }

            tmp_2 = cJSON_GetObjectItem(port, "reply_fast_timeout");
            if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Number)) {
                return "Invalid original port reply_fast_timeout item type";
            }

            tmp_2->valueint = tmp->valueint;
            tmp_2->valuedouble = tmp->valueint;
        }

        /* set usage_timeout */
        tmp = cJSON_GetObjectItem(cfg, "usage_timeout");
        if (tmp) {
            if (tmp->type != cJSON_Number) {
                return "Invalid usage_timeout item type";
            }

            if ((tmp->valueint < 20) || (tmp->valueint > 100)) {
                return TLT("Invalid usage_timeout");
            }

            tmp_2 = cJSON_GetObjectItem(port, "usage_timeout");
            if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Number)) {
                return "Invalid original port usage_timeout item type";
            }

            tmp_2->valueint = tmp->valueint;
            tmp_2->valuedouble = tmp->valueint;
        }
        
        /* set usage_fast_timeout */
        tmp = cJSON_GetObjectItem(cfg, "usage_fast_timeout");
        if (tmp) {
            if (tmp->type != cJSON_Number) {
                return "Invalid usage_fast_timeout item type";
            }

            if ((tmp->valueint < 1) || (tmp->valueint >= tmp_2->valueint)) {
                return TLT("Invalid usage_timeout");
            }

            tmp_2 = cJSON_GetObjectItem(port, "usage_fast_timeout");
            if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Number)) {
                return "Invalid original port usage_fast_timeout item type";
            }

            tmp_2->valueint = tmp->valueint;
            tmp_2->valuedouble = tmp->valueint;
        }

        tmp = cJSON_GetObjectItem(cfg, "fast_nodes");
        if (tmp) {
            if (tmp->type != cJSON_Array) {
                return "Invalid fast_nodes item type";
            }
            
            uint8_t bits[32] = {0};
            cJSON *item;
            cJSON_ArrayForEach(item, tmp) {
                if (item->type != cJSON_Number) {
                    return "fast_nodes should be array of number";
                }
                
                if ((item->valueint < 0) || (item->valueint > 254)) {
                    return "number inside fast_nodes array shoud be 0~254";
                }
                
                if (get_bit(bits, item->valueint)) {
                    return "duplicate number inside fast_nodes array";
                }
                set_bit(bits, item->valueint);
            }

            tmp_2 = cJSON_GetObjectItem(port, "fast_nodes");
            if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Array)) {
                return "Invalid original port fast_nodes item type";
            }

            cJSON_DeleteItemFromObject(port, "fast_nodes");
            cJSON_AddItemToObject(port, "fast_nodes", cJSON_DetachItemFromObject(cfg, "fast_nodes"));
        }

        cJSON *proxy = cJSON_GetObjectItem(cfg, "proxy");
        if (proxy != NULL) {
            if (proxy->type != cJSON_Object) {
                return "Invalid proxy item type";
            }

            cJSON *ori_proxy = cJSON_GetObjectItem(port, "proxy");
            if ((ori_proxy == NULL) || ori_proxy->type != cJSON_Object) {
                return "Invalid original proxy item type";
            }

            tmp = cJSON_GetObjectItem(proxy, "enable");
            if ((tmp != NULL) && (tmp->type != cJSON_True)
                    && (tmp->type != cJSON_False)) {
                return "Invalid enable item type in proxy";
            } else if (tmp != NULL) {
                tmp_2 = cJSON_GetObjectItem(ori_proxy, "enable");
                if ((tmp_2 == NULL)
                        || (tmp_2->type != cJSON_True
                                && tmp_2->type != cJSON_False)) {
                    return "Invalid original enable item type in proxy";
                }
                tmp_2->type = tmp->type;
            }

            tmp = cJSON_GetObjectItem(proxy, "auto_discovery");
            if ((tmp != NULL) && (tmp->type != cJSON_True)
                    && (tmp->type != cJSON_False)) {
                return "Invalid auto_discovery item type in proxy";
            } else if (tmp != NULL) {
                tmp_2 = cJSON_GetObjectItem(ori_proxy, "auto_discovery");
                if ((tmp_2 == NULL)
                        || (tmp_2->type != cJSON_True
                                && tmp_2->type != cJSON_False)) {
                    return "Invalid original auto_discovery item type in proxy";
                }
                tmp_2->type = tmp->type;
            }

            tmp = cJSON_GetObjectItem(proxy, "scan_interval");
            if ((tmp != NULL) && (tmp->type != cJSON_Number)) {
                return "Invalid scan_interval item type in proxy";
            } else if (tmp != NULL) {
                if (tmp->valueint < 120 || tmp->valueint > 65535) {
                    return "Invalid scan_interval in proxy";
                }
                tmp_2 = cJSON_GetObjectItem(ori_proxy, "scan_interval");
                if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Number)) {
                    return "Invalid original scan_interval item type in proxy";
                }
                tmp_2->valueint = tmp->valueint;
                tmp_2->valuedouble = tmp->valuedouble;
            }

            tmp = cJSON_GetObjectItem(proxy, "discovery_nodes");
            if ((tmp != NULL) && (tmp->type != cJSON_Array)) {
                return "Invalid discovery_nodes item type in proxy";
            } else if (tmp != NULL) {
                uint8_t bits[32] = {0};
                cJSON *item;
                cJSON_ArrayForEach(item, tmp) {
                    if (item->type != cJSON_Number) {
                        return "discovery_nodes in proxy should be array of number";
                    }

                    if ((item->valueint < 0) || (item->valueint > 254)) {
                        return "number inside discovery_nodes array in proxy shoud be 0~254";
                    }

                    if (get_bit(bits, item->valueint)) {
                        return "duplicate number inside discovery_nodes array in proxy";
                    }
                    set_bit(bits, item->valueint);
                }

                tmp_2 = cJSON_GetObjectItem(ori_proxy, "discovery_nodes");
                if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Array)) {
                    return "Invalid original discovery_nodes item type in proxy";
                }

                cJSON_DeleteItemFromObject(ori_proxy, "discovery_nodes");
                cJSON_AddItemToObject(ori_proxy, "discovery_nodes",
                        cJSON_DetachItemFromObject(proxy, "discovery_nodes"));
            }

            tmp = cJSON_GetObjectItem(proxy, "manual_binding");
            if ((tmp != NULL) && (tmp->type != cJSON_Array)) {
                return "Invalid manual_binding item type in proxy";
            } else if (tmp != NULL) {
                uint8_t bits[32] = {0};
                cJSON *item;
                cJSON_ArrayForEach(item, tmp) {
                    if (item->type != cJSON_String) {
                        return "manual_binding in proxy should be array of string";
                    }

                    unsigned mac, device_id;
                    int pos;
                    if (sscanf(item->valuestring, "%u:%u%n", &mac, &device_id, &pos) != 2
                            || pos != strlen(item->valuestring)) {
                        return "Invalid manual_binding in proxy";
                    }

                    if (mac >= 255) {
                        return "manual_binding in proxy invalid mac";
                    }

                    if (device_id >= BACNET_MAX_INSTANCE) {
                        return "manual_binding in proxy invalid device_id";
                    }

                    if (get_bit(bits, mac)) {
                        return "manual_binding in proxy duplicate mac";
                    }
                    set_bit(bits, mac);
                }

                tmp_2 = cJSON_GetObjectItem(ori_proxy, "manual_binding");
                if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Array)) {
                    return "Invalid original manual_binding item type in proxy";
                }

                cJSON_DeleteItemFromObject(ori_proxy, "manual_binding");
                cJSON_AddItemToObject(ori_proxy, "manual_binding",
                        cJSON_DetachItemFromObject(proxy, "manual_binding"));
            }
        }
    } else if (strcmp(dl_type, "BIP") == 0) {
        /* set udp_port item */
        tmp = cJSON_GetObjectItem(cfg, "udp_port");
        if (tmp) {
            if (tmp->type != cJSON_Number) {
                return "Invalid udp_port item type";
            }

            if ((tmp->valueint < 47808) || (tmp->valueint >= 65535)) {
                return TLT("The udp_port should be in range of 47808~65534");
            }

            tmp_2 = cJSON_GetObjectItem(port, "udp_port");
            if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Number)) {
                return "Invalid original port udp_port item type";
            }

            if (enable && (ori_enable->type != cJSON_True || tmp->valueint != tmp_2->valueint)) {
                if (conf_network_find_udp_port(resource_name, tmp->valueint))
                    return TLT("The udp_port is already present");
            }

            tmp_2->valueint = tmp->valueint;
            tmp_2->valuedouble = tmp->valueint;
        }
        
        /* set bbmd item */
        cJSON *bbmd, *ori_bbmd;
        bbmd = cJSON_GetObjectItem(cfg, "bbmd");
        if (bbmd) {
            if (bbmd->type != cJSON_Object) {
                return "Invalid bbmd item type";
            }

            ori_bbmd = cJSON_GetObjectItem(port, "bbmd");
            if ((ori_bbmd != NULL) && (ori_bbmd->type != cJSON_Object)) {
                return "Invalid original port bbmd item type";
            }
            if (ori_bbmd == NULL) {
                ori_bbmd = cJSON_CreateObject();
                cJSON_AddNumberToObject(ori_bbmd, "push_interval", 0);
                cJSON_AddFalseToObject(ori_bbmd, "router_broadcast");
                cJSON_AddItemToObject(ori_bbmd, "BDT", cJSON_CreateArray());
                cJSON_AddItemToObject(port, "bbmd", ori_bbmd);
            }

            tmp = cJSON_GetObjectItem(bbmd, "push_interval");
            if (tmp) {
                if (tmp->type != cJSON_Number) {
                    return "Invalid push_interval item type";
                }

                if (tmp->valueint < 0) {
                    return TLT("Invalid push_interval");
                }

                tmp_2 = cJSON_GetObjectItem(ori_bbmd, "push_interval");
                if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Number)) {
                    return "Invalid original port push_interval item type";
                }
                tmp_2->valueint = tmp->valueint;
                tmp_2->valuedouble = tmp->valueint;
            }
            
            tmp = cJSON_GetObjectItem(bbmd, "router_broadcast");
            if (tmp) {
                if ((tmp->type != cJSON_True) && (tmp->type != cJSON_False)) {
                    return "Invalid router_broadcast item type";
                }

                tmp_2 = cJSON_GetObjectItem(ori_bbmd, "router_broadcast");
                if (!tmp_2 || ((tmp->type != cJSON_True) && (tmp->type != cJSON_False))) {
                    return "Invalid original port router_broadcast item type";
                }
                tmp_2->type = tmp->type;
            }

            cJSON *bdt;
            bdt = cJSON_GetObjectItem(bbmd, "BDT");
            if (bdt) {
                if (bdt->type != cJSON_Array) {
                    return "Invalid BDT item type";
                }

                cJSON *new_bdt = cJSON_CreateArray();
                if (new_bdt == NULL) {
                    return "Create new bdt failed";
                }

                cJSON *entry, *new_entry;
                struct in_addr in_addr;

                cJSON_ArrayForEach(entry, bdt) {
                    if (entry->type != cJSON_Object) {
                        cJSON_Delete(new_bdt);
                        return "Invalid bdt entry item type";
                    }

                    new_entry = cJSON_CreateObject();
                    if (new_entry == NULL) {
                        cJSON_Delete(new_bdt);
                        return "Create bdt entry failed";
                    }
                    cJSON_AddItemToArray(new_bdt, new_entry);
                    
                    tmp = cJSON_GetObjectItem(entry, "dst_bbmd");
                    if ((tmp == NULL) || (tmp->type != cJSON_String)) {
                        cJSON_Delete(new_bdt);
                        return "Invalid dst_bbmd item type";
                    }

                    if (inet_aton(tmp->valuestring, &in_addr) == 0) {
                        cJSON_Delete(new_bdt);
                        return TLT("Invalid bdt dst_bbmd address");
                    }
                    cJSON_AddStringToObject(new_entry, "dst_bbmd", tmp->valuestring);
                    
                    tmp = cJSON_GetObjectItem(entry, "dst_port");
                    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
                        cJSON_Delete(new_bdt);
                        return "Invalid dst_port item type";
                    }

                    if (tmp->valueint < 47808 || tmp->valueint >= 65535) {
                        cJSON_Delete(new_bdt);
                        return TLT("BDT udp_port should be in range of 47808~65534");
                    }
                    cJSON_AddNumberToObject(new_entry, "dst_port", tmp->valueint);

                    tmp = cJSON_GetObjectItem(entry, "bcast_mask");
                    if ((tmp == NULL) || (tmp->type != cJSON_String)) {
                        cJSON_Delete(new_bdt);
                        return "Invalid bcast_mask item type";
                    }

                    if ((inet_aton(tmp->valuestring, &in_addr) == 0)
                            || !check_broadcast_mask(in_addr)) {
                        cJSON_Delete(new_bdt);
                        return TLT("Invalid bdt bcast_mask address");
                    }
                    cJSON_AddStringToObject(new_entry, "bcast_mask", tmp->valuestring);
                }
                cJSON_ReplaceItemInObject(ori_bbmd, "BDT", new_bdt);
            }
        } else {
            cJSON_DeleteItemFromObject(port, "bbmd");
        }

        /* set fd_client item */
        cJSON *fd_client, *ori_fd_client;
        fd_client = cJSON_GetObjectItem(cfg, "fd_client");
        if (bbmd && fd_client) {
            return TLT("bbmd and fd_client items cannot be set at the same time");
        }
        
        if (fd_client) {
            if (fd_client->type != cJSON_Object) {
                return "Invalid fd_client item type";
            }

            ori_fd_client = cJSON_GetObjectItem(port, "fd_client");
            if ((ori_fd_client != NULL) && (ori_fd_client->type != cJSON_Object)) {
                return "Invalid original fd_client item type";
            }
            if (ori_fd_client == NULL) {
                ori_fd_client = cJSON_CreateObject();
                cJSON_AddStringToObject(ori_fd_client, "dst_bbmd", "0.0.0.0");
                cJSON_AddNumberToObject(ori_fd_client, "dst_port", 47808);
                cJSON_AddNumberToObject(ori_fd_client, "ttl", 120);
                cJSON_AddNumberToObject(ori_fd_client, "register_interval", 30);
                cJSON_AddItemToObject(port, "fd_client", ori_fd_client);
            }

            tmp = cJSON_GetObjectItem(fd_client, "dst_bbmd");
            if ((tmp == NULL) || (tmp->type != cJSON_String)) {
                return "Invalid fd_client dst_bbmd item type";
            }

            struct in_addr in_addr;
            if (inet_aton(tmp->valuestring, &in_addr) == 0) {
                return TLT("Invalid fd_client dst_bbmd address");
            }

            tmp_2 = cJSON_GetObjectItem(ori_fd_client, "dst_bbmd");
            if ((tmp_2 == NULL) || (tmp_2->type != cJSON_String)) {
                return "Invalid original fd_client dst_bbmd item type";
            }
            if (tmp_2->valuestring) {
                free(tmp_2->valuestring);
            }
            tmp_2->valuestring = strdup(tmp->valuestring);
            
            tmp = cJSON_GetObjectItem(fd_client, "dst_port");
            if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
                return "Invalid fd_client dst_port item type";
            }

            if ((tmp->valueint < 47808) || (tmp->valueint >= 65535)) {
                return TLT("FD remote udp_port should be in range of 47808~65534");
            }

            tmp_2 = cJSON_GetObjectItem(ori_fd_client, "dst_port");
            if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Number)) {
                return "Invalid original fd_client dst_port item type";
            }
            tmp_2->valuedouble = tmp->valueint;
            tmp_2->valueint = tmp->valueint;

            tmp = cJSON_GetObjectItem(fd_client, "ttl");
            if (tmp) {
                if (tmp->type != cJSON_Number) {
                    return "Invalid fd_client ttl item type";
                }

                tmp_2 = cJSON_GetObjectItem(ori_fd_client, "ttl");
                if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Number)) {
                    return "Invalid original fd_client ttl item type";
                }
                tmp_2->valuedouble = tmp->valueint;
                tmp_2->valueint = tmp->valueint;
            }
            
            tmp = cJSON_GetObjectItem(fd_client, "register_interval");
            if (tmp) {
                if (tmp->type != cJSON_Number) {
                    return "Invalid fd_client register_interval item type";
                }

                tmp_2 = cJSON_GetObjectItem(ori_fd_client, "register_interval");
                if ((tmp_2 == NULL) || (tmp_2->type != cJSON_Number)) {
                    return "Invalid original fd_client register_interval item type";
                }
                tmp_2->valueint = tmp->valueint;
                tmp_2->valuedouble = tmp->valueint;
            }
        } else {
            cJSON_DeleteItemFromObject(port, "fd_client");
        }
    } else if (strcmp(dl_type, "ETH") == 0) {
        /* do nothing */
    } else {
        return "Unknown dl_type";
    }
    
    if (enable) {
        ori_enable->type = cJSON_True;
    }
    
    return NULL;
}

static void conf_network_set_port(const struct mg_request_info *ri, struct mg_connection *conn,
                const char *locale)
{
    cJSON *cfg, *port, *tmp;
    char post_data[8192];
    int post_data_len;
    const char *msg;
    
    if (strcmp(ri->request_method, "POST")) {
        errResp(conn, "This entity only support POST method");
        return;
    }
    
    post_data_len = mg_read(conn, post_data, sizeof(post_data));
    if (post_data_len == sizeof(post_data)) {
        errResp(conn, "Too long post");
        return;
    }
    post_data[post_data_len] = 0;

    cfg = cJSON_Parse(post_data);
    if (cfg == NULL) {
        errResp(conn, "Invalid json string");
        return;
    }

    tmp = cJSON_GetObjectItem(cfg, "port_id");
    if ((tmp == NULL) || (tmp->type != cJSON_Number)) {
        errResp(conn, "Invalid port_id item type");
        goto out0;
    }

    if (tmp->valueint < 0) {
        errResp(conn, "Invalid port_id");
        goto out0;
    }

    RWLOCK_WRLOCK(&network_cfg_rwlock);
    
    port = cJSON_GetArrayItem(port_array, tmp->valueint);
    if ((port == NULL) || (port->type != cJSON_Object)) {
        RWLOCK_UNLOCK(&network_cfg_rwlock);
        errResp(conn, TLT("The port_id is not present"));
        goto out0;
    }

    msg = __conf_network_set_port(cfg, port, locale);
    netcfg_modify = true;
    
    RWLOCK_UNLOCK(&network_cfg_rwlock);
    
    if (msg) {
        jsonErrResp(conn, msg);
    } else {
        jsonErrResp(conn, NULL);
    }
    
out0:
    cJSON_Delete(cfg);

    return;
}

static cJSON *__conf_network_get_port_mib(uint32_t port_id)
{
    connect_client_state state;
    cJSON *cfg, *result;
    char *data;
    uint32_t data_len;
    uint8_t *rsp;
    uint32_t rsp_len;

    cfg = cJSON_CreateObject();
    if (cfg == NULL) {
        printf("%s: create object failed\r\n", __func__);
        return NULL;
    }

    cJSON_AddStringToObject(cfg, "method", WEB_GET_PORT_MIB);
    cJSON_AddNumberToObject(cfg, "port_id", port_id);

    data = cJSON_Print(cfg);
    cJSON_Delete(cfg);
    if (data == NULL) {
        printf("%s: cJSON Print failed\r\n", __func__);
        return NULL;
    }
    
    data_len = strlen(data) + 1;
    rsp = NULL;
    rsp_len = 0;
    
    state = connect_client(BACNET_WEB_SERVICE, (uint8_t *)data, data_len, &rsp, &rsp_len, 50);
    free(data);
    if (state != CONNECT_CLIENT_OK) {
        printf("%s: send request failed(%d)\r\n", __func__, state);
        return NULL;
    }

    if ((rsp == NULL) || (rsp_len == 0)) {
        printf("%s: no rsp\r\n", __func__);
        return NULL;
    }

    cfg = cJSON_Parse((char *)rsp);
    free(rsp);
    if (cfg == NULL) {
        printf("%s: cJSON Parse failed\r\n", __func__);
        return NULL;
    }

    result = cJSON_DetachItemFromObject(cfg, "result");
    if (result == NULL) {
        printf("%s: get port_id(%d) result item failed\r\n", __func__, port_id);
        cJSON_Delete(cfg);
        return NULL;
    }

    if (result->type != cJSON_Object) {
        printf("%s: invalid port_id(%d) result item type\r\n", __func__, port_id);
        cJSON_Delete(result);
        cJSON_Delete(cfg);
        return NULL;
    }

    cJSON *tmp = cJSON_GetObjectItem(result, "error_code");
    if (tmp != NULL) {
        cJSON *reason = cJSON_GetObjectItem(result, "reason");
        if (tmp->type == cJSON_Number && reason != NULL
                && reason->type == cJSON_String)
            printf("%s: error code(%d) reason(%s)\r\n", __func__,
                    tmp->valueint, reason->valuestring);
        else
            printf("%s: unknown error\r\n", __func__);

        cJSON_Delete(result);
        cJSON_Delete(cfg);
        return NULL;
    }

    cJSON_Delete(cfg);

    return result;
}

static int conf_network_transform_port_id(cJSON *port, uint32_t port_id)
{
    cJSON *tmp;
    int i, port_counts;

    tmp = cJSON_GetObjectItem(port, "enable");
    if ((tmp == NULL) || (tmp->type == cJSON_False)) {
        return -EPERM;
    }

    port_counts = 0;
    for (i = 0; i < port_id; i++) {
        tmp = cJSON_GetArrayItem(port_array, i);
        if (tmp == NULL) {
            break;
        }

        tmp = cJSON_GetObjectItem(tmp, "enable");
        if (tmp && (tmp->type == cJSON_True)) {
            port_counts++;
        }
    }

    return port_counts;
}

static void conf_network_get_port(const struct mg_request_info *ri, struct mg_connection *conn,
                const char *locale)
{
    char para_str[16];
    char *endptr;
    long int para;
    cJSON *port, *tmp;
    uint32_t port_id;
    int real_port;
    bool get_mib;

    if (strcmp(ri->request_method, "GET")) {
        errResp(conn, "This entity only support GET method");
        return;
    }

    if ((ri->query_string == NULL)
            || (mg_get_var(ri->query_string, strlen(ri->query_string), "port_id", para_str,
                sizeof(para_str)) <= 0)) {
        errResp(conn, "Invalid port_id");
        return;
    }
    
    para = strtol(para_str, &endptr, 10);
    if (*endptr || para < 0) {
        errResp(conn, "Invalid port_id");
        return;
    }
    port_id = (uint32_t)para;

    RWLOCK_RDLOCK(&network_cfg_rwlock);
    
    tmp = cJSON_GetArrayItem(port_array, port_id);
    if ((tmp == NULL) || (tmp->type != cJSON_Object)) {
        RWLOCK_UNLOCK(&network_cfg_rwlock);
        jsonErrResp(conn, TLT("The port_id is not present"));
        return;
    }

    port = cJSON_Duplicate(tmp, 1);
    if (port == NULL) {
        RWLOCK_UNLOCK(&network_cfg_rwlock);
        errResp(conn, "Duplicate port failed");
        return;
    }

    get_mib = false;
    if (netcfg_modify == false) {
        real_port = conf_network_transform_port_id(port, port_id);
        if (real_port >= 0) {
            get_mib = true;
        }
    }

    RWLOCK_UNLOCK(&network_cfg_rwlock);

    if (get_mib == true) {
        tmp = __conf_network_get_port_mib(real_port);
        if (tmp == NULL) {
            errResp(conn, "get port mib failed");
            goto out;
        }
        cJSON_AddItemToObject(port, "mib", tmp);
    }

    jsonResp(conn, port);

out:
    cJSON_Delete(port);
    
    return;
}

static void conf_network_get_port_mib(const struct mg_request_info *ri, struct mg_connection *conn,
                const char *locale)
{
    char para_str[16];
    char *endptr;
    long int para;
    cJSON *tmp;
    uint32_t port_id;
    int real_port;
    
    if (strcmp(ri->request_method, "GET")) {
        errResp(conn, "This entity only support GET method");
        return;
    }

    if ((ri->query_string == NULL)
            || (mg_get_var(ri->query_string, strlen(ri->query_string), "port_id", para_str,
                sizeof(para_str)) <= 0)) {
        errResp(conn, "Invalid port_id");
        return;
    }
    
    para = strtol(para_str, &endptr, 10);
    if (*endptr || para < 0) {
        errResp(conn, "Invalid port_id");
        return;
    }
    port_id = (uint32_t)para;

    RWLOCK_RDLOCK(&network_cfg_rwlock);

    if (netcfg_modify) {
        RWLOCK_UNLOCK(&network_cfg_rwlock);
        jsonResp(conn, cJSON_CreateNull());
        return;
    }
    
    tmp = cJSON_GetArrayItem(port_array, port_id);
    if ((tmp == NULL) || (tmp->type != cJSON_Object)) {
        RWLOCK_UNLOCK(&network_cfg_rwlock);
        jsonErrResp(conn, "The port_id is not present");
        return;
    }

    real_port = conf_network_transform_port_id(tmp, port_id);
    if (real_port < 0) {
        RWLOCK_UNLOCK(&network_cfg_rwlock);
        jsonErrResp(conn, "The port_id is disabled");
        return;
    }
    
    RWLOCK_UNLOCK(&network_cfg_rwlock);

    tmp = __conf_network_get_port_mib(real_port);
    if (tmp == NULL) {
        errResp(conn, "get port mib failed");
        return;
    }
    
    jsonResp(conn, tmp);
    cJSON_Delete(tmp);

    return;
}

static void conf_network_get_all(const struct mg_request_info *ri, struct mg_connection *conn,
                const char *locale)
{
    cJSON *network_cfg, *child;
    cJSON *new_resource, *new_port_array, *port, *tmp;
    uint32_t port_count;
    
    if (strcmp(ri->request_method, "GET")) {
        errResp(conn, "This entity only support GET method");
        return;
    }

    network_cfg = cJSON_CreateObject();
    if (network_cfg == NULL) {
        errResp(conn, "Create Object failed");
        return;
    }

    RWLOCK_RDLOCK(&network_cfg_rwlock);
    new_port_array = cJSON_Duplicate(port_array, 1);
    RWLOCK_UNLOCK(&network_cfg_rwlock);
    
    if (new_port_array == NULL) {
        errResp(conn, "Duplicate port_array failed");
        cJSON_Delete(network_cfg);
        return;
    }

    if (netcfg_modify == false) {
        port_count = 0;
        cJSON_ArrayForEach(port, new_port_array) {
            tmp = cJSON_GetObjectItem(port, "enable");
            if (tmp && (tmp->type == cJSON_True)) {
                cJSON_AddItemToObject(port, "mib", __conf_network_get_port_mib(port_count));
                port_count++;
            }
        }
    }

    new_resource = cJSON_Duplicate(resource_cfg, 1);
    if (new_resource == NULL) {
        errResp(conn, "Duplicate resource_cfg failed");
        cJSON_Delete(new_port_array);
        cJSON_Delete(network_cfg);
        return;
    }
    
    child = new_resource->child;
    while (child) {
        cJSON_DeleteItemFromObject(child, "ifname");
        child = child->next;
    }

    cJSON_AddItemToObject(network_cfg, "resource", new_resource);
    cJSON_AddItemToObject(network_cfg, "port", new_port_array);

    jsonResp(conn, network_cfg);

    cJSON_Delete(network_cfg);

    return;
}

static cJSON *conf_network_get_port_array(cJSON *cfg)
{
    cJSON *old_port_array, *new_port, *tmp;
    const char *msg;
    int i;

    old_port_array = cJSON_GetObjectItem(cfg, "port");
    if ((old_port_array == NULL) || (old_port_array->type != cJSON_Array)) {
        printf("%s: get port item failed\r\n", __func__);
        return NULL;
    }

    port_array = cJSON_CreateArray();
    if (port_array == NULL) {
        printf("%s: create port array failed\r\n", __func__);
        return NULL;
    }
    
    i = 0;
    cJSON_ArrayForEach(tmp, old_port_array) {
        if (tmp->type != cJSON_Object) {
            printf("%s: invalid port array[%d] item type\r\n", __func__, i);
            goto out0;
        }

        new_port = cJSON_CreateObject();
        if (new_port == NULL) {
            printf("%s: create port item failed\r\n", __func__);
            goto out0;
        }
        
        msg = conf_network_init_port(tmp, new_port, NULL);
        if (msg) {
            printf("%s: init port array[%d] item failed cause %s\r\n", __func__, i, msg);
            goto out1;
        }

        msg = __conf_network_set_port(tmp, new_port, NULL);
        if (msg) {
            printf("%s: set port array[%d] item failed cause %s\r\n", __func__, i, msg);
            goto out1;
        }

        cJSON_AddItemToArray(port_array, new_port);
        i++;
    }

    return port_array;

out1:
    cJSON_Delete(new_port);

out0:
    cJSON_Delete(port_array);
    port_array = NULL;
    
    return NULL;
}

int saveNetCfg(void)
{
    cJSON *net_cfg, *ports;
    int rv;

    net_cfg = cJSON_CreateObject();
    if (net_cfg == NULL) {
        printf("%s: create object failed\r\n", __func__);
        return -EPERM;
    }

    RWLOCK_RDLOCK(&network_cfg_rwlock);
    ports = cJSON_Duplicate(port_array, 1);
    RWLOCK_UNLOCK(&network_cfg_rwlock);
    
    if (ports == NULL) {
        printf("%s: Duplicate port_cfg failed\r\n", __func__);
        cJSON_Delete(net_cfg);
        return -EPERM;
    }

    cJSON_AddItemToObject(net_cfg, "port", ports);

    rv = save_json_file(net_cfg, BACNET_NETWORK_CONFIG_FILE);
    cJSON_Delete(net_cfg);
    if (rv < 0) {
        printf("%s: save json file failed(%d)\r\n", __func__, rv);
    }

    return rv;
}

int initNetCfg(void)
{
    cJSON *network_cfg;
    
    if (pthread_rwlock_init(&network_cfg_rwlock, NULL)) {
        printf("%s: network_cfg_rwlock init failed\r\n", __func__);
        return -EPERM;
    }

    resource_cfg = bacnet_get_resource_cfg();
    if (resource_cfg == NULL) {
        printf("%s: load resource json failed\r\n", __func__);
        goto out0;
    }

    network_cfg = bacnet_get_network_cfg();
    if (network_cfg == NULL) {
        printf("%s: load network json failed\r\n", __func__);
        port_array = cJSON_CreateArray();
    } else {
        port_array = conf_network_get_port_array(network_cfg);
        cJSON_Delete(network_cfg);
        if (port_array == NULL) {
            port_array = cJSON_CreateArray();
        }
    }

    if (port_array == NULL) {
        printf("%s: create port array item failed\r\n", __func__);
        goto out1;
    }
    
    (void)reg_handler("add_port", conf_network_add_port);
    (void)reg_handler("delete_port", conf_network_delete_port);
    (void)reg_handler("set_port", conf_network_set_port);
    (void)reg_handler("get_port", conf_network_get_port);
    (void)reg_handler("get_port_mib", conf_network_get_port_mib);
    (void)reg_handler("get_all", conf_network_get_all);
    
    return OK;

out1:
    cJSON_Delete(resource_cfg);

out0:
    (void)pthread_rwlock_destroy(&network_cfg_rwlock);
    
    return -EPERM;
}

