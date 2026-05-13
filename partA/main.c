#define _POSIX_C_SOURCE 200809L

#include "my_fread.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef O_SYNC
#define O_SYNC 0
#endif

typedef int (*bench_fn)(const char *path, size_t bufsize, unsigned long long total_bytes);

typedef struct {
    double real_sec;
    double user_sec;
    double sys_sec;
} elapsed_t;

typedef struct {
    const char *name;
    bench_fn fn;
    int uses_bufsizes;
    int is_write;
} bench_case_t;

static const size_t k_bufsizes[] = {
    1, 2, 4, 8, 16, 32, 64, 128, 256, 512,
    1024, 2048, 4096, 8192, 16384, 32768,
    65536, 16777216
};

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s <method> <path> [total_write_bytes]\n\n"
            "Methods:\n"
            "  read          read(2) with all required buffer sizes\n"
            "  getc          stdio getc() byte-at-a-time read\n"
            "  fgetc         stdio fgetc() byte-at-a-time read\n"
            "  fread         stdio fread() with all required buffer sizes\n"
            "  my_fread      buffered read implemented on top of read(2)\n"
            "  write         write(2) without O_SYNC, all required buffer sizes\n"
            "  write_sync    write(2) with O_SYNC, all required buffer sizes\n"
            "  all_read      read + getc + fgetc + fread + my_fread\n"
            "  all_write     write + write_sync\n\n"
            "Examples:\n"
            "  %s read testfile.bin\n"
            "  %s write out.bin 524288000\n",
            prog, prog, prog);
}

static double timespec_diff_sec(const struct timespec *start, const struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) +
           (double)(end->tv_nsec - start->tv_nsec) / 1000000000.0;
}

static double timeval_diff_sec(const struct timeval *start, const struct timeval *end)
{
    return (double)(end->tv_sec - start->tv_sec) +
           (double)(end->tv_usec - start->tv_usec) / 1000000.0;
}

static int start_timer(struct timespec *wall, struct rusage *usage)
{
    if (clock_gettime(CLOCK_MONOTONIC, wall) != 0) {
        perror("clock_gettime");
        return -1;
    }
    if (getrusage(RUSAGE_SELF, usage) != 0) {
        perror("getrusage");
        return -1;
    }
    return 0;
}

static int stop_timer(const struct timespec *wall_start,
                      const struct rusage *usage_start,
                      elapsed_t *elapsed)
{
    struct timespec wall_end;
    struct rusage usage_end;

    if (clock_gettime(CLOCK_MONOTONIC, &wall_end) != 0) {
        perror("clock_gettime");
        return -1;
    }
    if (getrusage(RUSAGE_SELF, &usage_end) != 0) {
        perror("getrusage");
        return -1;
    }

    elapsed->real_sec = timespec_diff_sec(wall_start, &wall_end);
    elapsed->user_sec = timeval_diff_sec(&usage_start->ru_utime, &usage_end.ru_utime);
    elapsed->sys_sec = timeval_diff_sec(&usage_start->ru_stime, &usage_end.ru_stime);
    return 0;
}

