/*
 * utils.c
 *
 *  Created on: Apr 29, 2016
 *      Author: lin
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pthread.h>
#include <linux/version.h>

#include <misc/cJSON.h>
#include <misc/utils.h>

/* read a file to text */
char *read_file_to_text(const char *filename)
{
    FILE *fd;
    long len;
    size_t nread;
    char *data;
    int rv;

    if (filename == NULL) {
        printf("%s: invalid argument\r\n", __func__);
        return NULL;
    }

    fd = fopen(filename, "rb");
    if (fd == NULL) {
        printf("%s: fopen %s failed(%s)\r\n", __func__, filename, strerror(errno));
        return NULL;
    }

    rv = fseek(fd, 0, SEEK_END);
    if (rv < 0) {
        printf("%s: fseek %s failed(%s)\r\n", __func__, filename, strerror(errno));
        goto err;
    }

    len = ftell(fd);
    if (len < 0) {
        printf("%s: ftell %s failed(%s)\r\n", __func__, filename, strerror(errno));
        goto err;
    }

    rv = fseek(fd, 0, SEEK_SET);
    if (rv < 0) {
        printf("%s: fseek %s failed(%s)\r\n", __func__, filename, strerror(errno));
        goto err;
    }

    data = (char *)malloc(len + 1);
    if (data == NULL) {
        printf("%s: malloc failed(%ld)\r\n", __func__, len + 1);
        goto err;
    }
    data[len] = 0;

    nread = fread(data, 1, len, fd);
    if (nread != len) {
        printf("%s: fread %s want %ld byte(s) but only %u\r\n", __func__, filename, len, nread);
        free(data);
        goto err;
    }

    /* ensure no null in the text */
    nread = strlen(data);
    if (nread != len) {
        printf("%s: %s terminated at %d by null byte\r\n", __func__, filename, nread);
        free(data);
        goto err;
    }

    fclose(fd);
    return data;

err:
    fclose(fd);
    
    return NULL;
}

cJSON *load_json_file(const char *filename)
{
    cJSON *root;
    char *text;

    text = read_file_to_text(filename);
    if (text == NULL) {
        printf("%s: read %s failed\r\n", __func__, filename);
        return NULL;
    }

    root = cJSON_Parse(text);
    free(text);
    if (!root) {
        printf("%s: cJSON Parse %s error before [%s]\r\n", __func__, filename, cJSON_GetErrorPtr());
        return NULL;
    }

    return root;
}

int hexstr2bytes(char *out, unsigned len, const char *str)
{
    unsigned i;
    
    len <<= 1;
    for(i = 0; i < len; ++i) {
        uint8_t v = str[i];
        if (v >= '0' && v <= '9') {
            v -= '0';
        } else if (v >= 'a' && v <= 'f') {
            v -= 'a' - 10;
        } else if (v >= 'A' && v <= 'F') {
            v -= 'A' - 10;
        } else if (v || (i & 1)) {
            return -1;
        } else {
            break;
        }


        if (i & 1) {
            out[i >> 1] += v;
        } else {
            out[i >> 1] = v << 4;
        }
    }

    return i >> 1;
}

int save_json_file(cJSON *json, const char *filename)
{
    if (!json || !filename) {
        printf("%s: null arguments\r\n", __func__);
        return -EINVAL;
    }

    char *jstr = cJSON_Print(json);
    if (!jstr) {
        printf("%s: print json failed\r\n", __func__);
        return -EPERM;
    }

    size_t len = strlen(jstr);

    int fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (fd == -1) {
        printf("%s: open %s failed(%s)\r\n", __func__, filename, strerror(errno));
        free(jstr);
        return -EPERM;
    }

    size_t written = 0;
    while (written < len) {
        int nwrite = write(fd, jstr + written, len - written);
        if (nwrite < 0) {
            if (errno == EINTR) {
                continue;
            }
            
            printf("%s: write %s failed(%s)\r\n", __func__, filename, strerror(errno));
            close(fd);
            free(jstr);
            return -EPERM;
        }
        written += nwrite;
    }

    close(fd);
    free(jstr);

    return 0;
}

