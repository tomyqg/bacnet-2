/*
 * mstpcommon.c
 *
 *  Created on: Nov 12, 2015
 *      Author: lin
 */

#include "bacnet/mstp.h"
#include "debug.h"

bool mstp_dbg_verbos = true;
bool mstp_dbg_warn = true;
bool mstp_dbg_err = true;

/**
 * mstp_set_dbg_level - 设置调试状态
 *
 * @level: 调试开关
 *
 * @return: void
 *
 */
void mstp_set_dbg_level(uint32_t level)
{
    mstp_dbg_verbos = level & DEBUG_LEVEL_VERBOS;
    mstp_dbg_warn = level & DEBUG_LEVEL_WARN;
    mstp_dbg_err = level & DEBUG_LEVEL_ERROR;
}

/**
 * mstp_show_dbg_status - 查看调试状态信息
 *
 * @return: void
 *
 */
void mstp_show_dbg_status(void)
{
    printf("mstp_dbg_verbos: %d\r\n", mstp_dbg_verbos);
    printf("mstp_dbg_warn: %d\r\n", mstp_dbg_warn);
    printf("mstp_dbg_err: %d\r\n", mstp_dbg_err);
}

/**
 * return MSTP_B_MAX if invalid baudrate
 */
MSTP_BAUDRATE mstp_baudrate2enum(uint32_t baudrate)
{
    switch (baudrate) {
    case 9600:
        return MSTP_B9600;
    case 19200:
        return MSTP_B19200;
    case 38400:
        return MSTP_B38400;
    case 57600:
        return MSTP_B57600;
    case 76800:
        return MSTP_B76800;
    case 115200:
        return MSTP_B115200;
    default:
        return MSTP_B_MAX;
    }
}

/*
 * return -1 if invalid baud
 */
int mstp_enum2baudrate(MSTP_BAUDRATE baud)
{
    switch (baud) {
    case MSTP_B9600:
        return 9600;
    case MSTP_B19200:
        return 19200;
    case MSTP_B38400:
        return 38400;
    case MSTP_B57600:
        return 57600;
    case MSTP_B76800:
        return 76800;
    case MSTP_B115200:
        return 115200;
    default:
        return -1;
    }
}