static int read_full(int fd, void *buf, size_t len, unsigned long long *bytes)
{
    while (1) {
        ssize_t nread = read(fd, buf, len);
        if (nread < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (nread == 0) {
            return 0;
        }
        *bytes += (unsigned long long)nread;
    }
}

static int write_all(int fd, const unsigned char *buf, size_t len)
{
    size_t written = 0;

    while (written < len) {
        ssize_t nwritten = write(fd, buf + written, len - written);
        if (nwritten < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (nwritten == 0) {
            errno = EIO;
            return -1;
        }
        written += (size_t)nwritten;
    }

    return 0;
}

static int bench_read_syscall(const char *path, size_t bufsize, unsigned long long total_bytes)
{
    unsigned char *buf;
    int fd;
    int rc;
    unsigned long long bytes = 0;

    (void)total_bytes;
    buf = malloc(bufsize);
    if (buf == NULL) {
        perror("malloc");
        return -1;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror(path);
        free(buf);
        return -1;
    }

    rc = read_full(fd, buf, bufsize, &bytes);
    if (rc != 0) {
        perror("read");
    }
    if (close(fd) != 0) {
        perror("close");
        rc = -1;
    }

    free(buf);
    return rc;
}

static int bench_getc_stdio(const char *path, size_t bufsize, unsigned long long total_bytes)
{
    FILE *fp;
    unsigned long long bytes = 0;
    int ch;

    (void)bufsize;
    (void)total_bytes;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        perror(path);
        return -1;
    }

    while ((ch = getc(fp)) != EOF) {
        bytes++;
    }
    (void)bytes;

    if (ferror(fp)) {
        perror("getc");
        fclose(fp);
        return -1;
    }
    if (fclose(fp) != 0) {
        perror("fclose");
        return -1;
    }
    return 0;
}

static int bench_fgetc_stdio(const char *path, size_t bufsize, unsigned long long total_bytes)
{
    FILE *fp;
    unsigned long long bytes = 0;
    int ch;

    (void)bufsize;
    (void)total_bytes;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        perror(path);
        return -1;
    }

    while ((ch = fgetc(fp)) != EOF) {
        bytes++;
    }
    (void)bytes;

    if (ferror(fp)) {
        perror("fgetc");
        fclose(fp);
        return -1;
    }
    if (fclose(fp) != 0) {
        perror("fclose");
        return -1;
    }
    return 0;
}

static int bench_fread_stdio(const char *path, size_t bufsize, unsigned long long total_bytes)
{
    FILE *fp;
    unsigned char *buf;

    (void)total_bytes;

    buf = malloc(bufsize);
    if (buf == NULL) {
        perror("malloc");
        return -1;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        perror(path);
        free(buf);
        return -1;
    }

    while (fread(buf, 1, bufsize, fp) > 0) {
    }

    if (ferror(fp)) {
        perror("fread");
        fclose(fp);
        free(buf);
        return -1;
    }
    if (fclose(fp) != 0) {
        perror("fclose");
        free(buf);
        return -1;
    }

    free(buf);
    return 0;
}

static int bench_my_fread(const char *path, size_t bufsize, unsigned long long total_bytes)
{
    my_file_t *fp;
    unsigned char *buf;

    (void)total_bytes;

    buf = malloc(bufsize);
    if (buf == NULL) {
        perror("malloc");
        return -1;
    }

    fp = my_fopen(path, bufsize);
    if (fp == NULL) {
        perror(path);
        free(buf);
        return -1;
    }

    while (1) {
        ssize_t nread = my_fread(buf, 1, bufsize, fp);
        if (nread < 0) {
            errno = my_ferror(fp);
            perror("my_fread");
            my_fclose(fp);
            free(buf);
            return -1;
        }
        if (nread == 0) {
            break;
        }
    }

    if (my_fclose(fp) != 0) {
        perror("my_fclose");
        free(buf);
        return -1;
    }

    free(buf);
    return 0;
}

static int bench_write_common(const char *path,
                              size_t bufsize,
                              unsigned long long total_bytes,
                              int sync_flag)
{
    unsigned char *buf;
    int flags = O_WRONLY | O_CREAT | O_TRUNC;
    int fd;
    unsigned long long remaining = total_bytes;
    int rc = 0;

    if (sync_flag) {
        flags |= O_SYNC;
    }

    buf = malloc(bufsize);
    if (buf == NULL) {
        perror("malloc");
        return -1;
    }
    memset(buf, 0x5a, bufsize);

    fd = open(path, flags, 0644);
    if (fd < 0) {
        perror(path);
        free(buf);
        return -1;
    }

    while (remaining > 0) {
        size_t chunk = bufsize;
        if (remaining < (unsigned long long)chunk) {
            chunk = (size_t)remaining;
        }
        if (write_all(fd, buf, chunk) != 0) {
            perror("write");
            rc = -1;
            break;
        }
        remaining -= (unsigned long long)chunk;
    }

    if (close(fd) != 0) {
        perror("close");
        rc = -1;
    }
    free(buf);
    return rc;
}

static int bench_write_unsync(const char *path, size_t bufsize, unsigned long long total_bytes)
{
    return bench_write_common(path, bufsize, total_bytes, 0);
}

