/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * config.h
 * Original Author:  linzhixian, 2015-1-15
 *
 * Bacnet Config
 *
 * History
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

#define MAX_IP_NPDU         (1497)
#define MAX_ETH_NPDU        (1497)
#define MAX_MSTP_NPDU       (1497)
#define MAX_PTP_NPDU        (501)
#define MAX_ARC_NPDU        (501)
#define MAX_LON_NPDU        (228)

#define MIN_NPDU            (228)

#define MIN_APDU            (50)
#define MAX_IP_APDU         (1476)
#define MAX_ETH_APDU        (1476)
#define MAX_LON_APDU        (206)
#define MAX_MSTP_APDU       (1476)
#define MAX_ARC_APDU        (480)
#define MAX_PTP_APDU        (480)

#define MAX_APDU            MAX_IP_APDU

#endif  /* _CONFIG_H_ */

