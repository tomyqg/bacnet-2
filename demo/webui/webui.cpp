#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <linux/watchdog.h>
#include <signal.h>
#include <string.h>
#include <poll.h>
#include <utility>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sysexits.h>
#include <zlib.h>

#include "bacnet/bacnet.h"
#include "bacnet/bip.h"
#include "misc/utils.h"
#include "misc/uciutils.h"
#include "misc/usbuartproxy.h"
#include "translate.h"
#include "webui.h"
#include "config_app.h"
#include "config_network.h"

static const char LOGIN_CONFIG_FILE[] = "login.conf";
static const char DEFAULT_USERNAME[] = "admin";
static const char DEFAULT_PASSWORD[] = "";
static const char DEFAULT_LOCAL_INTERFACE[] = "eth0";
static const char WEBUI_CONFIG_FILE[] = "webui.conf";

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static pid_t daemonPid = 0;
static bool busy = false;
static bool daemonHas = false;    // unable to startup daemon
static bool daemonFail = false;   // daemon startup failed

static bool hasuci;
bool endhcp;
struct in_addr ipaddr, netmask, gateway;

static const char *auth_domain = "";
static char *username = NULL;
static char *password = NULL;

static const char *serial = "";
static const char *version = "";
static const char *daemon_name = "";

#define MIN_LOG_SIZE        8192
#define MAX_LOG_SIZE        (1<<20)
#define MAX_LOG_LINE_LEN    256

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static int log_fd = -1;
static unsigned log_size = 16384;
static char *log_buf = NULL;
static unsigned log_head = 0, log_tail = 0;
static long long unsigned log_head_sn = 0, log_tail_sn = 0;

cJSON *getGlobalCfg(void)
{
    cJSON *cfg = cJSON_CreateObject();

    cJSON_AddStringToObject(cfg, "serial", serial);

    cJSON *logincfg = cJSON_CreateObject();
    cJSON_AddItemToObject(logincfg, "username", cJSON_CreateString(username));
    cJSON_AddItemToObject(logincfg, "password", cJSON_CreateString(password));
    cJSON_AddItemToObject(cfg, "login", logincfg);

    cJSON *ipcfg = cJSON_CreateObject();
    cJSON_AddStringToObject(ipcfg, "ipaddr", inet_ntoa(ipaddr));
    cJSON_AddStringToObject(ipcfg, "netmask", inet_ntoa(netmask));
    cJSON_AddStringToObject(ipcfg, "gateway", inet_ntoa(gateway));
    cJSON_AddItemToObject(ipcfg, "endhcp",
            endhcp ? cJSON_CreateTrue() : cJSON_CreateFalse());
    cJSON_AddItemToObject(cfg, "ip", ipcfg);

    cJSON_AddStringToObject(cfg, "version", version);
    return cfg;
}

void setGlobalCfg(cJSON *cfg)
{
    cJSON *value;
    value = cJSON_GetObjectItem(cfg, "login");
    if (value && value->type == cJSON_Object) {
        cJSON *subvalue = cJSON_DetachItemFromObject(value, "username");
        if (subvalue && subvalue->type == cJSON_String) {
            if (username) {
                free(username);
        	}
            username = strdup(subvalue->valuestring);
        } else {
            printf("%s: no username item found\r\n", __func__);
        }

        subvalue = cJSON_DetachItemFromObject(value, "password");
        if (subvalue && subvalue->type == cJSON_String) {
            if (password) {
                free(password);
         	}
            password = strdup(subvalue->valuestring);
        } else {
            printf("%s: no password item found\r\n", __func__);
        }
    } else {
        printf("%s: no login item found\r\n", __func__);
    }

    if (!hasuci) {
    	return;
 	}
	
    if ((value = cJSON_GetObjectItem(cfg, "ip")) && value->type == cJSON_Object) {
        cJSON *subvalue;
        struct in_addr sin_addr;
        if ((subvalue = cJSON_GetObjectItem(value, "ipaddr"))
                && subvalue->type == cJSON_String
                && inet_aton(subvalue->valuestring, &sin_addr)) {
            ipaddr = sin_addr;
        } else {
            printf("%s: Invalid ipaddr\n", __func__);
     	}
		
        if ((subvalue = cJSON_GetObjectItem(value, "netmask"))
                && subvalue->type == cJSON_String
                && inet_aton(subvalue->valuestring, &sin_addr)) {
            netmask = sin_addr;
        } else {
            printf("%s: Invalid netmask\n", __func__);
     	}
		
        if ((subvalue = cJSON_GetObjectItem(value, "gateway"))
                && subvalue->type == cJSON_String
                && inet_aton(subvalue->valuestring, &sin_addr)) {
            gateway = sin_addr;
        } else {
            printf("%s: Invalid gateway\n", __func__);
     	}
		
        if ((subvalue = cJSON_GetObjectItem(value, "endhcp"))
                && (subvalue->type == cJSON_True
                        || subvalue->type == cJSON_False)) {
            endhcp = subvalue->type == cJSON_True;
        } else {
            printf("%s: Invalid endhcp\n", __func__);
    	}
    } else {
        printf("%s: Invalid ip\n", __func__);
  	}
}

