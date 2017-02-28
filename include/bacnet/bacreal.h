/*
 * Copyright(C) 2014 SWG. All rights reserved.
 *
 * bacreal.h
 * Original Author:  lincheng, 2015-6-4
 *
 * BACnet real and double
 *
 * History
 */

#ifndef BACREAL_H
#define BACREAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    int decode_real(
        const uint8_t * apdu,
        float *real_value);
    int encode_real(
        float value,
        uint8_t * apdu);
    int decode_double(
        const uint8_t * apdu,
        double *real_value);
    int encode_double(
        double value,
        uint8_t * apdu);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