static int bench_write_sync(const char *path, size_t bufsize, unsigned long long total_bytes)
{
    return bench_write_common(path, bufsize, total_bytes, 1);
}

static unsigned long long file_size_or_zero(const char *path)
{
    struct stat st;

    if (stat(path, &st) == 0 && st.st_size >= 0) {
        return (unsigned long long)st.st_size;
    }
    return 0;
}

static int parse_ull(const char *s, unsigned long long *value)
{
    char *end = NULL;
    unsigned long long parsed;

    errno = 0;
    parsed = strtoull(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0') {
        return -1;
    }
    *value = parsed;
    return 0;
}

static int run_one(const bench_case_t *bench,
                   const char *path,
                   size_t bufsize,
                   unsigned long long total_bytes)
{
    struct timespec wall_start;
    struct rusage usage_start;
    elapsed_t elapsed;
    unsigned long long bytes_metric = bench->is_write ? total_bytes : file_size_or_zero(path);

    if (start_timer(&wall_start, &usage_start) != 0) {
        return -1;
    }
    if (bench->fn(path, bufsize, total_bytes) != 0) {
        return -1;
    }
    if (stop_timer(&wall_start, &usage_start, &elapsed) != 0) {
        return -1;
    }

    printf("%s,%zu,%llu,%.9f,%.9f,%.9f\n",
           bench->name,
           bufsize,
           bytes_metric,
           elapsed.real_sec,
           elapsed.user_sec,
           elapsed.sys_sec);
    fflush(stdout);
    return 0;
}

static int run_case(const bench_case_t *bench, const char *path, unsigned long long total_bytes)
{
    size_t i;

    if (!bench->uses_bufsizes) {
        return run_one(bench, path, 1, total_bytes);
    }

    for (i = 0; i < sizeof(k_bufsizes) / sizeof(k_bufsizes[0]); i++) {
        if (run_one(bench, path, k_bufsizes[i], total_bytes) != 0) {
            return -1;
        }
    }
    return 0;
}

static const bench_case_t k_cases[] = {
    {"read", bench_read_syscall, 1, 0},
    {"getc", bench_getc_stdio, 0, 0},
    {"fgetc", bench_fgetc_stdio, 0, 0},
    {"fread", bench_fread_stdio, 1, 0},
    {"my_fread", bench_my_fread, 1, 0},
    {"write", bench_write_unsync, 1, 1},
    {"write_sync", bench_write_sync, 1, 1},
};

static const bench_case_t *find_case(const char *name)
{
    size_t i;

    for (i = 0; i < sizeof(k_cases) / sizeof(k_cases[0]); i++) {
        if (strcmp(k_cases[i].name, name) == 0) {
            return &k_cases[i];
        }
    }
    return NULL;
}

int main(int argc, char **argv)
{
    const char *method;
    const char *path;
    unsigned long long total_write_bytes = 500ULL * 1024ULL * 1024ULL;
    int rc = 0;

    if (argc < 3 || argc > 4) {
        usage(argv[0]);
        return 2;
    }

    method = argv[1];
    path = argv[2];

    if (argc == 4 && parse_ull(argv[3], &total_write_bytes) != 0) {
        fprintf(stderr, "invalid total_write_bytes: %s\n", argv[3]);
        return 2;
    }

    printf("method,bufsize,bytes,real_sec,user_sec,sys_sec\n");

    if (strcmp(method, "all_read") == 0) {
        size_t i;
        for (i = 0; i < sizeof(k_cases) / sizeof(k_cases[0]); i++) {
            if (!k_cases[i].is_write && run_case(&k_cases[i], path, total_write_bytes) != 0) {
                rc = 1;
                break;
            }
        }
    } else if (strcmp(method, "all_write") == 0) {
        size_t i;
        for (i = 0; i < sizeof(k_cases) / sizeof(k_cases[0]); i++) {
            if (k_cases[i].is_write && run_case(&k_cases[i], path, total_write_bytes) != 0) {
                rc = 1;
                break;
            }
        }
    } else {
        const bench_case_t *bench = find_case(method);
        if (bench == NULL) {
            usage(argv[0]);
            return 2;
        }
        if (run_case(bench, path, total_write_bytes) != 0) {
            rc = 1;
        }
    }

    return rc;
}