static void saveGlobalCfg(void)
{
    cJSON *cfg = getGlobalCfg();

    if (hasuci) {
        uci_saveIP(ipaddr, netmask, gateway, endhcp);
  	}
	
    /* save login separately */
    cJSON *logincfg = cJSON_GetObjectItem(cfg, "login");
    save_json_file(logincfg, LOGIN_CONFIG_FILE);
    cJSON_Delete(cfg);
}

static void saveCfg(void)
{
    pthread_mutex_lock(&mutex);
    while(busy) {
        pthread_cond_wait(&cond, &mutex);
 	}
    busy = true;
    pthread_mutex_unlock(&mutex);

    saveGlobalCfg();
    saveAppCfg();
    saveNetCfg();

    pthread_mutex_lock(&mutex);
    busy = false;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);
}

#define APP_PREFIX      "/app/"
static char *old_username;
static char htdigest[33];

static const void *eventcb(enum mg_event event, struct mg_connection *conn)
{
	const struct mg_request_info *ri;
	web_handler handler;
	char locale[16];
	
	if (event != MG_NEW_REQUEST) {
		return NULL;
	}

	if (authorizeRequest(conn, old_username, htdigest)) {
		return "";
	}

	ri = mg_get_request_info(conn);
	if (strncmp(ri->uri, APP_PREFIX, sizeof(APP_PREFIX) - 1)) {
		return NULL;
	}

	if (ri->query_string) {
        (void)mg_get_var(ri->query_string, strlen(ri->query_string), "locale", locale, sizeof(locale));
    } else {
        locale[0] = 0;
    }

	handler = find_handler(ri->uri + sizeof(APP_PREFIX) - 1);
	if (!handler) {
	    errResp(conn, "Unknown resource");
	    return "";
	}

	handler(ri, conn, locale);
	
	return "";
}

static void startMongoose(const char *port)
{
	const char *options[] = {
			"listening_ports", port,
			"document_root", "html",
			"num_threads", "10",
			"enable_directory_listing", "no",
			"authentication_domain", auth_domain,
			NULL};
	
	mg_start((mg_callback_t)&eventcb, NULL, options);
}

static void conf_handler(const struct mg_request_info *ri, struct mg_connection *conn,
				const char *locale)
{
    if (!strcmp(ri->request_method, "GET")) {
        cJSON *cfg;
        pthread_mutex_lock(&mutex);
        while (busy) pthread_cond_wait(&cond, &mutex);

        cfg = getGlobalCfg();
        pthread_mutex_unlock(&mutex);

        jsonResp(conn, cfg);
        cJSON_Delete(cfg);
    } else if (!strcmp(ri->request_method, "POST")) {
        char post_data[8192];
        int post_data_len;

        // Read POST data
        post_data_len = mg_read(conn, post_data, sizeof(post_data));
        if (post_data_len == sizeof(post_data)) {
            errResp(conn, "Too long post");
            return;
        }
        post_data[post_data_len] = 0;
        cJSON *cfg = cJSON_Parse(post_data);
        if (cfg == NULL) {
            errResp(conn, "Invalid json string");
            return;
        }

        pthread_mutex_lock(&mutex);
        while (busy) pthread_cond_wait(&cond, &mutex);

        setGlobalCfg(cfg);

        pthread_mutex_unlock(&mutex);
        cJSON_Delete(cfg);
        jsonErrResp(conn, NULL);
    } else {
        errResp(conn, "Unknown method");
    }
}

