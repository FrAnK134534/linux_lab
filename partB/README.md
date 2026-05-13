# Part B: High-Throughput I/O Server

This directory contains the baseline concurrent TCP echo servers and benchmark client for Lab 1 Part B.

## Build

```sh
make
```

## Servers

Run the `poll(2)` based server:

```sh
./poll_server 8080 1024 4096
```

Arguments:

```text
./poll_server [port] [max_clients] [buffer_size]
```

Run the thread-pool server:

```sh
./threadpool_server 8081 4 4096 4096
```

Arguments:

```text
./threadpool_server [port] [workers] [queue_capacity] [buffer_size]
```

## Benchmark Client

```sh
./client_bench 127.0.0.1 8080 100 1000 64 poll
```

Arguments:

```text
./client_bench <host> <port> <connections> <requests_per_connection> <message_size> [server_name]
```

The client prints CSV:

```text
server,connections,msg_size,total_requests,total_time_sec,throughput_req_s,avg_ms,p50_ms,p95_ms,p99_ms
```

## Example

Terminal 1:

```sh
make
./poll_server 8080 1024 4096
```

Terminal 2:

```sh
./client_bench 127.0.0.1 8080 20 1000 64 poll
```

## Notes

- `poll_server` is a single-threaded multiplexing echo server.
- `threadpool_server` accepts connections in the main thread and assigns each connection to a worker thread.
- Both servers intentionally keep the protocol simple: every request is echoed back byte-for-byte.
- An optional Linux-only `epoll_server` can be added later for extra comparison.
