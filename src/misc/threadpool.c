/*
 * threadpool.c
 *
 *  Created on: Apr 22, 2016
 *      Author: lin
 */

#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <sys/prctl.h>
#include "threadpool_def.h"

bool tp_dbg_verbos = false;
bool tp_dbg_warn = false;
bool tp_dbg_err = true;

tp_pool_t tp_default_pool = {
        .started = false,
        .worker_count = 0,
        .head = LIST_HEAD_INIT(tp_default_pool.head),
        .lock = PTHREAD_MUTEX_INITIALIZER,
        .cond = PTHREAD_COND_INITIALIZER,
};

static void _push_work(tp_pool_t *tp, tp_work_t work)
{
    tp_work_storage_t *storage;
    if (!list_empty(&tp->head)) {
        storage = list_last_entry(&tp->head, tp_work_storage_t, node);

        if (storage->size < TP_WORK_STORAGE_SIZE) {
            int idx = (storage->next + storage->size) % TP_WORK_STORAGE_SIZE;
            storage->works[idx] = work;
            storage->size++;
            return;
        }
    }

    storage = (tp_work_storage_t*)malloc(sizeof(tp_work_storage_t));
    if (storage == NULL) {
        TP_ERROR("%s: no enough memory\r\n", __func__);
        return;
    }

    storage->works[0] = work;
    storage->next = 0;
    storage->size = 1;

    list_add(&storage->node, &tp->head);
}

static tp_work_t _pull_work(tp_pool_t *tp)
{
    tp_work_storage_t *storage;
    tp_work_t work;

    if (list_empty(&tp->head)) {
        work = (tp_work_t){NULL, NULL};
        return work;
    }

    storage = list_first_entry(&tp->head, tp_work_storage_t, node);

    work = storage->works[storage->next];

    if (--storage->size) {
        storage->next = (storage->next + 1) % TP_WORK_STORAGE_SIZE;
    } else {
        __list_del_entry(&storage->node);
        free(storage);
    }

    return work;
}

static int _worker_thread(tp_pool_t *tp);

static int _worker_guard(tp_pool_t *tp)
{
    pthread_t tid;
    int rv;

    if (tp == NULL) {
        TP_ERROR("%s: null argument\r\n", __func__);
        return -EINVAL;
    }

    pthread_mutex_lock(&tp->lock);

    TP_ERROR("%s: worker die unexpectedly, restarting\r\n", __func__);

    rv = pthread_create(&tid, NULL,
            (void*(*)(void*))_worker_thread, tp);
    if (rv != 0) {
        TP_ERROR("%s: create thread failed cause %s\r\n", __func__, strerror(rv));
    }

    pthread_mutex_unlock(&tp->lock);

    return 0;
}

static int _worker_thread(tp_pool_t *tp)
{
    tp_work_t work;
    int rv;

    if (tp == NULL) {
        TP_ERROR("%s: null argument\r\n", __func__);
        return -EINVAL;
    }

    pthread_cleanup_push((void(*)(void*))_worker_guard, (void*)tp);

    pthread_mutex_lock(&tp->lock);

    tp->worker_count++;

    prctl(PR_SET_NAME, "threadpool worker");

    rv = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    if (rv != 0) {
        TP_ERROR("%s: setcancelstate failed cause %s\r\n", __func__, strerror(rv));
        goto exit;
    }

    rv = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    if (rv != 0) {
        TP_ERROR("%s: setcanceltype failed cause %s\r\n", __func__, strerror(rv));
        goto exit;
    }

    rv = pthread_detach(pthread_self());
    if (rv != 0) {
        TP_ERROR("%s: pthread_detach failed cause %s\r\n", __func__, strerror(rv));
        goto exit;
    }

    for (;;) {
        if (!tp->started)
            goto exit;

        work = _pull_work(tp);
        if (work.func) {
            pthread_mutex_unlock(&tp->lock);
            work.func(work.context, work.data);
            pthread_mutex_lock(&tp->lock);
            continue;
        }

        pthread_cond_wait(&tp->cond, &tp->lock);
    }

exit:
    if (tp->worker_count)
        tp->worker_count--;

    pthread_mutex_unlock(&tp->lock);

    pthread_cleanup_pop(0);

    pthread_exit(NULL);
}

int tp_queue_work(tp_pool_t *tp, tp_work_func func, void *context, unsigned data)
{
    tp_work_t work;

    if (tp == NULL || func == NULL) {
        TP_ERROR("%s: null argument\r\n", __func__);
        return -EINVAL;
    }

    work.func = func;
    work.context = context;
    work.data = data;

    pthread_mutex_lock(&tp->lock);

    if (tp->started) {
        _push_work(tp, work);
        pthread_cond_signal(&tp->cond);
    } else {
        TP_ERROR("%s: pool is stopped", __func__);
        pthread_mutex_unlock(&tp->lock);
        return -EINVAL;
    }

    pthread_mutex_unlock(&tp->lock);

    return 0;
}

int tp_pool_start(tp_pool_t *tp)
{
    int rv;

    if (tp == NULL) {
        TP_ERROR("%s: null argument\r\n", __func__);
        return -EINVAL;
    }

    pthread_mutex_lock(&tp->lock);

    for (int i = tp->worker_count; i < TP_WORKER_NUMBER; ++i) {
        pthread_t tid;
        rv = pthread_create(&tid, NULL, (void*(*)(void*))_worker_thread, tp);
        if (rv != 0) {
            TP_ERROR("%s: create thread failed cause %s\r\n", __func__, strerror(rv));
            goto out;
        }
    }

    tp->started = true;
    pthread_mutex_unlock(&tp->lock);

    return 0;

out:
    pthread_cond_broadcast(&tp->cond);
    pthread_mutex_unlock(&tp->lock);
    return -EPERM;
}

void tp_pool_stop(tp_pool_t *tp)
{
    if (tp == NULL) {
        TP_ERROR("%s: null argument\r\n", __func__);
        return;
    }

    pthread_mutex_lock(&tp->lock);

    tp->started = false;

    pthread_cond_broadcast(&tp->cond);
    pthread_mutex_unlock(&tp->lock);
    return;
}