static void save_handler(const struct mg_request_info *ri, struct mg_connection *conn,
				const char *locale)
{
    if (strcmp(ri->request_method, "POST")) {
        errResp(conn, "This entity only support POST method");
        return;
    }
	
    saveCfg();
    jsonErrResp(conn, NULL);
}

static void reboot_handler(const struct mg_request_info *ri, struct mg_connection *conn,
				const char *locale)
{
    if (strcmp(ri->request_method, "POST")) {
        errResp(conn, "This entity only support POST method");
        return;
    }

    if (!hasuci) {
        jsonErrResp(conn, TLT("Operation not supported"));
        return;
    }
	
    const char *argsrb[] = {"reboot", NULL};
    pid_waitexit(exec_getpid(argsrb, NULL, NULL, NULL), NULL);
    jsonErrResp(conn, NULL);
}

struct fw_context {
    int file;
    uLong crc;
    uLong crc_temp;
};

static int upload_cb(void *context, char *fname, char *data, int len)
{
    fw_context *fw = (fw_context*)context;

    if (fname != NULL) {
        if (len != 0) // too many file
            return -EMFILE;

        float ver;
        char crchex[9];
        char crc[4];
        char ext[16];
        size_t name_len = strlen(fname);

        for (unsigned i = 0; i < name_len; ++i)
            if (fname[i] == '_')
                fname[i] = ' ';

        if (sscanf(fname, "%*99s %*99s %f %8s.%15s", &ver, crchex, ext) != 3) {
            return -EBADF; // 文件名不符
        }

        if (strcasecmp(ext, "tar.gz") != 0) {
            return -EBADF;
        }

        if (hexstr2bytes(crc, 4, crchex) != 4) {
            return -EBADF;
        }
        fw->crc = ((uint8_t)crc[0] << 24) | ((uint8_t)crc[1] << 16)
                | ((uint8_t)crc[2] << 8) | (uint8_t)crc[3];
        fw->crc_temp = 0;
        return 1;
    }

    if (data != NULL) {
        fw->crc_temp = crc32(fw->crc_temp, (uint8_t*)data, (unsigned)len);

        int written = 0;
        while(written < len) {
            int nwrite = write(fw->file, data + written, len - written);
            if (nwrite < 0) {
                if (errno == EINTR)
                    continue;

                return -errno;
            }
            written += nwrite;
        }
    } else if (fw->crc != fw->crc_temp) {
        return -EIO;
    }

    return 1;
}

static void fm_handler(const struct mg_request_info *ri, struct mg_connection *conn,
				const char *locale)
{
    int uploaded;
    int rv;
    int status;

    if (strcmp(ri->request_method, "POST")) {
        errResp(conn, "This entity only support POST method");
        return;
    }

    if (!hasuci) {
        drop_left_request(conn);
        htmlJsonErrResp(conn, TLT("Operation not supported"));
        return;
    }

    const char *argsrm[] = {"rm", "-rf", "/tmp/fm/", NULL};
    pid_waitexit(exec_getpid(argsrm, NULL, NULL, NULL), NULL);

    const char *argsmk[] = {"mkdir", "/tmp/fm", NULL};
    pid_waitexit(exec_getpid(argsmk, NULL, NULL, NULL), NULL);

    pid_t child;
    int output;
    {
        const char *args[] = {"tar", "-xz", "-C", "/tmp/fm", NULL};
        child = exec_getpid(args, &output, NULL, NULL);
        if (child <= 0) {
            errResp(conn, "Internal server error");
            return;
        }
    }

    fw_context context;
    context.file = output;
    uploaded = mg_upload(conn, upload_cb, (void*)&context);
    close(output);
    rv = pid_waitexit(child, &status);

    if (uploaded <= 0) {
        // delete /tmp/fm
        pid_waitexit(exec_getpid(argsrm, NULL, NULL, NULL), NULL);
        if (uploaded == 0) {
            htmlJsonErrResp(conn, TLT("No firmware file submited"));
        } else if (uploaded == -EMFILE) {
            htmlJsonErrResp(conn, TLT("Only one firmware file is allowed"));
        } else if (uploaded == -EBADF) {
            htmlJsonErrResp(conn, TLT("Not firmware file"));
        } else if (uploaded == -EIO) {
            htmlJsonErrResp(conn, TLT("Data corrupted"));
        } else {
            htmlJsonErrResp(conn, TLT("Invalid firmware"));
        }
        return;
    }

    if (rv < 0 || !WIFEXITED(status) || WEXITSTATUS(status)){
        pid_waitexit(exec_getpid(argsrm, NULL, NULL, NULL), NULL); // delete /tmp/*
        htmlJsonErrResp(conn, TLT("Invalid firmware"));
        return;
    }

    htmlJsonErrResp(conn, NULL);

    const char *argsup[] = {"/tmp/fm/upgrade", NULL};
    pid_waitexit(exec_getpid(argsup, NULL, NULL, NULL), NULL);
    //never go below
    pid_waitexit(exec_getpid(argsrm, NULL, NULL, NULL), NULL); // delete /tmp/*
    return;
}

