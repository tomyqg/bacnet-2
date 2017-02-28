/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * trendlog_def.h
 * Original Author:  linzhixian, 2015-12-29
 *
 * History
 */

#ifndef _TRENDLOG_DEF_H_
#define _TRENDLOG_DEF_H_

/*
 * Data types associated with a BACnet Log Record. We use these for managing the log buffer 
 * but they are also the tag numbers to use when encoding/decoding the log datum field.
 */
#define TL_TYPE_STATUS      (0)
#define TL_TYPE_BOOL        (1)
#define TL_TYPE_REAL        (2)
#define TL_TYPE_ENUM        (3)
#define TL_TYPE_UNSIGN      (4)
#define TL_TYPE_SIGN        (5)
#define TL_TYPE_BITS        (6)
#define TL_TYPE_NULL        (7)
#define TL_TYPE_ERROR       (8)
#define TL_TYPE_DELTA       (9)
#define TL_TYPE_ANY         (10)        /* We don't support this particular can of worms! */

#define TL_T_START_WILD     (1)         /* Start time is wild carded */
#define TL_T_STOP_WILD      (2)         /* Stop Time is wild carded */

#define TL_MAX_ENTRIES      (1000)      /* Entries per datalog */

#define TRENDLOG_HASH_BITS  (8)

#endif /* _TRENDLOG_DEF_H_ */

