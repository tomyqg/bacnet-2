/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * connect_mng.c
 * Original Author:  linzhixian, 2015-9-8
 *
 * Connect Manager
 *
 * History
 */

#include <unistd.h>
#include <stdlib.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

#include "connect_mng_def.h"
#include "bacnet/bacint.h"
#include "debug.h"

static connect_service_handler service_handler_array[MAX_BACNET_SERVICE_TYPE];

static el_watch_t *listen_watcher = NULL;

static sockfd_list_t sockfd_list;

static bool connect_mng_status = false;

bool connet_mng_dbg_err = true;
bool connet_mng_dbg_warn = true;
bool connet_mng_dbg_verbos = true;

static int socket_nonblock_set(int sock_fd)
{
    int flags;

    flags = fcntl(sock_fd, F_GETFL, 0);
    if (flags < 0) {
        CONNECT_MNG_ERROR("%s: F_GETFL failed(%d)\r\n", __func__, flags);
        return -EPERM;
    }

    flags |= O_NONBLOCK;
    if (fcntl(sock_fd, F_SETFL, flags) < 0) {
        CONNECT_MNG_ERROR("%s: F_SETFL failed\r\n", __func__);
        return -EPERM;
    }

    return OK;
}

static void socket_connfd_status_reset(connect_info_impl_t *conn)
{
    if (conn->base.data) {
        free(conn->base.data);
        conn->base.data = NULL;
        conn->base.data_len = 0;
    }
    conn->bytes_done = 0;
}

static void socket_read_handler(el_watch_t *watch, int events)
{
    connect_service_handler handler;
    connect_info_impl_t *conn;
    int nread;
    
    if (!watch || !watch->data) {
        CONNECT_MNG_ERROR("%s: invalid argument\r\n", __func__);
        return;
    }

    conn = (connect_info_impl_t *)watch->data;

    CONNECT_MNG_VERBOS("%s: read events(%d) on fd(%d)\r\n", __func__, events, conn->fd);

    if (!(events & EPOLLIN)) {
        CONNECT_MNG_ERROR("%s: no EPOLLIN event\r\n", __func__);
        return;
    }

    if (conn->bytes_done < 8) {
        for (;;) {
            nread = read(conn->fd, ((uint8_t*)&conn->base.type) + conn->bytes_done,
                8 - conn->bytes_done);
            if (nread < 0) {
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN) {
                    return;
                }

                CONNECT_MNG_WARN("%s: read header failed(%s)\r\n", __func__, strerror(errno));
                connect_mng_drop(&conn->base);
                return;
            }
            
            if (nread == 0) {
                CONNECT_MNG_VERBOS("%s: header connection closed\r\n", __func__);
                connect_mng_drop(&conn->base);
                return;
            }

            conn->bytes_done += nread;
            if (conn->bytes_done != 8) {
                return;
            }
            
            break;
        }

        conn->base.type = ntohl(conn->base.type);
        conn->base.data_len = ntohl(conn->base.data_len);

        if ((conn->base.type >= MAX_BACNET_SERVICE_TYPE)
                || (!service_handler_array[conn->base.type])) {
            CONNECT_MNG_ERROR("%s: invalid service type(%d)\r\n", __func__, conn->base.type);
            connect_mng_drop(&conn->base);
            return;
        }

        if (conn->base.data_len > MAX_CONNECT_MNG_DATA_LEN) {
            CONNECT_MNG_ERROR("%s: data_len(%d) is too long\r\n", __func__, conn->base.data_len);
            connect_mng_drop(&conn->base);
            return;
        }

        conn->base.data = (uint8_t *)malloc(conn->base.data_len);
        if (!conn->base.data) {
            CONNECT_MNG_ERROR("%s: malloc data(%d) failed\r\n", __func__, conn->base.data_len);
            connect_mng_drop(&conn->base);
            return;
        }
    }

    for (;;) {
        nread = read(conn->fd, conn->base.data + conn->bytes_done - 8,
            conn->base.data_len + 8 - conn->bytes_done);
        if (nread < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN) {
                return;
            }
            
            CONNECT_MNG_WARN("%s: read body failed(%s)\r\n", __func__, strerror(errno));
            connect_mng_drop(&conn->base);
            return;
        }
        
        if (nread == 0) {
            CONNECT_MNG_VERBOS("%s: body connection closed\r\n", __func__);
            connect_mng_drop(&conn->base);
            return;
        }

        conn->bytes_done += nread;
        if (conn->bytes_done != conn->base.data_len + 8) {
            return;
        }
        
        break;
    }

    CONNECT_MNG_VERBOS("%s: new request(%u)\r\n", __func__, conn->base.type);

    handler = service_handler_array[conn->base.type];
    conn->in_handler = true;
    if (!handler(&conn->base)) {    /* delayed ack or dropped */
        if (!conn->watcher) {       /* already dropped */
            free(conn);
            return;
        } else {
            el_watch_destroy(&el_default_loop, conn->watcher);
            conn->watcher = NULL;
        }

        if (conn->timer) {
            el_timer_destroy(&el_default_loop, conn->timer);
            conn->timer = NULL;
        }
    }
    
    conn->in_handler = false;
}