static void log_handler(const struct mg_request_info *ri, struct mg_connection *conn,
                const char *locale)
{
    unsigned long long sn;
    unsigned done;
    char tmp_buf[4096];
    size_t line_len;

    if (strcmp(ri->request_method, "GET")) {
        errResp(conn, "This entity only support GET method");
        return;
    }

    chunkedHeaderResp(conn);
    pthread_mutex_lock(&log_mutex);
    done = log_tail;
    sn = log_tail_sn;
    line_len = 0;

    while (done != log_head
            && (sn + line_len) >= log_tail_sn && (sn + line_len) < log_head_sn) {
        for (;;) {
            char c = log_buf[done];
            tmp_buf[line_len++] = c;
            done = (done + 1) & (log_size - 1);
            if (c == '\n')
                break;
        }
        if (sizeof(tmp_buf) - line_len >= MAX_LOG_LINE_LEN)
            continue;

        sn += line_len;
        pthread_mutex_unlock(&log_mutex);

        chunkedBody(conn, tmp_buf, line_len);
        line_len = 0;
        pthread_mutex_lock(&log_mutex);
    }

    pthread_mutex_unlock(&log_mutex);
    if (line_len) {
        chunkedBody(conn, tmp_buf, line_len);
    }

    chunkedBody(conn, tmp_buf, 0);
    return;
}

