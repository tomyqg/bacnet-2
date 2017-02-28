/*
 * uciutils.c
 *
 *  Created on: Apr 30, 2016
 *      Author: lin
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <misc/utils.h>
#include <misc/uciutils.h>

bool uci_has(void)
{
    bool has;
    const char *args[] = {"sh", "-c",
            "which uci||return 0", NULL};
    char *result = get_run_result(args);

    if (result && result[0]) {
        has = true;
    } else {
        has = false;
    }
    
    if (result) {
        free(result);
    }
    
    return has;
}

static struct in_addr getIP(const char *cmdstr[])
{
    struct in_addr sin_addr;

    char *result = get_run_result(cmdstr);
    if (!result) {
        sin_addr.s_addr = 0;
        return sin_addr;
    }
    
    char *endline = strchr(result, '\n');
    if (endline) {
        *endline = 0;
    }
    
    if (inet_aton(result, &sin_addr) == 0) {
        sin_addr.s_addr = 0;
    }
    
    free(result);
    
    return sin_addr;
}

bool uci_loadIP(struct in_addr *ip, struct in_addr *mask, struct in_addr *router)
{
    if (ip) {
        const char *args[] = {"uci", "get", "network.lan.ipaddr", NULL};
        *ip = getIP(args);
    }

    if (mask) {
        const char *args[] = {"uci", "get", "network.lan.netmask", NULL};
        *mask = getIP(args);
    }

    if (router) {
        const char *args[] = {"sh", "-c",
                "uci -q get network.lan.gateway||return 0", NULL};
        *router = getIP(args);
    }

    {
        const char *args[] = {"sh", "-c",
                "ps|grep '[d]nsmasq'||return 0", NULL};
        char *result = get_run_result(args);
        bool dhcp;
        
        if (result && result[0]) {
            dhcp = true;
        } else {
            dhcp = false;
        }
        
        if (result) {
            free(result);
        }
        
        return dhcp;
    }
}

void uci_saveIP(struct in_addr ip, struct in_addr mask,
        struct in_addr router, bool dhcp)
{
    char *result;
    char buf[256];

    result = inet_ntoa(ip);
    {
        sprintf(buf, "network.lan.ipaddr=%s", result);
        const char* args[] = {"uci", "set", buf, NULL };
        pid_waitexit(exec_getpid(args, NULL, NULL, NULL), NULL);
    }

    result = inet_ntoa(mask);
    {
        sprintf(buf, "network.lan.netmask=%s", result);
        const char* args[] = {"uci", "set", buf, NULL};
        pid_waitexit(exec_getpid(args, NULL, NULL, NULL), NULL);
    }

    result = inet_ntoa(router);
    {
        sprintf(buf, "network.lan.gateway=%s", result);
        const char *args[] = {"uci", "set", buf, NULL};
        pid_waitexit(exec_getpid(args, NULL, NULL, NULL), NULL);
    }

    {
        const char *args[] = {"uci", "commit", NULL};
        pid_waitexit(exec_getpid(args, NULL, NULL, NULL), NULL);
    }

    {
        const char *args1[] = {"/etc/init.d/dnsmasq",
                dhcp ? "enable" : "disable", NULL };
        pid_waitexit(exec_getpid(args1, NULL, NULL, NULL), NULL);
    }
}