static void socket_write_handler(el_watch_t *watch, int events)
{
    connect_info_impl_t *conn;
    int nwrite;
    int rv;
    
    if (!watch || !watch->data) {
        CONNECT_MNG_ERROR("%s: invalid argument\r\n", __func__);
        return;
    }

    conn = (connect_info_impl_t *)watch->data;

    CONNECT_MNG_VERBOS("%s: write events(%d) on fd(%d)\r\n", __func__, events, conn->fd);

    if (!(events & EPOLLOUT)) {
        CONNECT_MNG_ERROR("%s: no EPOLLOUT event\r\n", __func__);
        return;
    }

    if (conn->bytes_done < 8) {
        for (;;) {
            nwrite = write(conn->fd, ((uint8_t*)&conn->base.type) + conn->bytes_done,
                8 - conn->bytes_done);
            if (nwrite < 0) {
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN) {
                    return;
                }
                
                CONNECT_MNG_WARN("%s: write header failed(%s)\r\n", __func__, strerror(errno));
                connect_mng_drop(&conn->base);
                return;
            }

            conn->bytes_done += nwrite;
            if (conn->bytes_done != 8) {
                return;
            }
            
            break;
        }

        conn->base.data_len = ntohl(conn->base.data_len);
    }

    for (;;) {
        nwrite = write(conn->fd, conn->base.data + conn->bytes_done - 8,
            conn->base.data_len + 8 - conn->bytes_done);
        if (nwrite < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN) {
                return;
            }
            
            CONNECT_MNG_WARN("%s: write body failed(%s)\r\n", __func__, strerror(errno));
            connect_mng_drop(&conn->base);
            return;
        }

        conn->bytes_done += nwrite;
        if (conn->bytes_done != conn->base.data_len + 8) {
            return;
        }
        
        break;
    }

    socket_connfd_status_reset(conn);

    rv = el_watch_mod(&el_default_loop, conn->watcher, EPOLLIN);
    if (rv < 0) {
        CONNECT_MNG_ERROR("%s: el watch mod failed(%d)\r\n", __func__, rv);
        connect_mng_drop(&conn->base);
        return;
    }
    conn->watcher->handler = socket_read_handler;

    rv = el_timer_mod(&el_default_loop, conn->timer, MAX_READ_WRITE_TIMEOUT);
    if (rv < 0) {
        CONNECT_MNG_ERROR("%s: el timer mod failed(%d)\r\n", __func__, rv);
        connect_mng_drop(&conn->base);
        return;
    }
}

static void socket_timer_handler(el_timer_t *timer)
{
    connect_info_impl_t *conn;
    
    if ((timer == NULL) || (timer->data == NULL)) {
        CONNECT_MNG_ERROR("%s: invalid argument\r\n", __func__);
        return;
    }

    CONNECT_MNG_VERBOS("%s: read write timeout\r\n", __func__);

    conn = (connect_info_impl_t *)timer->data;
    connect_mng_drop(&conn->base);
}

static int socket_connfd_add(int connfd)
{
    connect_info_impl_t *conn;

    conn = (connect_info_impl_t *)malloc(sizeof(connect_info_impl_t));
    if (conn == NULL) {
        CONNECT_MNG_ERROR("%s: malloc connect info failed\r\n", __func__);
        return -ENOMEM;
    }
    memset(conn, 0, sizeof(connect_info_impl_t));

    conn->fd= connfd;
    conn->watcher = el_watch_create(&el_default_loop, connfd, EPOLLIN);
    if (conn->watcher == NULL) {
        CONNECT_MNG_ERROR("%s: connd_fd(%d) create event watch failed\r\n", __func__, connfd);
        free(conn);
        return -EPERM;
    }

    conn->watcher->handler = socket_read_handler;
    conn->watcher->data = (void *)conn;

    conn->timer = el_timer_create(&el_default_loop, MAX_READ_WRITE_TIMEOUT);
    if (conn->timer == NULL) {
        CONNECT_MNG_ERROR("%s: connd_fd(%d) create timer failed\r\n", __func__, connfd);
        el_watch_destroy(&el_default_loop, conn->watcher);
        free(conn);
        return -EPERM;
    }
    conn->timer->handler = socket_timer_handler;
    conn->timer->data = (void*)conn;

    list_add(&conn->node, &sockfd_list.head);
    sockfd_list.count++;

    return OK;
}

static void socket_accept_handler(el_watch_t *watch, int events)
{
    struct sockaddr_un cli_addr;
    socklen_t cli_len;
    int connd_fd;
    int listen_fd;
    int rv;
    
    listen_fd = el_watch_fd(watch);
    if (listen_fd < 0) {
        CONNECT_MNG_ERROR("%s: invalid listen fd(%d)\r\n", __func__, listen_fd);
        return;
    }
    
    while (1) {
        if (sockfd_list.count >= MAX_SOCKET_LISTEN_BACKLOG) {
            CONNECT_MNG_WARN("%s: no more listen_backlog is present\r\n", __func__);
            rv = el_watch_mod(&el_default_loop, watch, 0);
            if (rv < 0) {
                CONNECT_MNG_ERROR("%s: el watch mod failed(%d)\r\n", __func__, rv);
            }
            break;
        }
    
        cli_len = sizeof(cli_addr);
        connd_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &cli_len);
        if (connd_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                break;
            }

            CONNECT_MNG_ERROR("%s: accept failed cause %s\r\n", __func__, strerror(errno));
            break;
        }

        if (socket_nonblock_set(connd_fd) < 0) {
            CONNECT_MNG_ERROR("%s: connd_fd(%d) set nonblock failed\r\n", __func__, connd_fd);
            close(connd_fd);
            continue;
        }

        if (socket_connfd_add(connd_fd) < 0) {
            CONNECT_MNG_ERROR("%s: add connd_fd(%d) failed\r\n", __func__, connd_fd);
            close(connd_fd);
        } else {
            CONNECT_MNG_VERBOS("%s: new connection(%d)\r\n", __func__, connd_fd);
        }
    }

    return;
}