static void initGlobalCfg(void)
{
    hasuci = uci_has();

    username = strdup(DEFAULT_USERNAME);
    password = strdup(DEFAULT_PASSWORD);

    cJSON *usb_info = usb_serial_query();
    if (usb_info != NULL && usb_info->type == cJSON_Array
            && cJSON_GetArraySize(usb_info) > 0) {
        cJSON *snjson = cJSON_GetObjectItem(cJSON_GetArrayItem(usb_info, 0), "serial");
        if (snjson != NULL && snjson->type == cJSON_String)
            serial = strdup(snjson->valuestring);
    }

    if (usb_info) {
        cJSON_Delete(usb_info);
        usb_info = NULL;
    }

    cJSON *webuicfg = load_json_file(WEBUI_CONFIG_FILE);
    if (webuicfg && webuicfg->type == cJSON_Object) {
        cJSON *versioncfg = cJSON_GetObjectItem(webuicfg, "version");
        if (versioncfg && versioncfg->type == cJSON_String) {
            version = strdup(versioncfg->valuestring);
        } else {
            printf("%s: %s: invalid version\r\n", __func__, WEBUI_CONFIG_FILE);
        }

        cJSON *daemoncfg = cJSON_GetObjectItem(webuicfg, "daemon");
        if (daemoncfg && daemoncfg->type == cJSON_String) {
            daemon_name = strdup(daemoncfg->valuestring);
        } else {
            printf("%s: %s: invalid daemon\r\n", __func__, WEBUI_CONFIG_FILE);
        }

        cJSON *logcfg = cJSON_GetObjectItem(webuicfg, "log_size");
        if (logcfg && logcfg->type == cJSON_Number) {
            log_size = (unsigned)logcfg->valueint;
            if (log_size < MIN_LOG_SIZE)
                log_size = MIN_LOG_SIZE;
            else if (log_size > MAX_LOG_SIZE)
                log_size = MAX_LOG_SIZE;

            for (unsigned i = 0x80000000; ; i >>= 1) {
                if (log_size >= i) {
                    log_size = i;
                    break;
                }
            }
        } else {
            printf("%s: %s: invalid log_size\r\n", __func__, WEBUI_CONFIG_FILE);
        }

        cJSON *domaincfg = cJSON_GetObjectItem(webuicfg, "auth_domain");
        if (domaincfg && domaincfg->type == cJSON_String) {
            auth_domain = strdup(domaincfg->valuestring);
        } else {
            printf("%s: %s: invalid auth_domain\r\n", __func__, WEBUI_CONFIG_FILE);
        }
        cJSON_Delete(webuicfg);
    } else if (webuicfg) {
        printf("%s: invalid %s\r\n", __func__, WEBUI_CONFIG_FILE);
        cJSON_Delete(webuicfg);
    }
    webuicfg = NULL;

    cJSON *logincfg = load_json_file(LOGIN_CONFIG_FILE);
    if (logincfg && logincfg->type == cJSON_Object) {
        cJSON *value = cJSON_GetObjectItem(logincfg, "username");
        if (value && value->type == cJSON_String) {
            free(username);
            username = strdup(value->valuestring);
        } else {
            printf("%s: no username item found\r\n", __func__);
        }

        value = cJSON_GetObjectItem(logincfg, "password");
        if (value && value->type == cJSON_String) {
            free(password);
            password = strdup(value->valuestring);
        } else {
            printf("%s: no password item found\r\n", __func__);
        }

        cJSON_Delete(logincfg);
    } else if (logincfg) {
        printf("%s: invalid %s\r\n", __func__, LOGIN_CONFIG_FILE);
        cJSON_Delete(logincfg);
    }
    logincfg = NULL;

    if (hasuci) {
        endhcp = uci_loadIP(&ipaddr, &netmask, &gateway);
    } else {
        int rv = bip_get_interface_info(DEFAULT_LOCAL_INTERFACE, &ipaddr, &netmask);
        if (rv < 0) {
            ipaddr.s_addr = 0;
            netmask.s_addr = 0;
            printf("%s: get %s interface info failed\n", __func__, DEFAULT_LOCAL_INTERFACE);
        }
    }

    (void)reg_handler("config", conf_handler);
    (void)reg_handler("save", save_handler);
    (void)reg_handler("reboot", reboot_handler);
    (void)reg_handler("fm", fm_handler);
    (void)reg_handler("log", log_handler);
}

static void initCfg(void)
{
   	initGlobalCfg();
  	(void)initAppCfg();
  	(void)initNetCfg();
}

#define LED_SHOT_INTERVAL       3
static const char LED_TRIGGER[] = "/sys/devices/platform/gpio-leds/leds/zbt-wa05:blue:status/trigger";
static const char LED_SHOT[] = "/sys/devices/platform/gpio-leds/leds/zbt-wa05:blue:status/shot";

static void term_handler(int signo)
{
    if (daemonPid > 0) {
        kill(daemonPid, signo);
    }

    exit(0);
}

static void push_log(char *buf, size_t len)
{
    pthread_mutex_lock(&log_mutex);

    unsigned left = (log_tail - log_head - 1) & (log_size - 1);
    while (left < len) {
        for (unsigned tail = log_tail; ;) {
            char c = log_buf[tail];
            tail = (tail + 1) & (log_size - 1);

            if (c == '\n') {
                unsigned drop = (tail - log_tail) & (log_size - 1);
                log_tail = tail;
                log_tail_sn += drop;
                left += drop;
                break;
            }
        }
    }

    unsigned first = log_size - log_head;
    if (first > len) {
        memcpy(log_buf + log_head, buf, len);
        log_head += len;
    } else {
        memcpy(log_buf + log_head, buf, first);
        memcpy(log_buf, buf + first, len - first);
        log_head = len - first;
    }
    log_head_sn += len;

    pthread_mutex_unlock(&log_mutex);
}

