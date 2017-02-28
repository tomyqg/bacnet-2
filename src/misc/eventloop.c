/*
 * Copyright(C) 2014 SWG. All rights reserved.
 *
 * eventloop.c
 * Original Author:  lincheng, 2015-5-21
 *
 * Event loop
 *
 * History
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>
#include <sys/prctl.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>

#include "eventloop_def.h"
#include "debug.h"

el_loop_t el_default_loop = {};

bool el_dbg_verbos = false;
bool el_dbg_warn = false;
bool el_dbg_err = true;

static el_watch_impl_t *alloc_watch(el_loop_t *el)
{
    el_watch_impl_t *watch;
    
    if (list_empty(&el->free_watch_head)) {
        el->free_watch_count = 0;
        watch = (el_watch_impl_t *)malloc(sizeof(el_watch_impl_t));
        if (watch == NULL) {
            EL_ERROR("%s: malloc failed\r\n", __func__);
        } else {
            watch->base.handler = NULL;
            watch->base.data = 0;
        }
    } else {
        watch = list_first_entry(&el->free_watch_head, el_watch_impl_t, list);
        __list_del_entry(&watch->list);
        watch->base.handler = NULL;
        watch->base.data = 0;
        el->free_watch_count--;
    }
    
    return watch;
}

static void dealloc_watch(el_loop_t *el, el_watch_impl_t *watch)
{
    if (el->free_watch_count >= RESERVE_WATCH) {
        free(watch);
    } else {
        list_add(&watch->list, &el->free_watch_head);
        el->free_watch_count++;
    }
}

el_watch_t *el_watch_create(el_loop_t *el, int fd, int events)
{
    struct epoll_event ev;
    el_watch_impl_t *watch;
    int rv;

    if (el == NULL) {
        EL_ERROR("%s: invalid event loop\r\n", __func__);
        return NULL;
    }

    if (fd < 0) {
        EL_ERROR("%s: invalid file fd(%d)\r\n", __func__, fd);
        return NULL;
    }

    pthread_mutex_lock(&el->sync_lock);
    
    watch = alloc_watch(el);
    if (watch == NULL) {
        pthread_mutex_unlock(&el->sync_lock);
        EL_ERROR("%s: alloc watch failed\r\n", __func__);
        return NULL;
    }
    
    watch->fd = fd;
    ev.events = events;
    ev.data.ptr = watch;
    rv = epoll_ctl(el->epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    if (rv < 0) {
        EL_ERROR("%s: epoll_ctl add failed cause %s\r\n", __func__, strerror(errno));
        dealloc_watch(el, watch);
        pthread_mutex_unlock(&el->sync_lock);
        return NULL;
    }

    pthread_mutex_unlock(&el->sync_lock);
    
    return &watch->base;
}

int el_watch_mod(el_loop_t *el, el_watch_t *base, int events)
{
    struct epoll_event ev;
    el_watch_impl_t *watch = (el_watch_impl_t *)base;
    int rv;
    
    if (el == NULL) {
        EL_ERROR("%s: null event loop\r\n", __func__);
        return -EINVAL;
    }

    if (base == NULL) {
        EL_ERROR("%s: null watch\r\n", __func__);
        return -EINVAL;
    }

    if (watch->fd < 0) {
        EL_ERROR("%s: invalid watch fd(%d)\r\n", __func__, watch->fd);
        return -EINVAL;
    }
    
    pthread_mutex_lock(&el->sync_lock);

    ev.events = events;
    ev.data.ptr = base;
    rv = epoll_ctl(el->epoll_fd, EPOLL_CTL_MOD, watch->fd, &ev);
    if (rv < 0) {
        EL_ERROR("%s: epoll_ctl mod failed cause %s\r\n", __func__, strerror(errno));
    }

    pthread_mutex_unlock(&el->sync_lock);
    
    return rv;
}

int el_watch_destroy(el_loop_t *el, el_watch_t *base)
{
    el_watch_impl_t *watch = (el_watch_impl_t *)base;
    int rv;

    if (el == NULL) {
        EL_ERROR("%s: null event loop\r\n", __func__);
        return -EINVAL;
    }

    if (base == NULL) {
        EL_ERROR("%s: null watch\r\n", __func__);
        return -EINVAL;
    }

    if (watch->fd < 0) {
        EL_ERROR("%s: invalid watch fd(%d)\r\n", __func__, watch->fd);
        return -EINVAL;
    }
    
    pthread_mutex_lock(&el->sync_lock);

    rv = epoll_ctl(el->epoll_fd, EPOLL_CTL_DEL, watch->fd, NULL);
    if (rv < 0) {
        EL_ERROR("%s: epoll_ctl del failed cause %s\r\n", __func__, strerror(errno));
    }  else {
        watch->fd = -1;
        list_add_tail(&watch->list, &el->recy_watch_head);
        el->recy_watch_count++;
    }

    pthread_mutex_unlock(&el->sync_lock);
    
    return rv;
}

int el_watch_fd(el_watch_t *base)
{
    el_watch_impl_t *watch;

    if (base == NULL) {
        EL_ERROR("%s: null watch\r\n", __func__);
        return -EINVAL;
    }

    watch = (el_watch_impl_t *)base;
    
    return watch->fd;
}

void el_sync(el_loop_t *el)
{
    if (el == NULL) {
        EL_ERROR("%s: null event loop\r\n", __func__);
        return;
    }

    /* not need to sync */
    if (pthread_equal(pthread_self(), el->epoll_thread)) {
        return;
    }
    
    pthread_mutex_lock(&el->sync_lock);
    while (el->busy > 0) {
        pthread_cond_wait(&el->sync_cond, &el->sync_lock);
    }
    
    --el->busy;
    pthread_mutex_unlock(&el->sync_lock);
}

