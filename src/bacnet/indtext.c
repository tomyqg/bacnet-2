/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * indtext.c
 * Original Author:  linzhixian, 2015-3-13
 *
 *
 * History
 */

#include <stdlib.h>

#include "bacnet/indtext.h"

const char *indtext_by_index_default(INDTEXT_DATA *data_list, uint32_t index, 
                const char *default_string)
{
    const char *pString = NULL;

    if (data_list) {
        while (data_list->pString) {
            if (data_list->index == index) {
                pString = data_list->pString;
                break;
            }
            data_list++;
        }
    }

    return pString ? pString : default_string;
}

const char *indtext_by_index_split_default(INDTEXT_DATA *data_list, uint32_t index, 
                uint32_t split_index, const char *before_split_default_name,
                const char *default_name)
{
    if (index < split_index) {
        return indtext_by_index_default(data_list, index, before_split_default_name);
    } else {
        return indtext_by_index_default(data_list, index, default_name);
    }
}

