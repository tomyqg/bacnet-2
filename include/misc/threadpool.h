/*
 * threadpool.h
 *
 *  Created on: Apr 22, 2016
 *      Author: lin
 */

#ifndef INCLUDE_MISC_THREADPOOL_H_
#define INCLUDE_MISC_THREADPOOL_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tp_pool_s tp_pool_t;

typedef void (*tp_work_func)(void* context, unsigned data);

extern tp_pool_t tp_default_pool;

extern int tp_queue_work(tp_pool_t *tp, tp_work_func work, void *context, unsigned data);

extern int tp_pool_start(tp_pool_t *tp);
extern void tp_pool_stop(tp_pool_t *tp);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_MISC_THREADPOOL_H_ */
