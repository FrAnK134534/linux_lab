#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct {
    int *items;
    int capacity;
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} fd_queue_t;

typedef struct {
    fd_queue_t *queue;
    int buffer_size;
} worker_arg_t;

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [port] [workers] [queue_capacity] [buffer_size]\n", prog);
    fprintf(stderr, "Defaults: port=8080 workers=4 queue_capacity=4096 buffer_size=4096\n");
}

static int queue_init(fd_queue_t *q, int capacity)
{
    q->items = calloc((size_t)capacity, sizeof(q->items[0]));
    if (q->items == NULL) {
        return -1;
    }
    q->capacity = capacity;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    if (pthread_mutex_init(&q->mutex, NULL) != 0) {
        return -1;
    }
    if (pthread_cond_init(&q->not_empty, NULL) != 0) {
        return -1;
    }
    if (pthread_cond_init(&q->not_full, NULL) != 0) {
        return -1;
    }
    return 0;
}

static void queue_push(fd_queue_t *q, int fd)
{
    pthread_mutex_lock(&q->mutex);
    while (q->count == q->capacity) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }
    q->items[q->tail] = fd;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

static int queue_pop(fd_queue_t *q)
{
    int fd;

    pthread_mutex_lock(&q->mutex);
    while (q->count == 0) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }
    fd = q->items[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return fd;
}

static void handle_connection(int fd, unsigned char *buf, int buffer_size)
{
    while (1) {
        ssize_t nread = retrying_read(fd, buf, (size_t)buffer_size);
        if (nread < 0) {
            return;
        }
        if (nread == 0) {
            return;
        }
        if (write_full_blocking(fd, buf, (size_t)nread) != 0) {
            return;
        }
    }
}

static void *worker_main(void *arg)
{
    worker_arg_t *worker = arg;
    unsigned char *buf = malloc((size_t)worker->buffer_size);

    if (buf == NULL) {
        fatal_errno("malloc");
    }

    while (1) {
        int fd = queue_pop(worker->queue);
        handle_connection(fd, buf, worker->buffer_size);
        close(fd);
    }
}

int main(int argc, char **argv)
{
    int port = 8080;
    int workers = 4;
    int queue_capacity = 4096;
    int buffer_size = 4096;
    int listen_fd;
    fd_queue_t queue;
    pthread_t *threads;
    worker_arg_t worker_arg;
    int i;

    if (argc > 5) {
        usage(argv[0]);
        return 2;
    }
    if (argc >= 2 && parse_int_arg(argv[1], 1, 65535, &port) != 0) {
        usage(argv[0]);
        return 2;
    }
    if (argc >= 3 && parse_int_arg(argv[2], 1, 4096, &workers) != 0) {
        usage(argv[0]);
        return 2;
    }
    if (argc >= 4 && parse_int_arg(argv[3], 1, 100000, &queue_capacity) != 0) {
        usage(argv[0]);
        return 2;
    }
    if (argc >= 5 && parse_int_arg(argv[4], 1, 1048576, &buffer_size) != 0) {
        usage(argv[0]);
        return 2;
    }

    ignore_sigpipe();
    listen_fd = create_listen_socket(port, 1024, 0);
    if (listen_fd < 0) {
        fatal_errno("create_listen_socket");
    }
    if (queue_init(&queue, queue_capacity) != 0) {
        fatal_errno("queue_init");
    }

    threads = calloc((size_t)workers, sizeof(*threads));
    if (threads == NULL) {
        fatal_errno("calloc");
    }

    worker_arg.queue = &queue;
    worker_arg.buffer_size = buffer_size;
    for (i = 0; i < workers; i++) {
        if (pthread_create(&threads[i], NULL, worker_main, &worker_arg) != 0) {
            fatal_errno("pthread_create");
        }
    }

    printf("threadpool_server listening on port %d, workers=%d, queue_capacity=%d, buffer_size=%d\n",
           port, workers, queue_capacity, buffer_size);
    fflush(stdout);

    while (1) {
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            fatal_errno("accept");
        }
        queue_push(&queue, client_fd);
    }
}
