/*
 * Copyright(C) 2014 SWG. All rights reserved.
 *
 * eventloop.h
 * Original Author:  lincheng, 2015-5-20
 *
 * Event loop
 *
 * History
 */

#ifndef _EVENTLOOP_H_
#define _EVENTLOOP_H_

#include <stdint.h>
#include <sys/epoll.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct el_watch_s el_watch_t;

/**
 * �¼��ص���������
 *
 * @watch: el_watch_t*���¼���������ָ��
 * @events: ��ǰ�¼�, �¼����ͼ�epoll.h
 *
 * @return: void
 */
typedef void (*el_watch_handler)(el_watch_t *watch, int events);

struct el_watch_s {
    el_watch_handler handler;
    void *data;
};

typedef struct el_timer_s el_timer_t;

/**
 * ��ʱ���ص���������
 *
 * @timer: el_timer_t*����ʱ����ָ��
 *
 * @return: void
 */
typedef void (*el_timer_handler)(el_timer_t *timer);

struct el_timer_s {
    el_timer_handler handler;
    void *data;
};

struct el_loop_s;
typedef struct el_loop_s el_loop_t;

/*
 * ϵͳȱʡ��event loop������δ����
 */
extern el_loop_t el_default_loop;

/**
 * el_watch_create - ע���¼��ص�����
 *
 * @el: ָ��el_loop_t����
 * @fd: �¼��������ļ����
 * @handler: el_watch_handler*, ָ����ָ���ָ��
 * @events: �¼����ͣ���epoll.h
 *
 * @return: el_watch_t*, ���󷵻�NULL;
 */
extern el_watch_t *el_watch_create(el_loop_t *el, int fd, int events);

/**
 * el_watch_mod - �޸��¼��ص�����
 *
 * @el: ָ��el_loop_t����
 * @watch: el_watch_create���ص�event_watch_t*
 * @events: �¼����ͣ���epoll.h
 *
 * @return: ����<0
 */
extern int el_watch_mod(el_loop_t *el, el_watch_t *watch, int events);

/**
 * el_watch_del - ע���¼��ص�����
 *
 * @el: ָ��el_loop_t����
 * @watch: el_watch_add���ص�el_watch_t*
 * @return: ����<0
 */
extern int el_watch_destroy(el_loop_t *el, el_watch_t *watch);

extern int el_watch_fd(el_watch_t *watch);

extern void el_sync(el_loop_t *el);

extern void el_unsync(el_loop_t *el);

extern el_timer_t* el_timer_create(el_loop_t *el, unsigned timeout_ms);

extern int el_timer_mod(el_loop_t *el, el_timer_t *timer, unsigned timeout_ms);

/*
 * return 0 if timer not queued, >0 if timer queued. <0 if error
 */
extern int el_timer_destroy(el_loop_t *el, el_timer_t *timer);

extern unsigned el_timer_expire(el_timer_t *timer);

extern unsigned el_current_second(void);

extern unsigned el_current_millisecond(void);

extern el_loop_t* el_loop_create(void);

extern void el_loop_destroy(el_loop_t *el);

/*
 * el_loop_init - ��ʼ��event loop����
 * @el: event_loop_t*, what to initialize
 * @return int, >=0 success, <0 fail
 */
extern int el_loop_init(el_loop_t *el);

/*
 * el_loop_start - ����event loop�߳�
 *
 * @el: ָ��event loop����
 * @return: �ɹ�����0����0Ϊ����
 */
extern int el_loop_start(el_loop_t *el);

extern void el_loop_exit(el_loop_t *el);

extern void el_set_dbg_level(uint32_t level);

#ifdef __cplusplus
}
#endif

#endif /* _EVENTLOOP_H_ */

