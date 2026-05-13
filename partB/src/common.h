#ifndef LAB1_PARTB_COMMON_H
#define LAB1_PARTB_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

int parse_int_arg(const char *text, int min_value, int max_value, int *out);
uint64_t monotonic_ns(void);

int set_nonblocking(int fd);
int create_listen_socket(int port, int backlog, int nonblocking);
int connect_to_server(const char *host, int port);

ssize_t retrying_read(int fd, void *buf, size_t len);
int write_full_blocking(int fd, const void *buf, size_t len);
int read_full_blocking(int fd, void *buf, size_t len);

void ignore_sigpipe(void);
void fatal_errno(const char *message);
void fatal_msg(const char *message);

#endif