int connect_mng_echo(connect_info_t *conn)
{
    connect_info_impl_t *conn_impl;
    int nwrite;
    int rv = OK;
    
    if (conn == NULL) {
        CONNECT_MNG_ERROR("%s: null argument\r\n", __func__);
        return -EINVAL;
    }

    if (conn->data_len > MAX_CONNECT_MNG_DATA_LEN) {
        CONNECT_MNG_ERROR("%s: data len(%u) over flow\r\n", __func__, conn->data_len);
        return -EPERM;
    }

    if (conn->data_len && !conn->data) {
        CONNECT_MNG_ERROR("%s: null data\r\n", __func__);
        return -EPERM;
    }

    conn_impl = container_of(conn, connect_info_impl_t, base);

    CONNECT_MNG_VERBOS("%s: echo to connection(%d) len(%d)\r\n", __func__, conn_impl->fd,
        conn->data_len);

    conn_impl->bytes_done = 0;
    conn_impl->base.type = htonl(conn_impl->base.type);
    conn_impl->base.data_len = htonl(conn_impl->base.data_len);

    for (;;) {
        nwrite = write(conn_impl->fd, &conn_impl->base.type, 8);
        if (nwrite < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN) {
                goto again;
            }
            
            CONNECT_MNG_WARN("%s: write header failed(%s)\r\n", __func__, strerror(errno));
            connect_mng_drop(conn);
            return -EPERM;
        }
        
        conn_impl->bytes_done = nwrite;
        if (nwrite < 8) {
            goto again;
        }
        break;
    }

    conn_impl->base.data_len = ntohl(conn_impl->base.data_len);

    for (;;) {
        nwrite = write(conn_impl->fd, conn_impl->base.data, conn_impl->base.data_len);
        if (nwrite < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN) {
                goto again;
            }
            
            CONNECT_MNG_WARN("%s: write body failed(%s)\r\n", __func__, strerror(errno));
            connect_mng_drop(conn);
            return -EPERM;
        }
        
        conn_impl->bytes_done += nwrite;
        if (nwrite < conn_impl->base.data_len) {
            goto again;
        }
        break;
    }

    socket_connfd_status_reset(conn_impl);

    el_sync(&el_default_loop);

    if (conn_impl->watcher) {
        rv = el_watch_mod(&el_default_loop, conn_impl->watcher, EPOLLIN);
        if (rv < 0) {
            CONNECT_MNG_ERROR("%s: el watch mod failed(%d)\r\n", __func__, rv);
            connect_mng_drop(conn);
            el_unsync(&el_default_loop);
            return rv;
        }
    } else {
        conn_impl->watcher = el_watch_create(&el_default_loop, conn_impl->fd, EPOLLIN);
        if (conn_impl->watcher == NULL) {
            CONNECT_MNG_ERROR("%s: create event watch failed\r\n", __func__);
            connect_mng_drop(conn);
            el_unsync(&el_default_loop);
            return -EPERM;
        }
        conn_impl->watcher->data = (void *)conn_impl;
    }
    conn_impl->watcher->handler = socket_read_handler;
    goto set_timer;

again:
    el_sync(&el_default_loop);

    if (conn_impl->watcher) {
        rv = el_watch_mod(&el_default_loop, conn_impl->watcher, EPOLLOUT);
        if (rv < 0) {
            CONNECT_MNG_ERROR("%s: el watch mod failed(%d)\r\n", __func__, rv);
            connect_mng_drop(conn);
            el_unsync(&el_default_loop);
            return rv;
        }
    } else {
        conn_impl->watcher = el_watch_create(&el_default_loop, conn_impl->fd, EPOLLOUT);
        if (conn_impl->watcher == NULL) {
            CONNECT_MNG_ERROR("%s: create event watch failed\r\n", __func__);
            connect_mng_drop(conn);
            el_unsync(&el_default_loop);
            return -EPERM;
        }

        conn_impl->watcher->data = (void *)conn_impl;
    }
    conn_impl->watcher->handler = socket_write_handler;

set_timer:
    if (conn_impl->timer) {
        rv = el_timer_mod(&el_default_loop, conn_impl->timer, MAX_READ_WRITE_TIMEOUT);
        if (rv < 0) {
            CONNECT_MNG_ERROR("%s: el timer mod failed(%d)\r\n", __func__, rv);
            connect_mng_drop(conn);
            el_unsync(&el_default_loop);
            return rv;
        }
    } else {
        conn_impl->timer = el_timer_create(&el_default_loop, MAX_READ_WRITE_TIMEOUT);
        if (conn_impl->timer == NULL) {
            CONNECT_MNG_ERROR("%s: create timer failed\r\n", __func__);
            connect_mng_drop(conn);
            el_unsync(&el_default_loop);
            return -EPERM;
        }
        conn_impl->timer->handler = socket_timer_handler;
        conn_impl->timer->data = (void *)conn_impl;
    }

    el_unsync(&el_default_loop);
    
    return rv;
}

