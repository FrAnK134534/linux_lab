#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef __linux__

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [port] [max_events] [buffer_size]\n", prog);
    fprintf(stderr, "Defaults: port=8082 max_events=1024 buffer_size=4096\n");
}

static void accept_ready_clients(int epoll_fd, int listen_fd)
{
    while (1) {
        int client_fd = accept(listen_fd, NULL, NULL);
        struct epoll_event ev;

        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            perror("accept");
            return;
        }

        if (set_nonblocking(client_fd) != 0) {
            perror("set_nonblocking");
            close(client_fd);
            continue;
        }

        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN | EPOLLRDHUP;
        ev.data.fd = client_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) != 0) {
            perror("epoll_ctl add client");
            close(client_fd);
        }
    }
}

static int write_back_nonblocking(int fd, const unsigned char *buf, size_t len)
{
    size_t written = 0;

    while (written < len) {
        ssize_t n = write(fd, buf + written, len - written);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                struct pollfd pfd;

                pfd.fd = fd;
                pfd.events = POLLOUT;
                pfd.revents = 0;
                while (poll(&pfd, 1, -1) < 0) {
                    if (errno != EINTR) {
                        return -1;
                    }
                }
                continue;
            }
            return -1;
        }
        if (n == 0) {
            errno = EIO;
            return -1;
        }
        written += (size_t)n;
    }

    return 0;
}

static int handle_client_read(int fd, unsigned char *buf, int buffer_size)
{
    while (1) {
        ssize_t nread = retrying_read(fd, buf, (size_t)buffer_size);
        if (nread < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;
            }
            return -1;
        }
        if (nread == 0) {
            return -1;
        }
        if (write_back_nonblocking(fd, buf, (size_t)nread) != 0) {
            return -1;
        }
    }
}

int main(int argc, char **argv)
{
    int port = 8082;
    int max_events = 1024;
    int buffer_size = 4096;
    int listen_fd;
    int epoll_fd;
    struct epoll_event listen_event;
    struct epoll_event *events;
    unsigned char *buf;

    if (argc > 4) {
        usage(argv[0]);
        return 2;
    }
    if (argc >= 2 && parse_int_arg(argv[1], 1, 65535, &port) != 0) {
        usage(argv[0]);
        return 2;
    }
    if (argc >= 3 && parse_int_arg(argv[2], 1, 100000, &max_events) != 0) {
        usage(argv[0]);
        return 2;
    }
    if (argc >= 4 && parse_int_arg(argv[3], 1, 1048576, &buffer_size) != 0) {
        usage(argv[0]);
        return 2;
    }

    ignore_sigpipe();

    listen_fd = create_listen_socket(port, 1024, 1);
    if (listen_fd < 0) {
        fatal_errno("create_listen_socket");
    }

    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd < 0) {
        fatal_errno("epoll_create1");
    }

    memset(&listen_event, 0, sizeof(listen_event));
    listen_event.events = EPOLLIN;
    listen_event.data.fd = listen_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &listen_event) != 0) {
        fatal_errno("epoll_ctl add listen");
    }

    events = calloc((size_t)max_events, sizeof(*events));
    buf = malloc((size_t)buffer_size);
    if (events == NULL || buf == NULL) {
        fatal_errno("malloc");
    }

    printf("epoll_server listening on port %d, max_events=%d, buffer_size=%d\n",
           port, max_events, buffer_size);
    fflush(stdout);

    while (1) {
        int ready = epoll_wait(epoll_fd, events, max_events, -1);
        int i;

        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            fatal_errno("epoll_wait");
        }

        for (i = 0; i < ready; i++) {
            int fd = events[i].data.fd;
            uint32_t ev = events[i].events;

            if (fd == listen_fd) {
                accept_ready_clients(epoll_fd, listen_fd);
                continue;
            }

            if (ev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
                continue;
            }

            if ((ev & EPOLLIN) && handle_client_read(fd, buf, buffer_size) != 0) {
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
            }
        }
    }
}

#else

int main(void)
{
    fprintf(stderr, "epoll_server is Linux-only because epoll is a Linux API.\n");
    return 1;
}

#endif
