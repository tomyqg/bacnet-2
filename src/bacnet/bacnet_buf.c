/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * bacnet_buf.c
 * Original Author:  linzhixian, 2014-12-18
 *
 * History
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "bacnet/bacnet_buf.h"
#include "bacnet/bacdef.h"

int bacnet_buf_init(bacnet_buf_t *buf, uint32_t size)
{
    if (buf == NULL) {
        return -EINVAL;
    }

    buf->data = buf->head + BACNET_BUF_HEADROOM;
    buf->end = buf->data + size;
    buf->data_len = 0;

    return OK;
}

int bacnet_buf_resize(bacnet_buf_t *buf, uint32_t size)
{
    if (buf == NULL) {
        return -EINVAL;
    }

    if (buf->end - buf->data < size)
        return -EINVAL;

    buf->end = buf->data + size;

    return OK;
}

int bacnet_buf_push(bacnet_buf_t *buf, uint32_t len)
{
    uint8_t *tmp;

    if (buf == NULL) {
        return -EINVAL;
    }

    tmp = buf->data - len;
    if (tmp < buf->head) {
        return -EPERM;
    }

    buf->data = tmp;
    buf->data_len += len;

    return OK;
}

int bacnet_buf_pull(bacnet_buf_t *buf, uint32_t len)
{
    if (buf == NULL) {
        return -EINVAL;
    }

    if (len > buf->data_len)
        return -EPERM;

    buf->data += len;
    buf->data_len -= len;

    return OK;
}

