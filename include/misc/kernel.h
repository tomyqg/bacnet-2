/*
 * Copyright(C) 2014 SWG. All rights reserved.
 *
 * list.h
 * Original Author:  lincheng, 2015-6-4
 *
 * Double and single linked list
 *
 * History
 */

#ifndef _KERNEL_H_
#define _KERNEL_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * swap - swap value of @a and @b
 */
#define swap(a, b) \
    do { typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while (0)

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:    the pointer to the member.
 * @type:   the type of the container struct this is embedded in.
 * @member: the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({          \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type,member) );})

#ifdef __cplusplus
}
#endif

#endif  /* _KERNEL_H_ */