void el_unsync(el_loop_t *el)
{
    if (el == NULL) {
        EL_ERROR("%s: null event loop\r\n", __func__);
        return;
    }

    /* not need to sync */
    if (pthread_equal(pthread_self(), el->epoll_thread)) {
        return;
    }
    
    pthread_mutex_lock(&el->sync_lock);
    if (++el->busy == 0) {
        pthread_cond_signal(&el->sync_cond);
    }
    pthread_mutex_unlock(&el->sync_lock);
}

static void internal_add_timer(struct list_head *wheel, el_timer_impl_t *timer, unsigned next)
{
    unsigned expires = timer->expires;
    unsigned idx = expires - next;

    if (idx < TVR_SIZE) {
        wheel += expires & TVR_MASK;
    } else {
        int level = 0, i;
        idx >>= TVR_BITS;

        for (; idx >= TVN_SIZE && level < (TVN_NUMS - 1);) {
            idx >>= TVN_BITS;
            ++level;
        }

        i = (expires >> (TVR_BITS + TVN_BITS * level)) & TVN_MASK;
        wheel += TVR_SIZE + TVN_SIZE * level + i;
    }

    list_add_tail(&timer->list, wheel);
}

static void cascade(el_loop_t *el)
{
    /* cascade all the timers from tv up one level */
    el_timer_impl_t *timer, *tmp;
    struct list_head tv_list;
    int level = 0, index;

    do {
        index = (el->curr_tick >> (TVR_BITS + level * TVN_BITS)) & TVN_MASK;

        list_replace_init(el->wheel_list + TVR_SIZE + TVN_SIZE * level + index, &tv_list);

        /*
         * We are removing _all_ timers from the list, so we
         * don't have to detach them individually.
         */
        list_for_each_entry_safe(timer, tmp, &tv_list, list) {
            internal_add_timer(el->wheel_list, timer, el->curr_tick);
        }
    } while (index == 0 && ++level < TVN_NUMS);
}

static el_timer_impl_t *alloc_timer(el_loop_t *el)
{
    el_timer_impl_t *timer;
    
    if (list_empty(&el->free_timer_head)) {
        el->free_timer_count = 0;
        timer = (el_timer_impl_t *)malloc(sizeof(el_timer_impl_t));
        if (timer == NULL) {
            EL_ERROR("%s: not enough memory\r\n", __func__);
        } else {
            INIT_LIST_HEAD(&timer->list);
            timer->base.handler = NULL;
            timer->base.data = 0;
        }
    } else {
        timer = list_first_entry(&el->free_timer_head, el_timer_impl_t, free_list);
        __list_del_entry(&timer->free_list);
        INIT_LIST_HEAD(&timer->list);
        timer->base.handler = NULL;
        timer->base.data = 0;
        el->free_timer_count--;
    }
    
    return timer;
}

static void dealloc_timer(el_loop_t *el, el_timer_impl_t *timer)
{
    if (el->free_timer_count >= RESERVE_TIMER) {
        free(timer);
    } else {
        timer->list.next = NULL;
        timer->list.prev = NULL;
        list_add(&timer->free_list, &el->free_timer_head);
        el->free_timer_count++;
    }
}

