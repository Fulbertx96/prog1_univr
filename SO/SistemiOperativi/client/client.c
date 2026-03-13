#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "common.h"
#include "client.h"

static int compare_size(const void *a, const void *b) {
    const FileRequest *r1 = (const FileRequest *)a;
    const FileRequest *r2 = (const FileRequest *)b;

    if (r1->size < r2->size) return -1;
    if (r1->size > r2->size) return 1;
    return 0;
}

static int create_reply_fifo(char *fifo_name, size_t size, int index) {
    snprintf(fifo_name, size, "/tmp/resp_%d_%d", getpid(), index);

    if (mkfifo(fifo_name, 0666) < 0) {
        perror("mkfifo risposta");
        return -1;
    }

    return 0;
}

static int send_request_to_server(const char *filepath, const char *reply_fifo) {
    int fd_out = open(SERVER_FIFO, O_WRONLY);
    if (fd_out < 0) {
        perror("Errore apertura FIFO server");
        return -1;
    }

    char msg[MAX_MSG_SIZE];
    snprintf(msg, sizeof(msg), "%s::%s", filepath, reply_fifo);

    if (write(fd_out, msg, strlen(msg) + 1) < 0) {
        perror("write FIFO server");
        close(fd_out);
        return -1;
    }

    close(fd_out);
    return 0;
}

static int receive_digest(const char *reply_fifo, char *digest, size_t size) {
    int fd_in = open(reply_fifo, O_RDONLY);
    if (fd_in < 0) {
        perror("Errore lettura FIFO client");
        strncpy(digest, "ERRORE", size - 1);
        digest[size - 1] = '\0';
        return -1;
    }

    ssize_t r = read(fd_in, digest, size - 1);
    if (r <= 0) {
        strncpy(digest, "ERRORE", size - 1);
        digest[size - 1] = '\0';
        close(fd_in);
        return -1;
    }

    digest[r] = '\0';
    close(fd_in);
    return 0;
}

int run_client(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso corretto: %s <file1> [file2 ...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    int count = argc - 1;
    FileRequest *entries = calloc(count, sizeof(FileRequest));
    if (entries == NULL) {
        perror("calloc");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < count; i++) {
        strncpy(entries[i].path, argv[i + 1], sizeof(entries[i].path) - 1);
        entries[i].path[sizeof(entries[i].path) - 1] = '\0';

        if (create_reply_fifo(entries[i].reply_fifo, sizeof(entries[i].reply_fifo), i) != 0) {
            free(entries);
            return EXIT_FAILURE;
        }

        struct stat info;
        if (stat(entries[i].path, &info) < 0) {
            perror("stat");
            unlink(entries[i].reply_fifo);
            free(entries);
            return EXIT_FAILURE;
        }

        entries[i].size = info.st_size;

        if (send_request_to_server(entries[i].path, entries[i].reply_fifo) != 0) {
            unlink(entries[i].reply_fifo);
            free(entries);
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < count; i++) {
        receive_digest(entries[i].reply_fifo, entries[i].digest, sizeof(entries[i].digest));
        unlink(entries[i].reply_fifo);
    }

    qsort(entries, count, sizeof(FileRequest), compare_size);

    printf("\n=== HASH RICEVUTI ===\n");
    for (int i = 0; i < count; i++) {
        printf("File: %s\n", entries[i].path);
        printf("Dimensione: %ld byte\n", (long)entries[i].size);
        printf("SHA-256: %s\n\n", entries[i].digest);
    }

    free(entries);
    return EXIT_SUCCESS;
}