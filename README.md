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
│   ├── report.pdf
│   ├── report.docx
│   ├── report.md
│   ├── figures/
│   └── README.md
├── partB/
│   ├── src/
│   │   ├── common.h
│   │   ├── utils.c
│   │   ├── poll_server.c
│   │   ├── threadpool_server.c
│   │   ├── epoll_server.c
│   │   └── client_bench.c
│   ├── Makefile
│   ├── paper.pdf
│   ├── paper.docx
│   ├── paper.md
│   ├── figures/
│   └── README.md
├── results/
├── docs/
├── Lab1_UNIX_IO性能实验与高吞吐量IO服务器设计_v2.md
└── Lab1_UNIX_IO性能实验与高吞吐量IO服务器设计_v2.pdf
```

## Current Progress

- Part A benchmark scaffold and implementation are available in `partA/`.
- Part B contains `poll(2)`, thread-pool, and Linux `epoll(7)` TCP echo servers plus a benchmark client.
- Part A report is available as `partA/report.pdf`; Part B paper is available as `partB/paper.pdf`.
- The actual experiment workflow and current result-file notes are recorded in `docs/EXPERIMENT_FLOW.md`.
- The final submission checklist is recorded in `docs/SUBMISSION_CHECKLIST.md`.
