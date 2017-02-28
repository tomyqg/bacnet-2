/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * connect_mng.h
 * Original Author:  linzhixian, 2015-9-10
 *
 * Connect Manager
 *
 * History
 */

#ifndef _CONNECT_MNG_H_
#define _CONNECT_MNG_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define SERVER_UNIXDG_PATH              ("/tmp/server_socket")

typedef enum {
    BACNET_WEB_SERVICE = 0,
    BACNET_DEBUG_SERVICE = 1,
    MAX_BACNET_SERVICE_TYPE
} BACNET_SERVICE_TYPE;

typedef enum {
    CONNECT_CLIENT_CREATE_FAILED = -1,
    CONNECT_CLIENT_CONNECT_FAILED = -2,
    CONNECT_CLIENT_SEND_FAIL = -3,
    CONNECT_CLIENT_WAIT_FAIL = -4,
    CONNECT_CLIENT_RECV_FAIL = -5,
    CONNECT_CLIENT_PROTO_ERROR = -6,
    CONNECT_CLIENT_ERROR_OTHER = -7,
    CONNECT_CLIENT_OK = 0,
    CONNECT_CLIENT_CONNECTED,
    CONNECT_CLIENT_SENT,
} connect_client_state;

typedef struct connect_info_s {
    BACNET_SERVICE_TYPE type;
    uint32_t data_len;          /* length of data */
    uint8_t *data;              /* buffer of data read in or to write */
} connect_info_t;

/* request body is in conn->data, handler should free it if needn't it.
 * @return true if handler has acked
 * false if ack is delayed or connection dropped */
typedef bool (*connect_service_handler)(connect_info_t *conn);

struct connect_client_async_s;
typedef struct connect_client_async_s connect_client_async_t;

typedef void (*connect_client_callback)(connect_client_async_t *connect, connect_client_state state);

/*
 * 在create之后，应立即设置callback(及context), 注意event loop同步
 * 最迟在callback收到CONNECT_CLIENT_CONNECTED时，应设置type, data, len
 * timeout_ms可以修改, 如果服务器需要执行较长时间，在callback收到CONNECT_CLIENT_SENT之后，修
 * 改timeout_ms为较大值。在callback收到CONNECT_CLIENT_RECV之后再修改为较小值。
 * 在callback收到CONNECT_CLIENT_SENT时，可以释放data。随后data与len被用于记录应答数据。应答数据不需
 * 用户负责释放。
 */
struct connect_client_async_s {
    uint8_t *data;
    uint32_t len;
    BACNET_SERVICE_TYPE type;
    connect_client_callback callback;
    unsigned context;
    unsigned timeout_ms;
};

struct el_loop_s;

extern connect_client_async_t *connect_client_async_create(struct el_loop_s *el, unsigned timeout_ms);

/**
 * 用户可以中断异步client请求。
 * 在请求OK/FAIL后，用户不需delete，回调返回后，后台自动删除connect。
 */
extern int connect_client_async_delete(connect_client_async_t *connect);

extern connect_client_state connect_client(BACNET_SERVICE_TYPE type, uint8_t *request,
        uint32_t req_len, uint8_t **rsp, uint32_t *rsp_len, unsigned timeout_ms);

/*
 * conn->data is data to reply. it will be free if all bytes sent
 */
extern int connect_mng_echo(connect_info_t *conn);

/*
 * drop connection and reclaim conn
 */
extern int connect_mng_drop(connect_info_t *conn);

extern int connect_mng_register(BACNET_SERVICE_TYPE type, connect_service_handler handler);

extern int connect_mng_unregister(BACNET_SERVICE_TYPE type);

extern int connect_mng_init(void);

extern void connect_mng_exit(void);

extern void connect_mng_set_dbg_level(uint32_t level);

#ifdef __cplusplus
}
#endif

#endif /* _CONNECT_MNG_H_ */

