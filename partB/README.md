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

Run the Linux `epoll(7)` based server:

```sh
./epoll_server 8082 2048 4096
```

Arguments:

```text
./epoll_server [port] [max_events] [buffer_size]
```

Run the Linux `io_uring` based server:

```sh
./io_uring_server 8083 256 4096
```

Arguments:

```text
./io_uring_server [port] [queue_depth] [buffer_size]
```

`io_uring_server` needs Linux and liburing development files. On Debian/Ubuntu:

```sh
sudo apt-get install liburing-dev
make clean
make
```

If `pkg-config` is unavailable but liburing is installed, build explicitly:

```sh
make clean
make HAVE_LIBURING=1
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
- `epoll_server` is a Linux-only single-threaded echo server using `epoll`.
- `io_uring_server` is a Linux-only echo server using liburing completion events.
- `threadpool_server` accepts connections in the main thread and assigns each connection to a worker thread.
- Both servers intentionally keep the protocol simple: every request is echoed back byte-for-byte.