pid_t exec_getpid(const char * const argv[], int *infd, int *outfd, int *errfd)
{
    pid_t child;
    int inpipes[2], outpipes[2], errpipes[2];

    if (infd) {
        if (pipe(inpipes)) {
            printf("%s: pipe for input failed(%s)\n", __func__, strerror(errno));
            return -1;
        }
    }

    if (outfd) {
        if (pipe(outpipes)) {
            printf("%s: pipe for output failed(%s)\n", __func__, strerror(errno));
            goto err0;
        }
    }

    if (errfd) {
        if (pipe(errpipes)) {
            printf("%s: pipe for error failed(%s)\n", __func__, strerror(errno));
            goto err1;
        }
    }

    if ((child = fork()) == 0) { // child
        if (infd) {
            close(inpipes[1]);
            if (dup2(inpipes[0], 0) < 0) {
                fprintf(stderr, "%s: subprocess dup2 stdin failed(%s)\n", __func__, strerror(errno));
                exit(1);
            }
            close(inpipes[0]);
        }
        
        if (outfd) {
            close(outpipes[0]);
            if (dup2(outpipes[1], 1) < 0) {
                fprintf(stderr, "%s: subprocess dup2 stdout failed(%s)\n", __func__, strerror(errno));
                exit(1);
            }
            close(outpipes[1]);
        }
        
        if (errfd) {
            close(errpipes[0]);
            if (dup2(errpipes[1], 2) < 0) {
                fprintf(stderr, "%s: subprocess dup2 stderr failed(%s)\n", __func__, strerror(errno));
                exit(1);
            }
            close(errpipes[1]);
        }
        
        execvp(argv[0], (char* const*)argv);
        /* should never go here */
        fprintf(stderr, "%s: execvp failed(%s)\n", __func__, strerror(errno));
        exit(1);
    } else if (child < 0) {
        printf("%s: fork new process failed.\n", __func__);
        goto err2;
    }

    if (infd) {
        close(inpipes[0]);
        *infd = inpipes[1];
    }
    
    if (outfd) {
        close(outpipes[1]);
        *outfd = outpipes[0];
    }
    
    if (errfd) {
        close(errpipes[1]);
        *errfd = errpipes[0];
    }
    
    return child;

err2:
    if (errfd) {
        close(errpipes[0]);
        close(errpipes[1]);
    }

err1:
    if (outfd) {
        close(outpipes[0]);
        close(outpipes[1]);
    }

err0:
    if (infd) {
        close(inpipes[0]);
        close(inpipes[1]);
    }

    return -1;
}

int pid_waitexit(pid_t pid, int *status)
{
    if (pid <= 0) {
        printf("%s: invalid subprocess pid_t(%d)\n", __func__, pid);
        return -1;
    }

    for (;;) {
        int rv = waitpid(pid, status, 0);
        if (rv == -1) {
            if (errno == EINTR) {
                continue;
            }
            printf("%s: wait child failed(%s)\n", __func__, strerror(errno));
            return -1;
        }
        break;
    }

    return 0;
}

