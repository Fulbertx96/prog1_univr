#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>

#define SERVER_FIFO "/tmp/fifo_in"

#define MAX_PATH_LEN   1024
#define MAX_FIFO_LEN   128
#define MAX_HASH_LEN   65
#define MAX_MSG_SIZE   2048

#define MAX_CACHE_SIZE 100
#define MAX_QUEUE      100
#define MAX_THREADS    4

typedef struct {
    char path[MAX_PATH_LEN];
    char reply_fifo[MAX_FIFO_LEN];
    char digest[MAX_HASH_LEN];
    off_t size;
} FileRequest;

#endif