/*
 * Copyright(C) 2014 SWG. All rights reserved.
 *
 * bits.h
 * Original Author:  lincheng, 2015-6-4
 *
 * Bits manipulating
 *
 * History
 */

#ifndef BITS_H
#define BITS_H

#include <stdint.h>

/********************************************************************
* Bit Masks
*********************************************************************/
#define BIT(x)        (1<<(x))
#define BIT0            (0x01)
#define BIT1            (0x02)
#define BIT2            (0x04)
#define BIT3            (0x08)
#define BIT4            (0x10)
#define BIT5            (0x20)
#define BIT6            (0x40)
#define BIT7            (0x80)
#define BIT8          (0x0100)
#define BIT9          (0x0200)
#define BIT10         (0x0400)
#define BIT11         (0x0800)
#define BIT12         (0x1000)
#define BIT13         (0x2000)
#define BIT14         (0x4000)
#define BIT15         (0x8000)
#define BIT16       (0x010000UL)
#define BIT17       (0x020000UL)
#define BIT18       (0x040000UL)
#define BIT19       (0x080000UL)
#define BIT20       (0x100000UL)
#define BIT21       (0x200000UL)
#define BIT22       (0x400000UL)
#define BIT23       (0x800000UL)
#define BIT24     (0x01000000UL)
#define BIT25     (0x02000000UL)
#define BIT26     (0x04000000UL)
#define BIT27     (0x08000000UL)
#define BIT28     (0x10000000UL)
#define BIT29     (0x20000000UL)
#define BIT30     (0x40000000UL)
#define BIT31     (0x80000000UL)

/* a=register, b=bit number to act upon 0-n */
#define BIT_SET(a,b) ((a) |= (1<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1<<(b)))
#define BIT_FLIP(a,b) ((a) ^= (1<<(b)))
#define BIT_CHECK(a,b) ((a) & (1<<(b)))

/* x=target variable, y=mask */
#define BITMASK_SET(x,y) ((x) |= (y))
#define BITMASK_CLEAR(x,y) ((x) &= (~(y)))
#define BITMASK_FLIP(x,y) ((x) ^= (y))
#define BITMASK_CHECK(x,y) ((x) & (y))

#ifndef _BV
#define _BV(x) (1<<(x))
#endif

static inline int get_bit(uint8_t *buf, unsigned offset)
{
    return buf[offset >> 3] & (1 << (offset & 7));
}

static inline void set_bit(uint8_t *buf, unsigned offset)
{
    buf[offset >> 3] |= 1 << (offset & 7);
}

static inline void clear_bit(uint8_t *buf, unsigned offset)
{
    buf[offset >> 3] &= ~(1 << (offset & 7));
}

#endif