int connect_mng_drop(connect_info_t *conn)
{
    connect_info_impl_t *conn_impl;
    int rv;
    
    if (!conn) {
        CONNECT_MNG_ERROR("%s: null argument\r\n", __func__);
        return -EINVAL;
    }

    el_sync(&el_default_loop);

    conn_impl = container_of(conn, connect_info_impl_t, base);

    if (conn_impl->watcher) {
        el_watch_destroy(&el_default_loop, conn_impl->watcher);
        conn_impl->watcher = NULL;
    }

    if (conn_impl->fd != -1) {
        close(conn_impl->fd);
        conn_impl->fd = -1;
    }

    if (conn_impl->timer) {     /* in handler, keep conn */
        el_timer_destroy(&el_default_loop, conn_impl->timer);
        conn_impl->timer = NULL;
    }

    socket_connfd_status_reset(conn_impl);
    list_del(&conn_impl->node);

    if (!conn_impl->in_handler) {
        free(conn);
    }

    if (sockfd_list.count-- == MAX_SOCKET_LISTEN_BACKLOG) {
        rv = el_watch_mod(&el_default_loop, listen_watcher, EPOLLIN);
        if (rv < 0) {
            CONNECT_MNG_ERROR("%s: el watch mod listen socket error\r\n", __func__);
        }
    }

    if (sockfd_list.count < 0) {
        CONNECT_MNG_ERROR("%s: sockfd count error\r\n", __func__);
        sockfd_list.count = 0;
        INIT_LIST_HEAD(&sockfd_list.head);
    }

    el_unsync(&el_default_loop);
    
    return OK;
}

int connect_mng_register(BACNET_SERVICE_TYPE type, connect_service_handler handler)
{
    if ((type >= MAX_BACNET_SERVICE_TYPE) || (handler == NULL)) {
        CONNECT_MNG_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    if (!connect_mng_status) {
        CONNECT_MNG_ERROR("%s: connect manager is not initialized\r\n", __func__);
        return -EPERM;
    }

    if (service_handler_array[type] != NULL) {
        CONNECT_MNG_ERROR("%s: service type(%d) is already registered\r\n", __func__, type);
        return -EPERM;
    }

    service_handler_array[type] = handler;
    
    return OK;
}

int connect_mng_unregister(BACNET_SERVICE_TYPE type)
{
    if (type >= MAX_BACNET_SERVICE_TYPE) {
        CONNECT_MNG_ERROR("%s: invalid type(%d)\r\n", __func__, type);
        return -EINVAL;
    }

    if (!connect_mng_status) {
        CONNECT_MNG_ERROR("%s: connect manager is not initialized\r\n", __func__);
        return -EPERM;
    }

    service_handler_array[type] = NULL;

    return OK;
}

int connect_mng_init(void)
{
    struct sockaddr_un serv_addr;
    int listen_fd;
    int rv;

    if (connect_mng_status) {
        CONNECT_MNG_WARN("%s: already inited\r\n", __func__);
        return OK;
    }
    
    (void)memset(service_handler_array, 0, sizeof(service_handler_array));
    sockfd_list.count = 0;
    INIT_LIST_HEAD(&sockfd_list.head);
    
    listen_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        CONNECT_MNG_ERROR("%s: create listen_socket failed cause %s\r\n", __func__, strerror(errno));
        rv = -EPERM;
        goto out0;
    }

    unlink(SERVER_UNIXDG_PATH);
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sun_family = AF_LOCAL;
    (void)strncpy(serv_addr.sun_path, SERVER_UNIXDG_PATH, sizeof(serv_addr.sun_path) - 1);
    rv = bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (rv < 0) {
        CONNECT_MNG_ERROR("%s: bind listen_socket failed cause %s\r\n", __func__, strerror(errno));
        goto out1;
    }
    
    rv = listen(listen_fd, MAX_SOCKET_LISTEN_BACKLOG);
    if (rv < 0) {
        CONNECT_MNG_ERROR("%s: listen socket failed cause %s\r\n", __func__, strerror(errno));
        goto out1;
    }

    rv = socket_nonblock_set(listen_fd);
    if (rv < 0) {
        CONNECT_MNG_ERROR("%s: listen_socket set nonblock failed(%d)\r\n", __func__, rv);
        goto out1;
    }
    
    el_sync(&el_default_loop);

    listen_watcher = el_watch_create(&el_default_loop, listen_fd, EPOLLIN);
    if (listen_watcher == NULL) {
        CONNECT_MNG_ERROR("%s: event watch create listen_fd(%d) failed\r\n", __func__, listen_fd);
        el_unsync(&el_default_loop);
        rv = -EPERM;
        goto out1;
    }
    listen_watcher->handler = socket_accept_handler;

    connect_mng_status = true;
    el_unsync(&el_default_loop);
    connect_mng_set_dbg_level(0);

    return OK;
    
out1:
    close(listen_fd);

out0:

    return rv;
}