char *get_run_result(const char * const argv[])
{
    int infd;
    int size;
    int len;
    char *buf;
    int status;
    pid_t pid;

    if (!argv) {
        printf("%s: null argument\n", __func__);
        return NULL;
    }

    if (!argv[0]) {
        printf("%s: no exec file\n", __func__);
        return NULL;
    }

    pid = exec_getpid(argv, NULL, &infd, NULL);
    if (pid <= 0) {
        printf("%s: exec_getpid failed:", __func__);
        goto err0;
    }

    size = 1024;
    buf = (char *)malloc(size);
    len = 0;

    if (!buf) {
        printf("%s: not enough memory:", __func__);
        goto err1;
    }

    for (;;) {
        int nread = read(infd, buf + len, size - (len + 1));
        if (nread < 0) {
           if (errno == EINTR) {
               continue;
            }
            printf("%s: read failed(%s):", __func__, strerror(errno));
            goto err1;
        }

        if (nread == 0) {
            break;
        }
        
        if (strnlen(buf+len, nread) != (size_t)nread) {
            printf("%s: read null:", __func__);
            goto err1;
        }

        len += nread;
        if (len >= (size - 1)) {
            size += 4096;
            char *newbuf = (char *)realloc(buf, size);
            if (!newbuf) {
                printf("%s: not enough memory:", __func__);
                goto err1;
            }
            buf = newbuf;
        }
    }

    close(infd);
    buf = (char*)realloc(buf, len + 1);
    buf[len] = 0;

    if (pid_waitexit(pid, &status) < 0) {
        free(buf);
        printf("%s: pid_waitexit failed:", __func__);
        goto err0;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        free(buf);
        printf("%s: abnormal exit:", __func__);
        goto err0;
    }

    return buf;

err1:
    if (buf) {
        free(buf);
    }

    close(infd);
    if (pid_waitexit(pid, NULL) < 0) {
        printf(" pid_waitexit failed:");
    }

err0:
    for (int i = 0; argv[i]; ++i) {
        printf(" %s", argv[i]);
    }
    printf("\n");

    return NULL;
}

#if  LINUX_VERSION_CODE < KERNEL_VERSION(3,17,0)

static int urandom_fd = -1;
static pthread_once_t random_is_initialized = PTHREAD_ONCE_INIT;

static void initialize_random(void)
{
    urandom_fd = open("/dev/urandom", O_RDONLY);
    if (urandom_fd == -1) {
        printf("%s: open /dev/urandom fail: %s\r\n", __func__,
                strerror(errno));
        return;
    }
}

/*
 * get random number into buf. read from /dev/urandom, should not block
 * return >= 0 success, < 0 error.
 */
int get_random(uint8_t *buf, size_t len)
{
    int rv;

    if (len == 0)
        return 0;

    if (buf == NULL) {
        printf("%s: null argument\r\n", __func__);
        return -EINVAL;
    }

    pthread_once(&random_is_initialized, initialize_random);

    rv = read(urandom_fd, buf, len);
    if (rv > 0)
        return rv;

    if (rv == 0) {
        printf("%s: EOF\r\n", __func__);
    } else {
        printf("%s: read fail: %s", __func__, strerror(errno));
    }

    return -EPERM;
}
#else
#include <syscall.h>
#include <linux/random.h>

/*
 * get random number into buf. use getrandom syscall introduced from kernel 3.17
 * return >= 0 success, < 0 error.
 */
int get_random(uint8_t *buf, size_t len)
{
    int rv;

    if (len == 0)
        return 0;

    if (buf == NULL) {
        printf("%s: null argument\r\n", __func__);
        return -EINVAL;
    }

    rv = syscall(SYS_getrandom, buf, len, GRND_NONBLOCK);

    if (rv == -1) {
        if (errno == EAGAIN) {
            for (unsigned i = 0; i < len; ++i) {
                buf[i] = random();
            }

            return len;
        }

        printf("%s: read random fail: %s\r\n", __func__, strerror(errno));
        return -EPERM;
    }

    return rv;
}
#endif

uint16_t cal_retry_timeout(uint16_t timeout, unsigned past,
        uint16_t min, uint16_t max, uint16_t inc)
{
    if (max == 0) {
        max = 1;
    }

    if (min > max) {
        min = max;
    }

    if (timeout < min) {
        timeout = min;
    } else if (timeout > max) {
        timeout = max;
    }

    if (past >= max) {
        unsigned dec = (long long unsigned)inc * (past - max) / max;
        if (dec > timeout - min) {
            return min;
        }
        return timeout - dec;
    } else if (timeout == max) {
        return timeout;
    }

    inc = inc * (max - past) / (max - timeout);
    if (inc <= 1) {
        timeout++;
    } else if (max - timeout <= inc){
        timeout = max;
    } else {
        timeout += inc;
    }

    return timeout;
}

