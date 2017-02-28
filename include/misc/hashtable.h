/*
 * Copyright(C) 2014 SWG. All rights reserved.
 *
 * hashtable.h
 * Original Author:  lincheng, 2015-6-4
 *
 * Statically sized hash table implementation
 *
 * History
 */

#ifndef _LINUX_HASHTABLE_H
#define _LINUX_HASHTABLE_H

#include "misc/list.h"
#include "hash.h"

#define DEFINE_HASHTABLE(name, bits)						\
	struct hlist_head name[1 << (bits)] =					\
			{ [0 ... ((1 << (bits)) - 1)] = HLIST_HEAD_INIT }

#define DECLARE_HASHTABLE(name, bits)                                   	\
	struct hlist_head name[1 << (bits)]

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define HASH_SIZE(name) (ARRAY_SIZE(name))
#define HASH_BITS(name) (     \
        (HASH_SIZE(name)) & (1ULL << 63) ? 63 :   \
        (HASH_SIZE(name)) & (1ULL << 62) ? 62 :   \
        (HASH_SIZE(name)) & (1ULL << 61) ? 61 :   \
        (HASH_SIZE(name)) & (1ULL << 60) ? 60 :   \
        (HASH_SIZE(name)) & (1ULL << 59) ? 59 :   \
        (HASH_SIZE(name)) & (1ULL << 58) ? 58 :   \
        (HASH_SIZE(name)) & (1ULL << 57) ? 57 :   \
        (HASH_SIZE(name)) & (1ULL << 56) ? 56 :   \
        (HASH_SIZE(name)) & (1ULL << 55) ? 55 :   \
        (HASH_SIZE(name)) & (1ULL << 54) ? 54 :   \
        (HASH_SIZE(name)) & (1ULL << 53) ? 53 :   \
        (HASH_SIZE(name)) & (1ULL << 52) ? 52 :   \
        (HASH_SIZE(name)) & (1ULL << 51) ? 51 :   \
        (HASH_SIZE(name)) & (1ULL << 50) ? 50 :   \
        (HASH_SIZE(name)) & (1ULL << 49) ? 49 :   \
        (HASH_SIZE(name)) & (1ULL << 48) ? 48 :   \
        (HASH_SIZE(name)) & (1ULL << 47) ? 47 :   \
        (HASH_SIZE(name)) & (1ULL << 46) ? 46 :   \
        (HASH_SIZE(name)) & (1ULL << 45) ? 45 :   \
        (HASH_SIZE(name)) & (1ULL << 44) ? 44 :   \
        (HASH_SIZE(name)) & (1ULL << 43) ? 43 :   \
        (HASH_SIZE(name)) & (1ULL << 42) ? 42 :   \
        (HASH_SIZE(name)) & (1ULL << 41) ? 41 :   \
        (HASH_SIZE(name)) & (1ULL << 40) ? 40 :   \
        (HASH_SIZE(name)) & (1ULL << 39) ? 39 :   \
        (HASH_SIZE(name)) & (1ULL << 38) ? 38 :   \
        (HASH_SIZE(name)) & (1ULL << 37) ? 37 :   \
        (HASH_SIZE(name)) & (1ULL << 36) ? 36 :   \
        (HASH_SIZE(name)) & (1ULL << 35) ? 35 :   \
        (HASH_SIZE(name)) & (1ULL << 34) ? 34 :   \
        (HASH_SIZE(name)) & (1ULL << 33) ? 33 :   \
        (HASH_SIZE(name)) & (1ULL << 32) ? 32 :   \
        (HASH_SIZE(name)) & (1ULL << 31) ? 31 :   \
        (HASH_SIZE(name)) & (1ULL << 30) ? 30 :   \
        (HASH_SIZE(name)) & (1ULL << 29) ? 29 :   \
        (HASH_SIZE(name)) & (1ULL << 28) ? 28 :   \
        (HASH_SIZE(name)) & (1ULL << 27) ? 27 :   \
        (HASH_SIZE(name)) & (1ULL << 26) ? 26 :   \
        (HASH_SIZE(name)) & (1ULL << 25) ? 25 :   \
        (HASH_SIZE(name)) & (1ULL << 24) ? 24 :   \
        (HASH_SIZE(name)) & (1ULL << 23) ? 23 :   \
        (HASH_SIZE(name)) & (1ULL << 22) ? 22 :   \
        (HASH_SIZE(name)) & (1ULL << 21) ? 21 :   \
        (HASH_SIZE(name)) & (1ULL << 20) ? 20 :   \
        (HASH_SIZE(name)) & (1ULL << 19) ? 19 :   \
        (HASH_SIZE(name)) & (1ULL << 18) ? 18 :   \
        (HASH_SIZE(name)) & (1ULL << 17) ? 17 :   \
        (HASH_SIZE(name)) & (1ULL << 16) ? 16 :   \
        (HASH_SIZE(name)) & (1ULL << 15) ? 15 :   \
        (HASH_SIZE(name)) & (1ULL << 14) ? 14 :   \
        (HASH_SIZE(name)) & (1ULL << 13) ? 13 :   \
        (HASH_SIZE(name)) & (1ULL << 12) ? 12 :   \
        (HASH_SIZE(name)) & (1ULL << 11) ? 11 :   \
        (HASH_SIZE(name)) & (1ULL << 10) ? 10 :   \
        (HASH_SIZE(name)) & (1ULL <<  9) ?  9 :   \
        (HASH_SIZE(name)) & (1ULL <<  8) ?  8 :   \
        (HASH_SIZE(name)) & (1ULL <<  7) ?  7 :   \
        (HASH_SIZE(name)) & (1ULL <<  6) ?  6 :   \
        (HASH_SIZE(name)) & (1ULL <<  5) ?  5 :   \
        (HASH_SIZE(name)) & (1ULL <<  4) ?  4 :   \
        (HASH_SIZE(name)) & (1ULL <<  3) ?  3 :   \
        (HASH_SIZE(name)) & (1ULL <<  2) ?  2 :   \
        (HASH_SIZE(name)) & (1ULL <<  1) ?  1 :   \
        0 )