void connect_mng_exit(void)
{
    connect_info_impl_t *sockfd_info, *tmp;
    int listen_fd;

    if (!connect_mng_status) {
        return;
    }

    listen_fd = el_watch_fd(listen_watcher);
    (void)el_watch_destroy(&el_default_loop, listen_watcher);
    close(listen_fd);
    
    list_for_each_entry_safe(sockfd_info, tmp, &sockfd_list.head, node) {
        connect_mng_drop(&sockfd_info->base);
    }

    connect_mng_status = false;
    
    return;
}

void connect_mng_set_dbg_level(uint32_t level)
{
    connet_mng_dbg_verbos = level & DEBUG_LEVEL_VERBOS;
    connet_mng_dbg_warn = level & DEBUG_LEVEL_WARN;
    connet_mng_dbg_err = level & DEBUG_LEVEL_ERROR;
}

connect_client_state connect_client(BACNET_SERVICE_TYPE type, uint8_t *request, uint32_t req_len,
                        uint8_t **rsp, uint32_t *rsp_len, unsigned timeout_ms)
{
    struct sockaddr_un serv_addr;
    struct timeval tv;
    struct timespec start_ts, now_ts;
    connect_mng_pkt_t pkt;
    uint32_t value;
    unsigned diffms;
    int client_fd, rv;

    if ((rsp == NULL) || (rsp_len == NULL) || (request == NULL && req_len != 0)) {
        CONNECT_MNG_ERROR("%s: null argument\r\n", __func__);
        return CONNECT_CLIENT_ERROR_OTHER;
    }

    rv = clock_gettime(CLOCK_MONOTONIC_COARSE, &start_ts);
    if (rv < 0) {
        CONNECT_MNG_ERROR("%s: clock gettime failed cause %s\r\n", __func__, strerror(errno));
        return CONNECT_CLIENT_CREATE_FAILED;
    }

    client_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (client_fd < 0) {
        CONNECT_MNG_ERROR("%s: create socket failed cause %s\r\n", __func__, strerror(errno));
        return CONNECT_CLIENT_CREATE_FAILED;
    }

    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sun_family = AF_LOCAL;
    (void)strncpy(serv_addr.sun_path, SERVER_UNIXDG_PATH, sizeof(serv_addr.sun_path) - 1);

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv)) < 0) {
        CONNECT_MNG_ERROR("%s: setsockopt SO_SNDTIME0 failed cause %s\r\n", __func__, strerror(errno));
        close(client_fd);
        return CONNECT_CLIENT_CREATE_FAILED;
    }
    
    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv)) < 0) {
        CONNECT_MNG_ERROR("%s: setsockopt SO_RCVTIME0 failed cause %s\r\n", __func__, strerror(errno));
        close(client_fd);
        return CONNECT_CLIENT_CREATE_FAILED;
    }

    for (;;) {
        clock_gettime(CLOCK_MONOTONIC_COARSE, &now_ts);
        diffms = (now_ts.tv_sec - start_ts.tv_sec) * 1000
                + (now_ts.tv_nsec - start_ts.tv_nsec) / 1000000;
        if (diffms >= timeout_ms) {
            CONNECT_MNG_WARN("%s: connect timeout\r\n", __func__);
            close(client_fd);
            return CONNECT_CLIENT_CONNECT_FAILED;
        }
        tv.tv_sec = (diffms - timeout_ms) / 1000;
        tv.tv_usec = ((diffms - timeout_ms) % 1000) * 1000;
        setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));

        rv = connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
        if (rv < 0) {
            if (errno == EINTR) {
                continue;
            }
            CONNECT_MNG_WARN("%s: connect failed cause %s\r\n", __func__, strerror(errno));
            close(client_fd);
            return CONNECT_CLIENT_CONNECT_FAILED;
        }
        
        break;
    }

    (void)encode_unsigned32((uint8_t *)&pkt, type);
    (void)encode_unsigned32(((uint8_t *)&pkt) + 4, req_len);

    for (;;) {
        clock_gettime(CLOCK_MONOTONIC_COARSE, &now_ts);
        diffms = (now_ts.tv_sec - start_ts.tv_sec) * 1000 
            + (now_ts.tv_nsec - start_ts.tv_nsec) / 1000000;
        if (diffms >= timeout_ms) {
            CONNECT_MNG_WARN("%s: send timeout\r\n", __func__);
            close(client_fd);
            return CONNECT_CLIENT_SEND_FAIL;
        }
        
        tv.tv_sec = (diffms - timeout_ms) / 1000;
        tv.tv_usec = ((diffms - timeout_ms) % 1000) * 1000;
        setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));

        rv = write(client_fd, (void *)&pkt, 8);
        if (rv != 8) {
            if (rv < 0) {
                if (errno == EINTR) {
                    continue;
                }
                CONNECT_MNG_WARN("%s: send failed cause %s\r\n", __func__, strerror(errno));
            } else {
                CONNECT_MNG_WARN("%s: send timeout\r\n", __func__);
            }
            close(client_fd);
            return CONNECT_CLIENT_SEND_FAIL;
        }
        
        break;
    }

    while (req_len) {
        clock_gettime(CLOCK_MONOTONIC_COARSE, &now_ts);
        diffms = (now_ts.tv_sec - start_ts.tv_sec) * 1000
            + (now_ts.tv_nsec - start_ts.tv_nsec) / 1000000;
        if (diffms >= timeout_ms) {
            CONNECT_MNG_WARN("%s: send timeout\r\n", __func__);
            close(client_fd);
            return CONNECT_CLIENT_SEND_FAIL;
        }
        
        tv.tv_sec = (diffms - timeout_ms) / 1000;
        tv.tv_usec = ((diffms - timeout_ms) % 1000) * 1000;
        setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));

        rv = write(client_fd, request, req_len);
        if (rv != req_len) {
            if (rv < 0) {
                if (errno == EINTR) {
                    continue;
                }
                CONNECT_MNG_WARN("%s: send failed cause %s\r\n", __func__, strerror(errno));
            } else {
                CONNECT_MNG_WARN("%s: send timeout\r\n", __func__);
            }
            close(client_fd);
            return CONNECT_CLIENT_SEND_FAIL;
        }
        
        break;
    }

    for (;;) {
        clock_gettime(CLOCK_MONOTONIC_COARSE, &now_ts);
        diffms = (now_ts.tv_sec - start_ts.tv_sec) * 1000
            + (now_ts.tv_nsec - start_ts.tv_nsec) / 1000000;
        if (diffms >= timeout_ms) {
            CONNECT_MNG_WARN("%s: receive timeout\r\n", __func__);
            close(client_fd);
            return CONNECT_CLIENT_WAIT_FAIL;
        }
        
        tv.tv_sec = (diffms - timeout_ms) / 1000;
        tv.tv_usec = ((diffms - timeout_ms) % 1000) * 1000;
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));

        rv = read(client_fd, (void *)&pkt, 8);
        if (rv != 8) {
            if (rv < 0) {
                if (errno == EINTR) {
                    continue;
                }
                CONNECT_MNG_WARN("%s: receive failed cause %s\r\n", __func__, strerror(errno));
            } else if (rv == 0) {
                CONNECT_MNG_WARN("%s: receive remote closed\r\n", __func__);
            } else {
                CONNECT_MNG_WARN("%s: receive timeout\r\n", __func__);
            }
            close(client_fd);
            return rv <= 0 ? CONNECT_CLIENT_WAIT_FAIL :CONNECT_CLIENT_RECV_FAIL;
        }
        
        break;
    }

    (void)decode_unsigned32((uint8_t *)&pkt, &value);
    if (value != type) {
        CONNECT_MNG_WARN("%s: receive type(%d) not match original(%d)\r\n", __func__, value, type);
        close(client_fd);
        return CONNECT_CLIENT_PROTO_ERROR;
    }

    (void)decode_unsigned32(((uint8_t *)&pkt) + 4, &value);
    *rsp_len = value;
    if (value == 0) {
        *rsp = NULL;
        close(client_fd);
        return CONNECT_CLIENT_OK;
    }

    *rsp = (uint8_t *)malloc(value);
    if (*rsp == NULL) {
        CONNECT_MNG_ERROR("%s: not enough memory\r\n", __func__);
        close(client_fd);
        return CONNECT_CLIENT_ERROR_OTHER;
    }

    for (;;) {
        clock_gettime(CLOCK_MONOTONIC_COARSE, &now_ts);
        diffms = (now_ts.tv_sec - start_ts.tv_sec) * 1000
            + (now_ts.tv_nsec - start_ts.tv_nsec) / 1000000;
        if (diffms >= timeout_ms) {
            CONNECT_MNG_WARN("%s: receive timeout\r\n", __func__);
            free(*rsp);
            *rsp = NULL;
            close(client_fd);
            return CONNECT_CLIENT_RECV_FAIL;
        }
        
        tv.tv_sec = (diffms - timeout_ms) / 1000;
        tv.tv_usec = ((diffms - timeout_ms) % 1000) * 1000;
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));

        rv = read(client_fd, *rsp, *rsp_len);
        if (rv != *rsp_len) {
            if (rv < 0) {
                if (errno == EINTR) {
                    continue;
                }
                CONNECT_MNG_WARN("%s: receive failed cause %s\r\n", __func__, strerror(errno));
            } else {
                CONNECT_MNG_WARN("%s: receive partial body\r\n", __func__);
            }
            free(*rsp);
            *rsp = NULL;
            close(client_fd);
            return CONNECT_CLIENT_RECV_FAIL;
        }
        break;
    }

    close(client_fd);
    
    return CONNECT_CLIENT_OK;
}

