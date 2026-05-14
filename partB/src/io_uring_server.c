#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(__linux__) && defined(HAVE_LIBURING)

#include <errno.h>
#include <liburing.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef enum {
    REQ_ACCEPT,
    REQ_READ,
    REQ_WRITE
} req_type_t;

typedef struct {
    int fd;
    unsigned char *buf;
    size_t len;
    size_t off;
} conn_t;

typedef struct {
    req_type_t type;
    conn_t *conn;
} request_t;

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [port] [queue_depth] [buffer_size]\n", prog);
    fprintf(stderr, "Defaults: port=8083 queue_depth=256 buffer_size=4096\n");
}

static request_t *new_request(req_type_t type, conn_t *conn)
{
    request_t *req = malloc(sizeof(*req));

    if (req == NULL) {
        fatal_errno("malloc");
    }
    req->type = type;
    req->conn = conn;
    return req;
}

static void close_conn(conn_t *conn)
{
    if (conn == NULL) {
        return;
    }
    if (conn->fd >= 0) {
        close(conn->fd);
    }
    free(conn->buf);
    free(conn);
}

static void submit_accept(struct io_uring *ring, int listen_fd)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    request_t *req;

    if (sqe == NULL) {
        fatal_msg("io_uring submission queue is full while submitting accept");
    }
    req = new_request(REQ_ACCEPT, NULL);
    io_uring_prep_accept(sqe, listen_fd, NULL, NULL, 0);
    io_uring_sqe_set_data(sqe, req);
}

static void submit_read(struct io_uring *ring, conn_t *conn, int buffer_size)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    request_t *req;

    if (sqe == NULL) {
        close_conn(conn);
        return;
    }
    req = new_request(REQ_READ, conn);
    io_uring_prep_read(sqe, conn->fd, conn->buf, (unsigned)buffer_size, 0);
    io_uring_sqe_set_data(sqe, req);
}

static void submit_write(struct io_uring *ring, conn_t *conn)
{
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    request_t *req;

    if (sqe == NULL) {
        close_conn(conn);
        return;
    }
    req = new_request(REQ_WRITE, conn);
    io_uring_prep_write(sqe, conn->fd, conn->buf + conn->off,
                        (unsigned)(conn->len - conn->off), 0);
    io_uring_sqe_set_data(sqe, req);
}

int main(int argc, char **argv)
{
    int port = 8083;
    int queue_depth = 256;
    int buffer_size = 4096;
    int listen_fd;
    struct io_uring ring;
    int ret;

    if (argc > 4) {
        usage(argv[0]);
        return 2;
    }
    if (argc >= 2 && parse_int_arg(argv[1], 1, 65535, &port) != 0) {
        usage(argv[0]);
        return 2;
    }
    if (argc >= 3 && parse_int_arg(argv[2], 8, 32768, &queue_depth) != 0) {
        usage(argv[0]);
        return 2;
    }
    if (argc >= 4 && parse_int_arg(argv[3], 1, 1048576, &buffer_size) != 0) {
        usage(argv[0]);
        return 2;
    }

    ignore_sigpipe();

    listen_fd = create_listen_socket(port, 1024, 0);
    if (listen_fd < 0) {
        fatal_errno("create_listen_socket");
    }

    ret = io_uring_queue_init((unsigned)queue_depth, &ring, 0);
    if (ret < 0) {
        errno = -ret;
        fatal_errno("io_uring_queue_init");
    }

    submit_accept(&ring, listen_fd);
    io_uring_submit(&ring);

    printf("io_uring_server listening on port %d, queue_depth=%d, buffer_size=%d\n",
           port, queue_depth, buffer_size);
    fflush(stdout);

    while (1) {
        struct io_uring_cqe *cqe;
        request_t *req;

        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) {
            if (ret == -EINTR) {
                continue;
            }
            errno = -ret;
            fatal_errno("io_uring_wait_cqe");
        }

        req = io_uring_cqe_get_data(cqe);
        if (req == NULL) {
            io_uring_cqe_seen(&ring, cqe);
            continue;
        }

        if (req->type == REQ_ACCEPT) {
            int client_fd = cqe->res;

            submit_accept(&ring, listen_fd);
            if (client_fd >= 0) {
                conn_t *conn = calloc(1, sizeof(*conn));
                if (conn == NULL) {
                    close(client_fd);
                    fatal_errno("calloc");
                }
                conn->fd = client_fd;
                conn->buf = malloc((size_t)buffer_size);
                if (conn->buf == NULL) {
                    close_conn(conn);
                    fatal_errno("malloc");
                }
                submit_read(&ring, conn, buffer_size);
            } else if (client_fd != -EAGAIN && client_fd != -EINTR) {
                errno = -client_fd;
                perror("accept");
            }
        } else if (req->type == REQ_READ) {
            conn_t *conn = req->conn;

            if (cqe->res <= 0) {
                close_conn(conn);
            } else {
                conn->len = (size_t)cqe->res;
                conn->off = 0;
                submit_write(&ring, conn);
            }
        } else if (req->type == REQ_WRITE) {
            conn_t *conn = req->conn;

            if (cqe->res <= 0) {
                close_conn(conn);
            } else {
                conn->off += (size_t)cqe->res;
                if (conn->off < conn->len) {
                    submit_write(&ring, conn);
                } else {
                    submit_read(&ring, conn, buffer_size);
                }
            }
        }

        free(req);
        io_uring_cqe_seen(&ring, cqe);
        io_uring_submit(&ring);
    }
}

#else

int main(void)
{
    fprintf(stderr,
            "io_uring_server requires Linux with liburing development files.\n"
            "Install liburing-dev/liburing-devel and rebuild with make.\n");
    return 1;
}

#endif
