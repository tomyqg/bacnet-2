/*
 * webui.h
 *
 *  Created on: May 3, 2016
 *      Author: lin
 */

#ifndef DEMO_WEBUI_WEBUI_H_
#define DEMO_WEBUI_WEBUI_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "mongoose.h"
#include "misc/cJSON.h"

typedef void (*web_handler)(const struct mg_request_info *ri, struct mg_connection *conn,
                const char *locale);

extern int reg_handler(const char *name, web_handler handler);
extern int unreg_handler(const char *name);
extern web_handler find_handler(const char *name);

extern void errResp(struct mg_connection *conn, const char *msg);
extern void norResp(struct mg_connection *conn, const char *msg);
extern void jsonResp(struct mg_connection *conn, cJSON *json);
extern void jsonErrResp(struct mg_connection *conn, const char *msg);
extern void htmlJsonErrResp(struct mg_connection *conn, const char *msg);
extern void chunkedHeaderResp(struct mg_connection *conn);
extern void chunkedBody(struct mg_connection *conn, const char *msg, size_t len);

extern void drop_left_request(struct mg_connection *conn);

extern cJSON *getGlobalCfg(void);
extern void setGlobalCfg(cJSON *cfg);

#ifdef __cplusplus
}
#endif

#endif /* DEMO_WEBUI_WEBUI_H_ */