static void client_fd_handler(el_watch_t *watch, int events)
{
    connect_client_async_impl_t *impl;
    int client_fd, rv;
    
    impl = (connect_client_async_impl_t *)watch->data;
    impl->in_callback = 1;
    
    if (events & EPOLLHUP) {
        if (impl->base.callback) {
            connect_client_state state;
            if (impl->recv_bytes) {         /* recv failed */
                state = CONNECT_CLIENT_RECV_FAIL;
            } else if (impl->sent_bytes == impl->base.len + 8) {    /* wait failed */
                state = CONNECT_CLIENT_WAIT_FAIL;
            } else if (impl->sent_bytes) {  /* send failed */
                state = CONNECT_CLIENT_SEND_FAIL;
            } else {                        /* connect failed */
                state = CONNECT_CLIENT_CONNECT_FAILED;
            }
            impl->base.callback(&impl->base, state);
        }
        goto recovery;
    } else if (events & EPOLLIN) {
        client_fd = el_watch_fd(impl->watch);

        if (impl->recv_bytes < 8) {
            for (;;) {
                rv = read(client_fd, ((uint8_t*)&impl->base.data) + impl->recv_bytes,
                    8 - impl->recv_bytes);
                if (rv > 0) {
                    impl->recv_bytes += rv;
                    break;
                }
                
                if ((rv < 0) && (errno == EINTR)) {
                    continue;
                }
                
                if (impl->base.callback) {
                    impl->base.callback(&impl->base, CONNECT_CLIENT_RECV_FAIL);
                }
                goto recovery;
            }
            
            if (impl->recv_bytes < 8) {
                goto exit;
            }

            if ((uint32_t)impl->base.data != impl->base.type) {
                if (impl->base.callback) {
                    impl->base.callback(&impl->base, CONNECT_CLIENT_PROTO_ERROR);
                }
                goto recovery;
            }
            if (impl->base.len == 0) {
                impl->base.data = NULL;
            } else {
                impl->base.data = (uint8_t *)malloc(impl->base.len);
                if (impl->base.data == NULL) {
                    CONNECT_MNG_ERROR("%s: not enough memory\r\n", __func__);
                    if (impl->base.callback) {
                        impl->base.callback(&impl->base, CONNECT_CLIENT_ERROR_OTHER);
                    }
                    goto recovery;
                }
            }
        }
        
        while (impl->base.len + 8 > impl->recv_bytes) {
            rv = read(client_fd, impl->base.data + impl->recv_bytes - 8,
                impl->base.len + 8 - impl->recv_bytes);
            if (rv > 0) {
                impl->recv_bytes += rv;
                break;
            }
            
            if (rv < 0) {
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN) {
                    goto exit;
                }
            }
            if (impl->base.callback) {
                impl->base.callback(&impl->base, CONNECT_CLIENT_RECV_FAIL);
            }
            goto recovery;
        }
        if (impl->recv_bytes < impl->base.len + 8) {
            goto exit;
        }

        if (impl->base.callback) {
            impl->base.callback(&impl->base, CONNECT_CLIENT_OK);
        }
        goto recovery;
    } else if (events & EPOLLOUT) {
        client_fd = el_watch_fd(impl->watch);

        if (impl->sent_bytes == 0) {
            if (impl->base.callback) {
                impl->base.callback(&impl->base, CONNECT_CLIENT_CONNECTED);
                if (impl->watch == NULL) {    /* 是否在回调中被delete */
                    goto recovery;
                }
            }

            if (impl->base.len && impl->base.data == NULL) {
                if (impl->base.callback) {
                    impl->base.callback(&impl->base, CONNECT_CLIENT_SEND_FAIL);
                }
                goto recovery;
            }

            rv = el_timer_mod(impl->el, impl->timer, impl->base.timeout_ms);
            if (rv < 0) {
                CONNECT_MNG_ERROR("%s: el_timer_mod after first received failed\r\n", __func__);
                if (impl->base.callback) {
                    impl->base.callback(&impl->base, CONNECT_CLIENT_ERROR_OTHER);
                }
                goto recovery;
            }

            connect_mng_pkt_t pkt;

            (void)encode_unsigned32((uint8_t *)&pkt, impl->base.type);
            (void)encode_unsigned32(((uint8_t *)&pkt) + 4, impl->base.len);

            for (;;) {
                rv = write(client_fd, (void *)&pkt, 8);
                if (rv == 8) {
                    impl->sent_bytes = 8;
                    break;
                }
                
                if ((rv < 0) && (errno == EINTR)) {
                    continue;
                }

                if (impl->base.callback) {
                    impl->base.callback(&impl->base, CONNECT_CLIENT_SEND_FAIL);
                }
                goto recovery;
            }
        }

        while (impl->base.len) {
            rv = write(client_fd, impl->base.data + impl->sent_bytes - 8,
                impl->base.len + 8 - impl->sent_bytes);
            if (rv > 0) {
                impl->sent_bytes += rv;
                break;
            }
            
            if (rv < 0) {
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN) {
                    goto exit;
                }
            }
            if (impl->base.callback) {
                impl->base.callback(&impl->base, CONNECT_CLIENT_SEND_FAIL);
            }
            goto recovery;
        }
        
        if (impl->sent_bytes < impl->base.len + 8) {
            goto exit;
        }

        if (impl->base.callback) {
            impl->base.callback(&impl->base, CONNECT_CLIENT_SENT);
            if (impl->watch == NULL) {      /* 是否在call中被删除 */
                goto recovery;
            }
        }

        rv = el_watch_mod(impl->el, impl->watch, EPOLLIN);
        if (rv < 0) {
            CONNECT_MNG_ERROR("%s: el_watch_mod failed\r\n", __func__);
            if (impl->base.callback) {
                impl->base.callback(&impl->base, CONNECT_CLIENT_ERROR_OTHER);
            }
            goto recovery;
        }

        rv = el_timer_mod(impl->el, impl->timer, impl->base.timeout_ms);
        if (rv < 0) {
            CONNECT_MNG_ERROR("%s: el_timer_mod after sent failed\r\n", __func__);
            if (impl->base.callback) {
                impl->base.callback(&impl->base, CONNECT_CLIENT_ERROR_OTHER);
            }
            goto recovery;
        }
    }

