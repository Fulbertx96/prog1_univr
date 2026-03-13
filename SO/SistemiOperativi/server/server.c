#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <openssl/sha.h>
#include <sys/stat.h>
#include <stdint.h>
#include <errno.h>

#include "common.h"
#include "server.h"

typedef struct {
    char filepath[MAX_PATH_LEN];
    char hash_string[MAX_HASH_LEN];
} CacheEntry;

typedef struct {
    char request_str[MAX_MSG_SIZE];
    off_t filesize;
} Request;

typedef struct {
    char filepath[MAX_PATH_LEN];
    int done;
    int wait_count;
    char hash_string[MAX_HASH_LEN];
    pthread_cond_t cond;
} InProgressEntry;

static CacheEntry cache[MAX_CACHE_SIZE];
static int cache_size = 0;
static pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;

static Request request_queue[MAX_QUEUE];
static int queue_size = 0;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;

static InProgressEntry in_progress_list[MAX_QUEUE];
static int in_progress_count = 0;

static pthread_mutex_t thread_count_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t thread_available = PTHREAD_COND_INITIALIZER;
static int active_threads = 0;

static void digest_file(const char *filename, uint8_t *hash) {
    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    int file = open(filename, O_RDONLY);
    if (file == -1) {
        perror("open file");
        memset(hash, 0, 32);
        return;
    }

    unsigned char buffer[1024];
    ssize_t bytes;

    while ((bytes = read(file, buffer, sizeof(buffer))) > 0) {
        SHA256_Update(&ctx, buffer, bytes);
    }

    close(file);
    SHA256_Final(hash, &ctx);
}

static void cache_insert_unlocked(const char *path, const char *hash) {
    for (int i = 0; i < cache_size; i++) {
        if (strcmp(cache[i].filepath, path) == 0) {
            return;
        }
    }

    if (cache_size < MAX_CACHE_SIZE) {
        strncpy(cache[cache_size].filepath, path, sizeof(cache[cache_size].filepath) - 1);
        cache[cache_size].filepath[sizeof(cache[cache_size].filepath) - 1] = '\0';

        strncpy(cache[cache_size].hash_string, hash, sizeof(cache[cache_size].hash_string) - 1);
        cache[cache_size].hash_string[sizeof(cache[cache_size].hash_string) - 1] = '\0';

        cache_size++;
    }
}

static int cache_lookup(const char *path, char *hash_out) {
    pthread_mutex_lock(&cache_mutex);

    for (int i = 0; i < cache_size; i++) {
        if (strcmp(cache[i].filepath, path) == 0) {
            strcpy(hash_out, cache[i].hash_string);
            pthread_mutex_unlock(&cache_mutex);
            return 1;
        }
    }

    pthread_mutex_unlock(&cache_mutex);
    return 0;
}

static int find_in_progress_index(const char *path) {
    for (int i = 0; i < in_progress_count; i++) {
        if (strcmp(in_progress_list[i].filepath, path) == 0 &&
            in_progress_list[i].done == 0) {
            return i;
        }
    }
    return -1;
}

static void enqueue_request(const char *request_str, off_t filesize) {
    pthread_mutex_lock(&queue_mutex);

    if (queue_size >= MAX_QUEUE) {
        fprintf(stderr, "Coda piena. Richiesta scartata.\n");
        pthread_mutex_unlock(&queue_mutex);
        return;
    }

    int i = queue_size;
    while (i > 0) {
        if (request_queue[i - 1].filesize > filesize ||
            (request_queue[i - 1].filesize == filesize &&
             strcmp(request_queue[i - 1].request_str, request_str) > 0)) {
            request_queue[i] = request_queue[i - 1];
            i--;
        } else {
            break;
        }
    }

    strncpy(request_queue[i].request_str, request_str, sizeof(request_queue[i].request_str) - 1);
    request_queue[i].request_str[sizeof(request_queue[i].request_str) - 1] = '\0';
    request_queue[i].filesize = filesize;

    queue_size++;

    pthread_cond_signal(&queue_not_empty);
    pthread_mutex_unlock(&queue_mutex);
}

static int dequeue_request(Request *out) {
    pthread_mutex_lock(&queue_mutex);

    while (queue_size == 0) {
        pthread_cond_wait(&queue_not_empty, &queue_mutex);
    }

    *out = request_queue[0];

    for (int i = 1; i < queue_size; i++) {
        request_queue[i - 1] = request_queue[i];
    }

    queue_size--;

    pthread_mutex_unlock(&queue_mutex);
    return 1;
}

static void send_response_to_client(const char *fifo_path, const char *hash_string) {
    int fd_out = open(fifo_path, O_WRONLY);
    if (fd_out >= 0) {
        write(fd_out, hash_string, strlen(hash_string) + 1);
        close(fd_out);
    } else {
        perror("open FIFO client per risposta");
    }
}

