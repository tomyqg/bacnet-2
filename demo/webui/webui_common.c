/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * webui_common.c
 * Original Author:  lincheng, 2016-5-17
 *
 * WEBUI COMMON
 *
 * History
 */

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include "webui.h"
#include "misc/rbtree.h"

typedef struct app_entry_s {
    struct rb_node node;
    char *name;
    web_handler handler;
} app_entry_t;

static pthread_mutex_t web_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct rb_root app_root = {.rb_node = NULL};

static app_entry_t *_find_app(const char *name)
{
    struct rb_node *node;
    app_entry_t *app;
    int rv;

    node = app_root.rb_node;
    while (node) {
        app = container_of(node, app_entry_t, node);
        rv = strcmp(name, app->name);
        if (rv > 0) {
            node = node->rb_right;
        } else if (rv < 0) {
            node = node->rb_left;
        } else {
            return app;
        }
    }

    return NULL;
}

web_handler find_handler(const char *name)
{
    app_entry_t *app;
    web_handler handler;
    
    if (!name) {
        printf("%s: null argument\r\n", __func__);
        return NULL;
    }

    pthread_mutex_lock(&web_mutex);

    app = _find_app(name);
    if (!app) {
        pthread_mutex_unlock(&web_mutex);
        return NULL;
    }

    handler = app->handler;

    pthread_mutex_unlock(&web_mutex);
	
    return handler;
}

int reg_handler(const char *name, web_handler handler)
{
    struct rb_node **where;
    struct rb_node *parent;
    app_entry_t *app;
    int rv;
    
    if (!name || !handler) {
        printf("%s: null arguments\r\n", __func__);
        return -EINVAL;
    }

    pthread_mutex_lock(&web_mutex);

    where = &app_root.rb_node;
    parent = NULL;
    
    /* Figure out where to put new node */
    while (*where) {
        app = container_of(*where, app_entry_t, node);
        rv = strcmp(name, app->name);
        parent = *where;
        if (rv < 0) {
            where = &((*where)->rb_left);
        } else if (rv > 0) {
            where = &((*where)->rb_right);
        } else {    /* find same device id, copy to it */
            pthread_mutex_unlock(&web_mutex);
            printf("%s: duplicate app entry(%s)\r\n", __func__, name);
            return -EPERM;
        }
    }

    app = (app_entry_t *)malloc(sizeof(app_entry_t));
    if (!app) {
        pthread_mutex_unlock(&web_mutex);
        printf("%s: not enough memory\r\n", __func__);
        return -EPERM;
    }

    app->name = strdup(name);
    app->handler = handler;

    /* Add new node and rebalance tree. */
    rb_link_node(&app->node, parent, where);
    rb_insert_color(&app->node, &app_root);

    pthread_mutex_unlock(&web_mutex);
	
    return 0;
}

int unreg_handler(const char *name)
{
    app_entry_t *app;
    
    if (!name) {
        printf("%s: null arguments\r\n", __func__);
        return -EINVAL;
    }

    pthread_mutex_lock(&web_mutex);

    app = _find_app(name);
    if (!app) {
        pthread_mutex_unlock(&web_mutex);
        printf("%s: no app %s registered\r\n", __func__, name);
        return -EPERM;
    }

    rb_erase(&app->node, &app_root);
    free(app->name);
    free(app);

    pthread_mutex_unlock(&web_mutex);
	
    return 0;
}

void errResp(struct mg_connection *conn, const char *msg)
{
    if ((conn == NULL) || (msg == NULL)) {
        return;
    }

    size_t len = strlen(msg);

	mg_printf(conn, "HTTP/1.1 500 Error\r\n"
			"Content-Length: %d\r\n\r\n",
			len);
	mg_write(conn, msg, len);
}

static void headerResp(struct mg_connection *conn, const char *content_type,
        size_t len)
{
    mg_printf(conn, "HTTP/1.1 200 OK\r\n"
            "Cache-Control: no-cache, no-store, must-revalidate\r\n"
            "Pragma: no-cache\r\n"
            "Expires: 0\r\n"
            "Content-Type: %s; charset=utf-8\r\n"
            "Content-Length: %d\r\n\r\n",
            content_type, len);
}

void norResp(struct mg_connection *conn, const char *msg)
{
    if ((conn == NULL) || (msg == NULL)) {
        return;
    }

    size_t len = strlen(msg);
    headerResp(conn, "text/html", len);
    mg_write(conn, msg, len);
}

void chunkedHeaderResp(struct mg_connection *conn)
{
    if (conn == NULL) {
        return;
    }

    mg_printf(conn, "HTTP/1.1 200 OK\r\n"
            "Cache-Control: no-cache, no-store, must-revalidate\r\n"
            "Pragma: no-cache\r\n"
            "Expires: 0\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Transfer-Encoding: chunked\r\n\r\n");
}

void chunkedBody(struct mg_connection *conn, const char *msg, size_t len)
{
    if ((conn == NULL) || (msg == NULL && len != 0)) {
        return;
    }

    mg_printf(conn, "%x\r\n", len);
    mg_write(conn, msg, len);
    mg_write(conn, "\r\n", 2);
}

void jsonResp(struct mg_connection *conn, cJSON *json)
{
    if ((conn == NULL) || (json == NULL)) {
        return;
    }
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "Code", cJSON_CreateNumber(0));
    cJSON_AddItemReferenceToObject(root, "Result", json);

    char *jsonstr = cJSON_Print(root);
    cJSON_Delete(root);

    if (!jsonstr) {
        errResp(conn, "Internal error");
        return;
    }
	
    int size = strlen(jsonstr);
    headerResp(conn, "application/json", size);
    mg_write(conn, jsonstr, size);
    free(jsonstr);
}

void jsonErrResp(struct mg_connection *conn, const char *msg)
{
    static const char errformat[] = "{\"Code\":1,\"Message\":\"%s\"}";
    static const char okformat[] = "{\"Code\":0}";
    
    if (conn == NULL) {
        return;
    }
    
    if (msg) {
        headerResp(conn, "application/json", strlen(msg) + sizeof(errformat) - 3);
        mg_printf(conn, errformat, msg);
    } else {
        headerResp(conn, "application/json", sizeof(okformat) - 1);
        mg_write(conn, okformat, sizeof(okformat) - 1);
  	}
}

void htmlJsonErrResp(struct mg_connection *conn, const char *msg)
{
    static const char errformat[] = "{\"Code\":1,\"Message\":\"%s\"}";
    static const char okformat[] = "{\"Code\":0}";

    if (conn == NULL) {
        return;
    }

    if (msg) {
        headerResp(conn, "text/html", strlen(msg) + sizeof(errformat) - 3);
        mg_printf(conn, errformat, msg);
    } else {
        headerResp(conn, "text/html", sizeof(okformat) - 1);
        mg_write(conn, okformat, sizeof(okformat) - 1);
    }
}

void drop_left_request(struct mg_connection *conn)
{
    char buf[1024];
    while (mg_read(conn, buf, sizeof(buf)) > 0);
}

