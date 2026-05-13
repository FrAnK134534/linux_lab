#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [port] [max_clients] [buffer_size]\n", prog);
    fprintf(stderr, "Defaults: port=8080 max_clients=1024 buffer_size=4096\n");
}

static int wait_writable_and_write(int fd, const unsigned char *buf, size_t len)
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
                if (poll(&pfd, 1, -1) < 0 && errno != EINTR) {
                    return -1;
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

static void close_client(struct pollfd *pfds, int index, int *nfds)
{
    close(pfds[index].fd);
    pfds[index] = pfds[*nfds - 1];
    (*nfds)--;
}

static void accept_ready_clients(int listen_fd, struct pollfd *pfds, int *nfds, int max_fds)
{
    while (*nfds < max_fds) {
        int client_fd = accept(listen_fd, NULL, NULL);
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
        pfds[*nfds].fd = client_fd;
        pfds[*nfds].events = POLLIN;
        pfds[*nfds].revents = 0;
        (*nfds)++;
    }
}

int main(int argc, char **argv)
{
    int port = 8080;
    int max_clients = 1024;
    int buffer_size = 4096;
    int listen_fd;
    int nfds = 1;
    int max_fds;
    struct pollfd *pfds;
    unsigned char *buf;

    if (argc > 4) {
        usage(argv[0]);
        return 2;
    }
    if (argc >= 2 && parse_int_arg(argv[1], 1, 65535, &port) != 0) {
        usage(argv[0]);
        return 2;
    }
    if (argc >= 3 && parse_int_arg(argv[2], 1, 100000, &max_clients) != 0) {
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

    max_fds = max_clients + 1;
    pfds = calloc((size_t)max_fds, sizeof(*pfds));
    buf = malloc((size_t)buffer_size);
    if (pfds == NULL || buf == NULL) {
        fatal_errno("malloc");
    }

    pfds[0].fd = listen_fd;
    pfds[0].events = POLLIN;

    printf("poll_server listening on port %d, max_clients=%d, buffer_size=%d\n",
           port, max_clients, buffer_size);
    fflush(stdout);

    while (1) {
        int ready = poll(pfds, (nfds_t)nfds, -1);
        int i;

        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            fatal_errno("poll");
        }

        if (pfds[0].revents & POLLIN) {
            accept_ready_clients(listen_fd, pfds, &nfds, max_fds);
        }
        pfds[0].revents = 0;

        for (i = 1; i < nfds;) {
            short events = pfds[i].revents;

            if (events == 0) {
                i++;
                continue;
            }
            if (events & (POLLERR | POLLHUP | POLLNVAL)) {
                close_client(pfds, i, &nfds);
                continue;
            }
            if (events & POLLIN) {
                while (1) {
                    ssize_t nread = retrying_read(pfds[i].fd, buf, (size_t)buffer_size);
                    if (nread < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        close_client(pfds, i, &nfds);
                        goto next_client;
                    }
                    if (nread == 0) {
                        close_client(pfds, i, &nfds);
                        goto next_client;
                    }
                    if (wait_writable_and_write(pfds[i].fd, buf, (size_t)nread) != 0) {
                        close_client(pfds, i, &nfds);
                        goto next_client;
                    }
                }
            }
            pfds[i].revents = 0;
            i++;
next_client:
            ;
        }
    }
}
