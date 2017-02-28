/*
 * utils.h
 *
 *  Created on: Apr 29, 2016
 *      Author: lin
 */

#ifndef INCLUDE_MISC_UTILS_H_
#define INCLUDE_MISC_UTILS_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "misc/cJSON.h"

/**
 * @return NULL if fail
 */
extern char *read_file_to_text(const char *filename);

/**
 * @return NULL if fail
 */
extern cJSON *load_json_file(const char *filename);

/**
 * @return >=0 if success, <0 if error
 */
extern int save_json_file(cJSON *json, const char *filename);

/**
 * decode hex string into bytes
 * @param out, out buffer
 * @param len, len of out buffer
 * @param str, hex string
 * @return bytes of out set by str, if any error return -1
 */
extern int hexstr2bytes(char *out, unsigned len, const char *str);

/**
 * create subprocess and exec argv, redirect stdin/stdout/stderr
 * @param argv, program and arguments
 * @param infd, input fd of subprocess, null if not re-direct
 * @param outfd, output fd of subprocess, null if not re-direct
 * @param errfd, error fd of subprocess, null if not re-direct
 * @return >0 pid_t of subprocess, <=0 error
 */
extern pid_t exec_getpid(const char * const argv[], int *infd, int *outfd, int *errfd);

/**
 * wait subprocess end
 * @param pid, subprocess pid_t
 * @param status, return exit status, if not care set to NULL
 * @return >=0 as success, <0 if fail
 */
extern int pid_waitexit(pid_t pid, int *status);

/**
 * create subprocess and exec argv, return stdout output string
 * @param argv, program and arguments
 * @return stdout output string if subprocess normally exit, it should be
 * freed lately. return NULL if any error.
 */
extern char *get_run_result(const char * const argv[]);

/*
 * get random number into buf. read from /dev/urandom, should not block
 * return >= 0 success, < 0 error.
 */
extern int get_random(uint8_t *buf, size_t len);

/* ����retry timeout��ͨ���߼�, ���ݱ���past�� �����´�timeout
 * @timeout, ԭ����timeoutʱ��
 * @past, ����retry��ʱ����
 * @min, ��С��timeout
 * @max, ����timeout
 * @inc, ����ԭ����timeout��������retry���´�timeout�����ӵ�ʱ��
 * @return, �����´�timeout��ʱ��
 */
extern uint16_t cal_retry_timeout(uint16_t timeout, unsigned past,
        uint16_t min, uint16_t max, uint16_t inc);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_MISC_UTILS_H_ */

