/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * translate.c
 * Original Author:  lincheng, 2016-5-23
 *
 * History
 */

#include <string.h>
#include "translate.h"
#include <map>

struct cmp_str
{
   bool operator()(const char *a, const char *b) const
   {
      return strcmp(a, b) < 0;
   }
};

static std::map<const char*, std::map<const char*, const char*, cmp_str>, cmp_str> createDict()
{
    std::map<const char*, const char*, cmp_str> zhdict;

    std::map<const char*, std::map<const char*, const char*, cmp_str>, cmp_str> dict;
    dict["zh"] = zhdict;

    zhdict["Resource_name item conflict"] = "资源名冲突";
    zhdict["BDT udp_port should be in range of 47808~65534"] = "BDT中的udp端口号应在47808~65534之间";
    zhdict["The udp_port is already present"] = "udp端口已被占用";
    zhdict["Invalid baudrate"] = "无效的波特率";
    zhdict["Invalid max_info_frames"] = "无效的最大一次发包数";
    zhdict["Invalid this_station"] = "无效的本地MAC地址";
    zhdict["Invalid push_interval"] = "无效的BDT推送间隔";
    zhdict["bbmd and fd_client items cannot be set at the same time"] = "广播管理设备与外部设备不能同时启用";
    zhdict["FD remote udp_port should be in range of 47808~65534"] = "外部设备的远程udp端口号应在47808~65534之间";
    zhdict["The dl_type item conflict"] = "链路层类型冲突";
    zhdict["Invalid fd_client dst_bbmd address"] = "外部设备的远程IP地址无效";
    zhdict["The net_num is already present"] = "网络号已被占用";
    zhdict["The port_id is not present"] = "端口不存在";
    zhdict["The udp_port should be in range of 47808~65534"] = "udp端口应在47808~65534之间";
    zhdict["Invalid max_master"] = "无效的最大扫描站号";
    zhdict["Invalid bdt dst_bbmd address"] = "BDT中IP地址无效";
    zhdict["Invalid bdt bcast_mask address"] = "BDT中IP广播地址无效";
    zhdict["No firmware file submited"] = "没有提交固件";
    zhdict["Only one firmware file is allowed"] = "仅能提交一个固件文件";
    zhdict["Not firmware file"] = "提交的不是固件";
    zhdict["Data corrupted"] = "数据错误";
    zhdict["Invalid firmware"] = "无效的固件";
    zhdict["Operation not supported"] = "不支持该操作";
    return dict;
}

static const std::map<const char*, std::map<const char*, const char*, cmp_str>, cmp_str> dict = createDict();

const char *translate(const char *str, const char *locale)
{
    if (!locale || !str)
	return str;

    std::map<const char*, std::map<const char*, const char*, cmp_str>, cmp_str>::const_iterator it = dict.find(locale);
    if (it == dict.end())
	return str;

    std::map<const char*, const char*, cmp_str>::const_iterator dictit = (it->second).find(str);
    if (dictit == it->second.end())
	return str;

    return dictit->second;
}