/* Use hash_32 when possible to allow for fast 32bit hashing in 64bit kernels. */
#define hash_min(val, bits)							\
	(sizeof(val) <= 4 ? hash_32(val, bits) : hash_long(val, bits))

static inline void __hash_init(struct hlist_head *ht, unsigned int sz)
{
	unsigned int i;

	for (i = 0; i < sz; i++)
		INIT_HLIST_HEAD(&ht[i]);
}

/**
 * hash_init - initialize a hash table
 * @hashtable: hashtable to be initialized
 *
 * Calculates the size of the hashtable from the given parameter, otherwise
 * same as hash_init_size.
 *
 * This has to be a macro since HASH_BITS() will not work on pointers since
 * it calculates the size during preprocessing.
 */
#define hash_init(hashtable) __hash_init(hashtable, HASH_SIZE(hashtable))

/**
 * hash_add - add an object to a hashtable
 * @hashtable: hashtable to add to
 * @node: the &struct hlist_node of the object to be added
 * @key: the key of the object to be added
 */
#define hash_add(hashtable, node, key)						\
	hlist_add_head(node, &hashtable[hash_min(key, HASH_BITS(hashtable))])

/**
 * hash_hashed - check whether an object is in any hashtable
 * @node: the &struct hlist_node of the object to be checked
 */
static inline bool hash_hashed(struct hlist_node *node)
{
	return !hlist_unhashed(node);
}

static inline bool __hash_empty(struct hlist_head *ht, unsigned int sz)
{
	unsigned int i;

	for (i = 0; i < sz; i++)
		if (!hlist_empty(&ht[i]))
			return false;

	return true;
}

/**
 * hash_empty - check whether a hashtable is empty
 * @hashtable: hashtable to check
 *
 * This has to be a macro since HASH_BITS() will not work on pointers since
 * it calculates the size during preprocessing.
 */
#define hash_empty(hashtable) __hash_empty(hashtable, HASH_SIZE(hashtable))

/**
 * hash_del - remove an object from a hashtable
 * @node: &struct hlist_node of the object to remove
 */
static inline void hash_del(struct hlist_node *node)
{
	hlist_del_init(node);
}

/**
 * hash_for_each - iterate over a hashtable
 * @name: hashtable to iterate
 * @bkt: integer to use as bucket loop cursor
 * @obj: the type * to use as a loop cursor for each entry
 * @member: the name of the hlist_node within the struct
 */
#define hash_for_each(name, bkt, obj, member)				\
	for ((bkt) = 0, obj = NULL; obj == NULL && (bkt) < HASH_SIZE(name);\
			(bkt)++)\
		hlist_for_each_entry(obj, &name[bkt], member)

/**
 * hash_for_each_safe - iterate over a hashtable safe against removal of
 * hash entry
 * @name: hashtable to iterate
 * @bkt: integer to use as bucket loop cursor
 * @tmp: a &struct used for temporary storage
 * @obj: the type * to use as a loop cursor for each entry
 * @member: the name of the hlist_node within the struct
 */
#define hash_for_each_safe(name, bkt, obj, tmp, member)			\
	for ((bkt) = 0, obj = NULL; obj == NULL && (bkt) < HASH_SIZE(name);\
			(bkt)++)\
		hlist_for_each_entry_safe(obj, tmp, &name[bkt], member)

/**
 * hash_for_each_possible - iterate over all possible objects hashing to the
 * same bucket
 * @name: hashtable to iterate
 * @obj: the type * to use as a loop cursor for each entry
 * @member: the name of the hlist_node within the struct
 * @key: the key of the objects to iterate over
 */
#define hash_for_each_possible(name, obj, member, key)			\
	hlist_for_each_entry(obj, &name[hash_min(key, HASH_BITS(name))], member)

/**
 * hash_for_each_possible_safe - iterate over all possible objects hashing to the
 * same bucket safe against removals
 * @name: hashtable to iterate
 * @obj: the type * to use as a loop cursor for each entry
 * @tmp: a &struct used for temporary storage
 * @member: the name of the hlist_node within the struct
 * @key: the key of the objects to iterate over
 */
#define hash_for_each_possible_safe(name, obj, tmp, member, key)	\
	hlist_for_each_entry_safe(obj, tmp,\
		&name[hash_min(key, HASH_BITS(name))], member)

#endif