static void _queue_timer(el_loop_t *el, el_timer_impl_t *timer, unsigned timeout)
{
    struct timespec ts;
    unsigned expire;
    int rv;

    rv = clock_gettime(CLOCK_MONOTONIC_COARSE, &ts);
    if (rv < 0) {
        EL_ERROR("%s: clock gettime failed cause %s\r\n", __func__, strerror(errno));
    }

    expire = timeout / TIMER_GRANULARITY;
    timeout = timeout % TIMER_GRANULARITY;
    expire += ts.tv_sec * (1000 / TIMER_GRANULARITY)
            + (timeout + ts.tv_nsec / 1000000 + TIMER_GRANULARITY - 1) / TIMER_GRANULARITY;
    timer->expires = expire;

    if (expire == el->curr_tick) {
        list_add_tail(&timer->list, &el->to_timer);
    } else {
        internal_add_timer(el->wheel_list, timer, el->curr_tick + 1);
    }
}

el_timer_t *el_timer_create(el_loop_t *el, unsigned timeout)
{
    el_timer_impl_t *timer;
    
    if (el == NULL) {
        EL_ERROR("%s: null event loop\r\n", __func__);
        return NULL;
    }
    
    pthread_mutex_lock(&el->sync_lock);

    timer = alloc_timer(el);
    if (timer == NULL) {
        pthread_mutex_unlock(&el->sync_lock);
        EL_ERROR("%s: alloc timer failed\r\n", __func__);
        return NULL;
    }

    _queue_timer(el, timer, timeout);
    
    pthread_mutex_unlock(&el->sync_lock);
    
    return &timer->base;
}

int el_timer_mod(el_loop_t *el, el_timer_t *base, unsigned timeout)
{
    el_timer_impl_t *timer = (el_timer_impl_t *)base;
    
    if (el == NULL) {
        EL_ERROR("%s: null event loop\r\n", __func__);
        return -EINVAL;
    }

    if (timer == NULL) {
        EL_ERROR("%s: null timer\r\n", __func__);
        return -EINVAL;
    }

    pthread_mutex_lock(&el->sync_lock);

    if (timer->list.next == NULL) {
        pthread_mutex_unlock(&el->sync_lock);
        EL_ERROR("%s: invalid timer\r\n", __func__);
        return -EPERM;
    }
    
    if (!list_empty(&timer->list)) {
        __list_del_entry(&timer->list);
    }

    _queue_timer(el, timer, timeout);

    pthread_mutex_unlock(&el->sync_lock);

    return 0;
}

int el_timer_destroy(el_loop_t *el, el_timer_t *base)
{
    el_timer_impl_t *timer = (el_timer_impl_t *)base;
    int rv;
    
    if (el == NULL) {
        EL_ERROR("%s: null event loop\r\n", __func__);
        return -EINVAL;
    }

    if (timer == NULL) {
        EL_ERROR("%s: null timer\r\n", __func__);
        return -EINVAL;
    }

    rv = 0;
    
    pthread_mutex_lock(&el->sync_lock);

    if (timer->list.next == NULL) {
        pthread_mutex_unlock(&el->sync_lock);
        EL_ERROR("%s: invalid timer\r\n", __func__);
        return -EPERM;
    }
    
    if (!list_empty(&timer->list)) {
        __list_del_entry(&timer->list);
        rv = 1;
    }
    dealloc_timer(el, timer);

    pthread_mutex_unlock(&el->sync_lock);
    
    return rv;
}

unsigned el_timer_expire(el_timer_t *base)
{
    el_timer_impl_t *timer = (el_timer_impl_t *)base;

    if (timer == NULL) {
        EL_ERROR("%s: null timer\r\n", __func__);
        return 0;
    }

    if (timer->list.next == NULL) {
        EL_ERROR("%s: invalid timer\r\n", __func__);
        return 0;
    }

    return timer->expires * TIMER_GRANULARITY;
}

unsigned el_current_second(void)
{
    struct timespec ts;
    int rv;
    
    rv = clock_gettime(CLOCK_MONOTONIC_COARSE, &ts);
    if (rv < 0) {
        EL_ERROR("%s: clock gettime failed cause %s\r\n", __func__, strerror(errno));
        return 0;
    }
    
    return ts.tv_sec;
}

