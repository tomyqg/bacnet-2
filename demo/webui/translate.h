/*
 * Copyright(C) 2014 SWG. All rights reserved.
 */
/*
 * translate.h
 * Original Author:  lincheng, 2016-5-23
 *
 * History
 */

#pragma once

#ifndef _TRANSLATE_H_
#define _TRANSLATE_H_

#ifdef __cplusplus
extern "C"
{
#endif

#define TLT(msg)        translate(msg, locale)

extern const char *translate(const char *str, const char *locale);

#ifdef __cplusplus
}
#endif

#endif  /* _TRANSLATE_H_ */