exit:
    impl->in_callback = 0;
    return;

recovery:
    if (impl->watch) {
        client_fd = el_watch_fd(impl->watch);
        (void)el_watch_destroy(impl->el, impl->watch);
    } else {
        client_fd = -1;
    }
    
    if (impl->timer) {
        (void)el_timer_destroy(impl->el, impl->timer);
    }

    if (impl->recv_bytes >= 8 && impl->base.data) {
        free(impl->base.data);
    }

    free(impl);
    if (client_fd >= 0) {
        close(client_fd);
    }
}

static void client_timeout_handler(el_timer_t *timer)
{
    int client_fd;
    connect_client_async_impl_t *impl;

    impl = (connect_client_async_impl_t *)timer->data;
    impl->in_callback = 1;
    
    if (impl->base.callback) {
        connect_client_state state;
        if (impl->recv_bytes) {             /* recv failed */
            state = CONNECT_CLIENT_RECV_FAIL;
        } else if (impl->sent_bytes == impl->base.len + 8) {    /* wait failed */
            state = CONNECT_CLIENT_WAIT_FAIL;
        } else if (impl->sent_bytes) {      /* send failed */
            state = CONNECT_CLIENT_SEND_FAIL;
        } else {        /* connect failed */
            state = CONNECT_CLIENT_CONNECT_FAILED;
        }
        impl->base.callback(&impl->base, state);
    }

    if (impl->watch) {
        client_fd = el_watch_fd(impl->watch);
        el_watch_destroy(impl->el, impl->watch);
    } else {
        client_fd = -1;
    }
    
    if (impl->timer) {
        el_timer_destroy(impl->el, impl->timer);
    }

    if (impl->recv_bytes >= 8 && impl->base.data) {
        free(impl->base.data);
    }

    free(impl);
    if (client_fd >= 0) {
        close(client_fd);
    }
}

