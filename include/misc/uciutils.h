/*
 * uciutils.h
 *
 *  Created on: Apr 30, 2016
 *      Author: lin
 */

#ifndef INCLUDE_MISC_UCIUTILS_H_
#define INCLUDE_MISC_UCIUTILS_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>

bool uci_has(void);

bool uci_loadIP(struct in_addr *ip,
        struct in_addr *mask, struct in_addr *router);
void uci_saveIP(struct in_addr ip, struct in_addr mask,
        struct in_addr router, bool dhcp);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_MISC_UCIUTILS_H_ */
