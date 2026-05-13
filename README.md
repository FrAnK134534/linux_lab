# Lab 1: UNIX I/O Performance and High-Throughput I/O Server

This repository contains the source code and notes for System Programming Lab 1.

## Layout

```text
.
├── partA/
│   ├── main.c
│   ├── my_fread.c
│   ├── my_fread.h
│   ├── Makefile
│   └── README.md
├── partB/
│   ├── src/
│   │   ├── common.h
│   │   ├── utils.c
│   │   ├── poll_server.c
│   │   ├── threadpool_server.c
│   │   └── client_bench.c
│   ├── Makefile
│   └── README.md
├── Lab1_UNIX_IO性能实验与高吞吐量IO服务器设计_v2.md
└── Lab1_UNIX_IO性能实验与高吞吐量IO服务器设计_v2.pdf
```

## Current Progress

- Part A benchmark scaffold and implementation are available in `partA/`.
- Part B contains baseline `poll(2)` and thread-pool TCP echo servers plus a benchmark client.