connect_client_async_t *connect_client_async_create(struct el_loop_s *el, unsigned timeout_ms)
{
    connect_client_async_impl_t *impl;
    struct sockaddr_un serv_addr;
    int client_fd, rv;

    if (el == NULL) {
        CONNECT_MNG_ERROR("%s: null argument\r\n", __func__);
        return NULL;
    }

    client_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (client_fd < 0) {
        CONNECT_MNG_ERROR("%s: create socket failed cause %s\r\n", __func__, strerror(errno));
        return NULL;
    }
    socket_nonblock_set(client_fd);

    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sun_family = AF_LOCAL;
    (void)strncpy(serv_addr.sun_path, SERVER_UNIXDG_PATH, sizeof(serv_addr.sun_path) - 1);

    rv = connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if ((rv < 0) && (errno != EAGAIN)) {
        CONNECT_MNG_ERROR("%s: connect failed cause %s\r\n", __func__, strerror(errno));
        close(client_fd);
        return NULL;
    }

    impl = (connect_client_async_impl_t *)malloc(sizeof(connect_client_async_impl_t));
    if (impl == NULL) {
        CONNECT_MNG_ERROR("%s: not enough memory\r\n", __func__);
        close(client_fd);
        return NULL;
    }
    bzero(impl, sizeof(connect_client_async_impl_t));

    impl->el = el;
    impl->base.timeout_ms = timeout_ms;

    el_sync(el);
    impl->watch = el_watch_create(el, client_fd, EPOLLOUT);
    if (impl->watch == NULL) {
        CONNECT_MNG_ERROR("%s: create watch failed\r\n", __func__);
        el_unsync(el);
        free(impl);
        close(client_fd);
        return NULL;
    }
    impl->watch->handler = client_fd_handler;
    impl->watch->data = (void*)impl;

    impl->timer = el_timer_create(el, timeout_ms);
    if (impl->timer == NULL) {
        CONNECT_MNG_ERROR("%s: create timer failed\r\n", __func__);
        el_watch_destroy(el, impl->watch);
        el_unsync(el);
        free(impl);
        close(client_fd);
        return NULL;
    }
    impl->timer->handler = client_timeout_handler;
    impl->timer->data = (void*)impl;
    el_unsync(el);
    
    return &impl->base;
}

int connect_client_async_delete(connect_client_async_t *connect)
{
    connect_client_async_impl_t *impl;
    el_loop_t *el;
    int client_fd = -1;
    
    if (connect == NULL) {
        CONNECT_MNG_ERROR("%s: null argument\r\n", __func__);
        return -EINVAL;
    }
    
    impl = container_of(connect, connect_client_async_impl_t, base);
    el = impl->el;

    el_sync(el);
    
    if (impl->watch) {
        client_fd = el_watch_fd(impl->watch);
        el_watch_destroy(el, impl->watch);
        impl->watch = NULL;
    }

    if (impl->timer) {
        el_timer_destroy(el, impl->timer);
        impl->timer = NULL;
    }

    if (impl->recv_bytes >= 8 && impl->base.data) {
        free(impl->base.data);
        impl->base.data = NULL;
    }

    if (!impl->in_callback) {
        free(impl);
    }
    
    if (client_fd >= 0) {
        close(client_fd);
    }
    
    el_unsync(el);

    return OK;
}