static void *handle_request(void *arg) {
    char *input = (char *)arg;
    char *sep = strstr(input, "::");

    if (sep == NULL) {
        fprintf(stderr, "Richiesta malformata: %s\n", input);
        goto cleanup;
    }

    *sep = '\0';
    char *filepath = input;
    char *fifo_path = sep + 2;

    char hash_string[MAX_HASH_LEN] = "";
    int need_compute = 0;

    if (cache_lookup(filepath, hash_string)) {
        need_compute = 0;
    } else {
        need_compute = 1;
    }

    if (need_compute) {
        pthread_mutex_lock(&cache_mutex);

        int idx = find_in_progress_index(filepath);

        if (idx != -1) {
            in_progress_list[idx].wait_count++;

            while (in_progress_list[idx].done == 0) {
                pthread_cond_wait(&in_progress_list[idx].cond, &cache_mutex);
            }

            strcpy(hash_string, in_progress_list[idx].hash_string);
            in_progress_list[idx].wait_count--;

            if (in_progress_list[idx].done == 1 &&
                in_progress_list[idx].wait_count == 0) {
                pthread_cond_destroy(&in_progress_list[idx].cond);
                in_progress_list[idx] = in_progress_list[in_progress_count - 1];
                in_progress_count--;
            }

            pthread_mutex_unlock(&cache_mutex);
        } else {
            if (in_progress_count >= MAX_QUEUE) {
                pthread_mutex_unlock(&cache_mutex);
                fprintf(stderr, "Troppe richieste in corso.\n");
                goto cleanup;
            }

            int new_idx = in_progress_count++;

            strncpy(in_progress_list[new_idx].filepath, filepath,
                    sizeof(in_progress_list[new_idx].filepath) - 1);
            in_progress_list[new_idx].filepath[sizeof(in_progress_list[new_idx].filepath) - 1] = '\0';

            in_progress_list[new_idx].done = 0;
            in_progress_list[new_idx].wait_count = 0;
            in_progress_list[new_idx].hash_string[0] = '\0';
            pthread_cond_init(&in_progress_list[new_idx].cond, NULL);

            pthread_mutex_unlock(&cache_mutex);

            uint8_t hash[32];
            digest_file(filepath, hash);

            for (int i = 0; i < 32; i++) {
                sprintf(hash_string + (i * 2), "%02x", hash[i]);
            }
            hash_string[64] = '\0';

            pthread_mutex_lock(&cache_mutex);

            cache_insert_unlocked(filepath, hash_string);

            int comp_idx = find_in_progress_index(filepath);
            if (comp_idx != -1) {
                in_progress_list[comp_idx].done = 1;
                strcpy(in_progress_list[comp_idx].hash_string, hash_string);

                if (in_progress_list[comp_idx].wait_count > 0) {
                    pthread_cond_broadcast(&in_progress_list[comp_idx].cond);
                } else {
                    pthread_cond_destroy(&in_progress_list[comp_idx].cond);
                    in_progress_list[comp_idx] = in_progress_list[in_progress_count - 1];
                    in_progress_count--;
                }
            }

            pthread_mutex_unlock(&cache_mutex);
        }
    }

    send_response_to_client(fifo_path, hash_string);

cleanup:
    free(input);

    pthread_mutex_lock(&thread_count_mutex);
    active_threads--;
    pthread_cond_signal(&thread_available);
    pthread_mutex_unlock(&thread_count_mutex);

    return NULL;
}

static void *dispatcher_thread(void *arg) {
    (void)arg;

    char buffer[MAX_MSG_SIZE];
    int fd_in = open(SERVER_FIFO, O_RDWR);

    if (fd_in < 0) {
        perror("open FIFO_IN");
        exit(1);
    }

    while (1) {
        ssize_t len = read(fd_in, buffer, sizeof(buffer) - 1);
        if (len <= 0) {
            continue;
        }

        buffer[len] = '\0';

        char *ptr = buffer;

        while (ptr < buffer + len) {
            size_t rem = strlen(ptr);

            if (rem == 0) {
                ptr++;
                continue;
            }

            char *sep = strstr(ptr, "::");
            if (sep == NULL) {
                fprintf(stderr, "Richiesta malformata: %s\n", ptr);
                break;
            }

            *sep = '\0';
            char *filepath = ptr;
            char *fifo_client = sep + 2;

            char combined[MAX_MSG_SIZE];
            snprintf(combined, sizeof(combined), "%s::%s", filepath, fifo_client);

            struct stat st;
            if (stat(filepath, &st) < 0) {
                perror("stat file");
                ptr += rem + 1;
                continue;
            }

            enqueue_request(combined, st.st_size);

            pthread_mutex_lock(&thread_count_mutex);
            while (active_threads >= MAX_THREADS) {
                pthread_cond_wait(&thread_available, &thread_count_mutex);
            }
            active_threads++;
            pthread_mutex_unlock(&thread_count_mutex);

            Request req;
            if (dequeue_request(&req)) {
                char *thread_arg = strdup(req.request_str);
                if (thread_arg == NULL) {
                    perror("strdup");
                    pthread_mutex_lock(&thread_count_mutex);
                    active_threads--;
                    pthread_cond_signal(&thread_available);
                    pthread_mutex_unlock(&thread_count_mutex);
                } else {
                    pthread_t tid;
                    pthread_create(&tid, NULL, handle_request, thread_arg);
                    pthread_detach(tid);
                }
            }

            ptr += rem + 1;
        }
    }

    close(fd_in);
    return NULL;
}

int run_server(void) {
    unlink(SERVER_FIFO);

    if (mkfifo(SERVER_FIFO, 0666) < 0) {
        perror("mkfifo");
        return 1;
    }

    printf("Server in ascolto su %s...\n", SERVER_FIFO);

    pthread_t dispatcher;
    pthread_create(&dispatcher, NULL, dispatcher_thread, NULL);
    pthread_join(dispatcher, NULL);

    unlink(SERVER_FIFO);
    return 0;
}