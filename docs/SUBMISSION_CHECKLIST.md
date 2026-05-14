# Lab 1 Submission Checklist

This checklist compares the current repository with `Lab1_UNIX_IO性能实验与高吞吐量IO服务器设计_v2.md`.

## Part A

Required source code:

- `partA/main.c`: implemented
- `partA/my_fread.c`, `partA/my_fread.h`: implemented
- `partA/Makefile`: implemented

Required experiments:

- `read()` with all required BUFFSIZE values: data in `results/partA_all_read.csv`
- `getc()` / `fgetc()`: data in `results/partA_all_read.csv`
- `fread()` with all required BUFFSIZE values: data in `results/partA_all_read.csv`
- `my_fread()` with all required BUFFSIZE values: data in `results/partA_all_read.csv`
- `write()` without `O_SYNC`: data in `results/partA_all_write.csv`, plus 64 KB and 1 MB comparisons
- `write()` with `O_SYNC`: data in `results/partA_write_sync_64KB.csv` and `results/partA_write_sync_1MB.csv`

Required report:

- `partA/report.pdf`: generated
- `partA/report.docx`: generated as an extra editable copy
- `partA/report.md`: source version
- `partA/figures/`: generated charts

Notes:

- Read experiments use a 500 MB file.
- `O_SYNC` write uses smaller 64 KB and 1 MB workloads because extremely small BUFFSIZE values make 500 MB synchronous writes impractical. This rationale is explained in the report.

## Part B

Required source code:

- `partB/src/poll_server.c`: implemented
- `partB/src/threadpool_server.c`: implemented
- `partB/src/client_bench.c`: implemented
- `partB/Makefile`: implemented
- `partB/README.md`: implemented

Extra source code:

- `partB/src/epoll_server.c`: implemented as an optional Linux-only bonus model

Required/expected experiments:

- `poll` server: data in `results/partB_poll_10.csv`, `results/partB_poll_100.csv`, `results/partB_poll_500.csv`
- thread-pool server: data in `results/partB_threadpool_10.csv`, `results/partB_threadpool_100.csv`, `results/partB_threadpool_500.csv`
- `epoll` server: data in `results/partB_epoll_10.csv`, `results/partB_epoll_100.csv`, `results/partB_epoll_500.csv`
- Throughput, average latency, P50, P95, and P99 are recorded by `client_bench`

Required paper:

- `partB/paper.pdf`: generated
- `partB/paper.docx`: generated as an extra editable copy
- `partB/paper.md`: source version
- `partB/figures/`: generated charts and architecture diagram

Notes:

- The paper includes principle analysis for blocking I/O, `select`/`poll`/`epoll`, asynchronous I/O, and multi-threaded I/O.
- The paper includes a high-throughput architecture proposal based on Reactor + `epoll` + worker thread pool.
- `io_uring_server` is not implemented. The lab document marks io_uring as optional; the paper discusses it only in the principle-analysis section.
- CPU and memory usage curves were not separately collected. This is listed as a limitation in `partB/paper.md`.
- Maximum connection capacity was not exhaustively searched; experiments use 10, 100, and 500 concurrent connections.

## Repository-Level Files

- `README.md`: updated with current layout and report/paper locations
- `docs/EXPERIMENT_FLOW.md`: records the actual experiment commands
- `docs/SUBMISSION_CHECKLIST.md`: this checklist
- `results/env/`: Linux experiment environment information
- `results/`: CSV experiment results

## Final Manual Step

Before course submission, package the repository directory according to the required naming rule:

```text
学号姓名_系统程序设计lab1
```

The generated executable files and large temporary benchmark files are not required.
