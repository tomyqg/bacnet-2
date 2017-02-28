/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bacnet_buf.h
 * Original Author:  linzhixian, 2014-12-2
 *
 * History
 */

#ifndef _BACNET_BUF_H_
#define _BACNET_BUF_H_

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define BACNET_BUF_HEADROOM         (40)
#define BACNET_BUF_TAILROOM         (32)
#define BACNET_BUF_ALLROOM          (BACNET_BUF_HEADROOM + BACNET_BUF_TAILROOM)

typedef struct bacnet_buf_s {
    uint8_t *data;
    uint8_t *end;
    uint32_t data_len;
    uint8_t head[];                 /* Êý¾Ý¶Î */
} bacnet_buf_t;

#define DECLARE_BACNET_BUF(name, size)                      \
    struct {                                                \
        bacnet_buf_t buf;                                   \
        uint8_t __padding[size + BACNET_BUF_ALLROOM];       \
    } name

static inline uint32_t bacnet_buf_calsize(uint32_t size)
{
    return (sizeof(bacnet_buf_t) + size + BACNET_BUF_ALLROOM + sizeof(void*) - 1)
            / sizeof(void*) * sizeof(void*);
}

/*
 * could only trim buffer, disallow extend buffer.
 */
extern int bacnet_buf_resize(bacnet_buf_t *buf, uint32_t size);

extern int bacnet_buf_push(bacnet_buf_t *buf, uint32_t len);

extern int bacnet_buf_pull(bacnet_buf_t *buf, uint32_t len);

extern int bacnet_buf_init(bacnet_buf_t *buf, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif  /* _BACNET_BUF_H_ */

