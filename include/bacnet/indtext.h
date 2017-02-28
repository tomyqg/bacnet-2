/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * indtext.h
 * Original Author:  linzhixian, 2015-3-16
 *
 *
 * History
 */

#ifndef _INDTEXT_H_
#define _INDTEXT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* index and text pairs */
typedef struct {
    uint32_t index;             /* index number that matches the text */
    const char *pString;        /* text pair - use NULL to end the list */
} INDTEXT_DATA;

extern const char *indtext_by_index_default(INDTEXT_DATA *data_list, uint32_t index, 
                    const char *default_string);

extern const char *indtext_by_index_split_default(INDTEXT_DATA *data_list, uint32_t index, 
                    uint32_t split_index, const char *before_split_default_name,
                    const char *default_name);

#ifdef __cplusplus
}
#endif

#endif  /* _INDTEXT_H_ */