static void read_log(void)
{
    static char tmp_buf[MAX_LOG_LINE_LEN];
    static size_t line_len = 0;
    static bool discard = false;

    for (;;) {
        int rv = read(log_fd, tmp_buf + line_len, sizeof(tmp_buf) - line_len);
        if (rv < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EWOULDBLOCK)
                return;
        } else if (rv == 0) {
            return; // closed
        }

        rv += line_len;
        do {
            if (tmp_buf[line_len++] == '\n') {
                if (!discard)
                    push_log(tmp_buf, line_len);

                memmove(tmp_buf, tmp_buf + line_len, rv - line_len);
                rv -= line_len;
                line_len = 0;
                discard = false;
            }
        } while (line_len < (size_t)rv);

        if (line_len == sizeof(tmp_buf)) {
            if (!discard) {
                tmp_buf[line_len - 1] = '\n';
                push_log(tmp_buf, line_len);
            }
            line_len = 0;
            discard = true;
        }
    }
}

int main(int argc, char *argv[])
{
	int status;
	int rv;

    signal(SIGPIPE, SIG_IGN);

    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = term_handler;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    initCfg();

    old_username = strdup(username);
    mg_md5(htdigest, old_username, ":", auth_domain, ":", password, NULL);

    /* 命令行可指定http端口号，方便调试 */
    if (argc < 2)
        startMongoose("80");
    else
        startMongoose(argv[1]);

    log_buf = (char*)malloc(log_size);

    if (strlen(daemon_name) == 0) {
        daemonHas = false;
        for (;;) sleep(1);
    }

    if (hasuci) {
        int trigger_fd = open(LED_TRIGGER, O_RDWR);
        if (trigger_fd == -1) {
            printf("%s: open led trigger failed(%s).\n", __func__,
                    strerror(errno));
            return 1;
        }

        int nwrite = write(trigger_fd, "oneshot", sizeof("oneshot"));
        if (nwrite < 0) {
            printf("%s: write to led trigger failed(%s).\n", __func__,
                    strerror(errno));
            return 1;
        }
        close(trigger_fd);
    }

    char *execarg[2];
    execarg[0] = (char*)daemon_name;
    execarg[1] = NULL;

    pthread_mutex_lock(&mutex);
    for (;;) {
        while (busy) pthread_cond_wait(&cond, &mutex);

        daemonPid = exec_getpid(execarg, NULL, &log_fd, NULL);
        if (daemonPid <= 0) {
            continue;
        } else {
            int flags = fcntl(log_fd, F_GETFL, 0);
            fcntl(log_fd, F_SETFL, flags | O_NONBLOCK);
        }
        pthread_mutex_unlock(&mutex);

        struct timespec led_ts;
        clock_gettime(CLOCK_MONOTONIC_COARSE, &led_ts);

        for (;;) {
            struct timespec now;
            sched_yield();
            read_log();
            rv = waitpid(daemonPid, &status, WNOHANG);
            if (rv > 0) {
                close(log_fd);
                log_fd = -1;
                break;
            }

            if (rv < 0) {
                printf("%s: waitpid(%d) failed(%s)\n", __func__,
                        daemonPid, strerror(errno));
            } else if (hasuci && clock_gettime(CLOCK_MONOTONIC_COARSE, &now) == 0
                    && now.tv_sec - led_ts.tv_sec >= LED_SHOT_INTERVAL) {
                led_ts.tv_sec = now.tv_sec;
                int shot_fd = open(LED_SHOT, O_WRONLY);
                if (shot_fd == -1) {
                    printf("%s: open shot file failed(%s)\n", __func__,
                            strerror(errno));
                } else {
                    if (write(shot_fd, "1", 1) < 0) {
                        printf("%s: write shot file failed(%s)\n", __func__,
                                strerror(errno));
                    }
                    close(shot_fd);
                }
            }
        }

        pthread_mutex_lock(&mutex);
        daemonPid = 0;
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) == EX_CONFIG) {		// if invalid cfg file
                daemonFail = true;
                // wait until user reset daemonFail
                do {
                    pthread_cond_wait(&cond, &mutex);
                } while (daemonFail == true);
            } else { // other reason, fetal
                daemonHas = false;
                pthread_mutex_unlock(&mutex);
                for (;;) sleep(1);
            }
        }
    }

    return 0;
}
