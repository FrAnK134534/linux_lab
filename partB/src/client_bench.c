#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    const char *host;
    int port;
    int requests;
    int message_size;
    uint64_t *latencies;
    size_t latency_offset;
    int failed;
} client_arg_t;

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s <host> <port> <connections> <requests_per_connection> <message_size> [server_name]\n",
            prog);
}

static int compare_u64(const void *a, const void *b)
{
    uint64_t lhs = *(const uint64_t *)a;
    uint64_t rhs = *(const uint64_t *)b;

    return (lhs > rhs) - (lhs < rhs);
}

static double ns_to_ms(uint64_t ns)
{
    return (double)ns / 1000000.0;
}

static uint64_t percentile(const uint64_t *values, size_t count, double p)
{
    size_t index;

    if (count == 0) {
        return 0;
    }
    index = (size_t)((p / 100.0) * (double)(count - 1));
    return values[index];
}

static void fill_payload(unsigned char *buf, int size, int seed)
{
    int i;

    for (i = 0; i < size; i++) {
        buf[i] = (unsigned char)((i + seed) & 0xff);
    }
}

static void *client_worker(void *arg)
{
    client_arg_t *client = arg;
    unsigned char *send_buf = NULL;
    unsigned char *recv_buf = NULL;
    int fd;
    int i;

    send_buf = malloc((size_t)client->message_size);
    recv_buf = malloc((size_t)client->message_size);
    if (send_buf == NULL || recv_buf == NULL) {
        client->failed = 1;
        goto done;
    }

    fd = connect_to_server(client->host, client->port);
    if (fd < 0) {
        client->failed = 1;
        goto done;
    }

    fill_payload(send_buf, client->message_size, (int)client->latency_offset);

    for (i = 0; i < client->requests; i++) {
        uint64_t start = monotonic_ns();
        uint64_t end;

        if (write_full_blocking(fd, send_buf, (size_t)client->message_size) != 0) {
            client->failed = 1;
            break;
        }
        if (read_full_blocking(fd, recv_buf, (size_t)client->message_size) != 0) {
            client->failed = 1;
            break;
        }
        end = monotonic_ns();

        if (memcmp(send_buf, recv_buf, (size_t)client->message_size) != 0) {
            client->failed = 1;
            break;
        }
        client->latencies[client->latency_offset + (size_t)i] = end - start;
    }

    close(fd);

done:
    free(send_buf);
    free(recv_buf);
    return NULL;
}

int main(int argc, char **argv)
{
    const char *host;
    const char *server_name = "server";
    int port;
    int connections;
    int requests;
    int message_size;
    size_t total_requests;
    uint64_t *latencies;
    pthread_t *threads;
    client_arg_t *args;
    uint64_t start;
    uint64_t end;
    uint64_t sum = 0;
    int failed = 0;
    size_t j;
    int i;

    if (argc < 6 || argc > 7) {
        usage(argv[0]);
        return 2;
    }

    host = argv[1];
    if (parse_int_arg(argv[2], 1, 65535, &port) != 0 ||
        parse_int_arg(argv[3], 1, 100000, &connections) != 0 ||
        parse_int_arg(argv[4], 1, 10000000, &requests) != 0 ||
        parse_int_arg(argv[5], 1, 1048576, &message_size) != 0) {
        usage(argv[0]);
        return 2;
    }
    if (argc == 7) {
        server_name = argv[6];
    }

    total_requests = (size_t)connections * (size_t)requests;
    latencies = calloc(total_requests, sizeof(*latencies));
    threads = calloc((size_t)connections, sizeof(*threads));
    args = calloc((size_t)connections, sizeof(*args));
    if (latencies == NULL || threads == NULL || args == NULL) {
        fatal_errno("calloc");
    }

    ignore_sigpipe();
    start = monotonic_ns();
    for (i = 0; i < connections; i++) {
        args[i].host = host;
        args[i].port = port;
        args[i].requests = requests;
        args[i].message_size = message_size;
        args[i].latencies = latencies;
        args[i].latency_offset = (size_t)i * (size_t)requests;
        args[i].failed = 0;
        if (pthread_create(&threads[i], NULL, client_worker, &args[i]) != 0) {
            fatal_errno("pthread_create");
        }
    }
    for (i = 0; i < connections; i++) {
        pthread_join(threads[i], NULL);
        if (args[i].failed) {
            failed = 1;
        }
    }
    end = monotonic_ns();

    if (failed) {
        fprintf(stderr, "one or more client workers failed\n");
        free(latencies);
        free(threads);
        free(args);
        return 1;
    }

    for (j = 0; j < total_requests; j++) {
        sum += latencies[j];
    }
    qsort(latencies, total_requests, sizeof(*latencies), compare_u64);

    printf("server,connections,msg_size,total_requests,total_time_sec,throughput_req_s,avg_ms,p50_ms,p95_ms,p99_ms\n");
    printf("%s,%d,%d,%zu,%.6f,%.2f,%.6f,%.6f,%.6f,%.6f\n",
           server_name,
           connections,
           message_size,
           total_requests,
           (double)(end - start) / 1000000000.0,
           (double)total_requests / ((double)(end - start) / 1000000000.0),
           ns_to_ms(sum / (uint64_t)total_requests),
           ns_to_ms(percentile(latencies, total_requests, 50.0)),
           ns_to_ms(percentile(latencies, total_requests, 95.0)),
           ns_to_ms(percentile(latencies, total_requests, 99.0)));

    free(latencies);
    free(threads);
    free(args);
    return 0;
}