unsigned el_current_millisecond(void)
{
    struct timespec ts;
    int rv;
    
    rv = clock_gettime(CLOCK_MONOTONIC_COARSE, &ts);
    if (rv < 0) {
        EL_ERROR("%s: clock gettime failed cause %s\r\n", __func__, strerror(errno));
        return 0;
    }

    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void recycle_watch(el_loop_t *el)
{
    el_watch_impl_t *watch;
    
    list_splice_tail_init(&el->recy_watch_head, &el->free_watch_head);
    el->free_watch_count += el->recy_watch_count;
    el->recy_watch_count = 0;

    while (el->free_watch_count > RESERVE_WATCH) {
        if (list_empty(&el->free_watch_head)) {
            watch = list_first_entry(&el->free_watch_head, el_watch_impl_t, list);
            __list_del_entry(&watch->list);
            free(watch);
            el->free_watch_count--;
        } else {
            EL_ERROR("%s: free watch empty but count report more\r\n", __func__);
            el->free_watch_count = 0;
        }
    }
}

/* run with lock */
static int timer_work(el_loop_t *el)
{
    struct list_head *wheel = el->wheel_list;
    struct timespec ts;
    unsigned past;
    int waitms;
    int index;
    int rv;
    
    rv = clock_gettime(CLOCK_MONOTONIC_COARSE, &ts);
    if (rv < 0) {
        EL_ERROR("%s: clock gettime failed cause %s\r\n", __func__, strerror(errno));
        return TIMER_GRANULARITY;
    }

    past = ts.tv_nsec / 1000000;
    waitms = past % TIMER_GRANULARITY;
    past = past / TIMER_GRANULARITY;

    past = past + ts.tv_sec * (1000 / TIMER_GRANULARITY) - el->curr_tick;

    while(past) {
        ++el->curr_tick;
        index = el->curr_tick & TVR_MASK;

         /*
         * Cascade timers:
         */
        if (!index) {
            cascade(el);
        }
        
        list_splice_tail_init(wheel + index, &el->to_timer);
        past--;
    }

    return waitms;
}

static void *event_loop_func(el_loop_t *el)
{
    struct epoll_event evlist[MAX_EVENTS];
    el_watch_impl_t *watch;
    el_watch_handler handler;
    int wait_ms;
    int nfds = 0;
    int rv;
    int i;

    prctl(PR_SET_NAME, "eventloop_pthr");

    rv = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    if (rv != 0) {
        EL_ERROR("%s: setcancelstate failed cause %s\r\n", __func__, strerror(rv));
        goto exit;
    }

    rv = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    if (rv != 0) {
        EL_ERROR("%s: setcanceltype failed cause %s\r\n", __func__, strerror(rv));
        goto exit;
    }

    rv = pthread_detach(pthread_self());
    if (rv != 0) {
        EL_ERROR("%s: pthread_detach failed cause %s\r\n", __func__, strerror(rv));
        goto exit;
    }

    for (;;) {
        pthread_mutex_lock(&el->sync_lock);
        while (el->busy) {
            pthread_cond_wait(&el->sync_cond, &el->sync_lock);
        }
        
        ++el->busy;
        if (!list_empty(&el->recy_watch_head)) {
            for (i = 0; i < nfds; ++i) {
                watch = (el_watch_impl_t *)(evlist[i].data.ptr);
                if (watch->fd == -1) {
                    evlist[i].data.ptr = NULL;
                }
            }
            
            recycle_watch(el);
        }

        for (i = 0; i < nfds; ++i) {
            watch = (el_watch_impl_t *)(evlist[i].data.ptr);
            if ((watch != NULL) && (watch->fd != -1)) {
                handler = watch->base.handler;
                if (handler == NULL) {
                    EL_WARN("%s: fd(%d) null handler\r\n", __func__, watch->fd);
                    continue;
                }

                pthread_mutex_unlock(&el->sync_lock);
                handler(&watch->base, evlist[i].events);
                pthread_mutex_lock(&el->sync_lock);
            }
        }

        for (;;) {
            wait_ms = timer_work(el);
            if (list_empty(&el->to_timer)) {
                break;
            }

            do {
                el_timer_impl_t *timer =
                        list_first_entry(&el->to_timer, el_timer_impl_t, list);
                el_timer_handler handler = timer->base.handler;

                list_del_init(&timer->list);
                if (handler == NULL) {
                    EL_WARN("%s: null handler\r\n", __func__);
                    continue;
                }

                pthread_mutex_unlock(&el->sync_lock);
                handler(&timer->base);
                pthread_mutex_lock(&el->sync_lock);
            } while (!list_empty(&el->to_timer));
        }

        if (!list_empty(&el->recy_watch_head)) {
            recycle_watch(el);
        }
        
        if (--el->busy == 0) {
            pthread_cond_broadcast(&el->sync_cond);
        }
        
        pthread_mutex_unlock(&el->sync_lock);

        nfds = epoll_wait(el->epoll_fd, evlist, MAX_EVENTS, wait_ms);
        if (nfds < 0) {
            if (errno == EINTR) {
                nfds = 0;
                continue;
            }
            
            EL_ERROR("%s: epoll wait failed cause %s\r\n", __func__, strerror(errno));
            break;
        }
    }

exit:
    pthread_exit(NULL);
}

int el_loop_init(el_loop_t *el)
{
    struct timespec ts;
    int i;
    int rv;
    
    if (el == NULL) {
        EL_ERROR("%s: null argument\r\n", __func__);
        return -EPERM;
    }

    if (el->inited) {
        EL_WARN("%s: duplicated init\r\n", __func__);
        return 0;
    }

    rv = pthread_mutex_init(&el->sync_lock, NULL);
    if (rv != 0) {
        EL_ERROR("%s: init mutex failed cause %s\r\n", __func__, strerror(rv));
        return -EPERM;
    }

    rv = pthread_cond_init(&el->sync_cond, NULL);
    if (rv != 0) {
        EL_ERROR("%s: init cond failed cause %s\r\n", __func__, strerror(rv));
        goto out0;
    }

    el->epoll_fd = epoll_create(MAX_EVENTS);
    if (el->epoll_fd < 0) {
        EL_ERROR("%s: create epoll failed cause %s\r\n", __func__, strerror(errno));
        goto out1;
    }

    el->busy = 0;
    el->started = 0;
    INIT_LIST_HEAD(&el->free_watch_head);
    INIT_LIST_HEAD(&el->recy_watch_head);
    INIT_LIST_HEAD(&el->free_timer_head);
    INIT_LIST_HEAD(&el->to_timer);
    el->free_watch_count = 0;
    el->recy_watch_count = 0;
    el->free_timer_count = 0;

    for (i = 0; i < (TVR_SIZE + TVN_SIZE * TVN_NUMS); ++i) {
        INIT_LIST_HEAD(&el->wheel_list[i]);
    }

    rv = clock_gettime(CLOCK_MONOTONIC_COARSE, &ts);
    if (rv < 0) {
        EL_ERROR("%s: clock gettime failed cause %s\r\n", __func__, strerror(errno));
        goto out2;
    }

    el->curr_tick = ts.tv_sec * (1000 / TIMER_GRANULARITY)
            + ts.tv_nsec / (1000000 * TIMER_GRANULARITY);
    el->inited = true;
    return 0;

out2:
    close(el->epoll_fd);

out1:
    pthread_cond_destroy(&el->sync_cond);

out0:
    pthread_mutex_destroy(&el->sync_lock);
    
    return -EPERM;
}

int el_loop_start(el_loop_t *el)
{
    int rv;

    if (el == NULL) {
        EL_ERROR("%s: invalid argument\r\n", __func__);
        return -EINVAL;
    }

    if (!el->inited) {
        EL_ERROR("%s: not init yet\r\n", __func__);
        return -EPERM;
    }

    rv = -EPERM;
    
    pthread_mutex_lock(&el->sync_lock);

    if (el->started) {
        EL_ERROR("%s: already started\r\n", __func__);
        goto out;
        rv = 0;
    }

    rv = pthread_create(&el->epoll_thread, NULL, (void*(*)(void*))event_loop_func, el);
    if (rv != 0) {
        EL_ERROR("%s: create thread failed cause %s\r\n", __func__, strerror(rv));
        rv = -EPERM;
        goto out;
    }

    el->started = 1;

out:
    pthread_mutex_unlock(&el->sync_lock);
    
    return rv;
}

void el_loop_exit(el_loop_t *el)
{
    /* TODO */
    EL_ERROR("%s: not implement yet.\r\n", __func__);
    return;
}

void el_set_dbg_level(uint32_t level)
{
    el_dbg_verbos = level & DEBUG_LEVEL_VERBOS;
    el_dbg_warn = level & DEBUG_LEVEL_WARN;
    el_dbg_err = level & DEBUG_LEVEL_ERROR;
}

