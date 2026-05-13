# Lab 1 Experiment Flow

This document records the actual experiment workflow used for the current result files. It is not the final report; it is a reproducibility note for running, checking, and synchronizing the experiments.

## 1. Repository Workflow

On the Linux server:

```sh
git clone -b lab1 https://github.com/FrAnK134534/linux_lab.git
cd linux_lab
git pull --ff-only origin lab1
```

After experiments:

```sh
git status
git add results
git commit -m "Add Linux benchmark results"
git push origin lab1
```

On the local machine:

```sh
cd /Users/frank/Documents/系统程序设计/lab1
git checkout lab1
git pull --ff-only origin lab1
```

## 2. Part A: UNIX I/O Benchmark

Build:

```sh
cd partA
make
mkdir -p ../results
```

Generate a 500 MB read test file:

```sh
dd if=/dev/urandom of=testfile.bin bs=1M count=500
```

Run read benchmarks:

```sh
./io_bench all_read testfile.bin > ../results/partA_all_read.csv
```

This produces data for:

- `read`
- `getc`
- `fgetc`
- `fread`
- `my_fread`

The output columns are:

```text
method,bufsize,bytes,real_sec,user_sec,sys_sec
```

### Write Benchmark Design

The first full write attempt used 500 MB:

```sh
./io_bench all_write out.bin 524288000 > ../results/partA_all_write.csv
```

This completed the ordinary `write` section, but `write_sync` with very small buffer sizes was too slow. The reason is that `O_SYNC` with `bufsize=1` performs one synchronous write path per byte. For 500 MB, this means 524,288,000 `write()` calls.

Therefore, the final write experiment uses smaller fixed sizes while keeping the same buffer-size sequence.

Run 64 KB write comparison:

```sh
./io_bench write out_64KB.bin 65536 > ../results/partA_write_64KB.csv
./io_bench write_sync out_sync_64KB.bin 65536 > ../results/partA_write_sync_64KB.csv
```

Run 1 MB write comparison:

```sh
./io_bench write out_1MB.bin 1048576 > ../results/partA_write_1MB.csv
./io_bench write_sync out_sync_1MB.bin 1048576 > ../results/partA_write_sync_1MB.csv
```

Rationale:

- The lab document explicitly recommends a large file for read experiments.
- The write sections require comparing different buffer sizes and the effect of `O_SYNC`, but do not require 500 MB.
- Using smaller write sizes keeps the experiment practical while preserving the trend across the required buffer sizes.

## 3. Part B: Server Benchmark

Build:

```sh
cd partB
make
mkdir -p ../results
```

Run the `poll` server:

```sh
./poll_server 8080 2048 4096 > ../results/poll_server.log 2>&1 &
echo $! > ../results/poll_server.pid
sleep 1
```

Run client benchmarks:

```sh
./client_bench 127.0.0.1 8080 10 1000 64 poll > ../results/partB_poll_10.csv
./client_bench 127.0.0.1 8080 100 1000 64 poll > ../results/partB_poll_100.csv
./client_bench 127.0.0.1 8080 500 1000 64 poll > ../results/partB_poll_500.csv
```

Stop the `poll` server:

```sh
kill $(cat ../results/poll_server.pid)
```

Run the thread-pool server:

```sh
./threadpool_server 8081 4 4096 4096 > ../results/threadpool_server.log 2>&1 &
echo $! > ../results/threadpool_server.pid
sleep 1
```

Run client benchmarks:

```sh
./client_bench 127.0.0.1 8081 10 1000 64 threadpool > ../results/partB_threadpool_10.csv
./client_bench 127.0.0.1 8081 100 1000 64 threadpool > ../results/partB_threadpool_100.csv
./client_bench 127.0.0.1 8081 500 1000 64 threadpool > ../results/partB_threadpool_500.csv
```

Stop the thread-pool server:

```sh
kill $(cat ../results/threadpool_server.pid)
```

Client output columns:

```text
server,connections,msg_size,total_requests,total_time_sec,throughput_req_s,avg_ms,p50_ms,p95_ms,p99_ms
```

## 4. Result Files

Current result files:

```text
results/partA_all_read.csv
results/partA_all_write.csv
results/partA_write_64KB.csv
results/partA_write_sync_64KB.csv
results/partA_write_1MB.csv
results/partA_write_sync_1MB.csv
results/partB_poll_10.csv
results/partB_poll_100.csv
results/partB_poll_500.csv
results/partB_threadpool_10.csv
results/partB_threadpool_100.csv
results/partB_threadpool_500.csv
```

`partA_all_write.csv` contains the completed 500 MB ordinary `write` data. It does not contain `write_sync` data because the original full `all_write` run was stopped before the `O_SYNC` part became practical.

## 5. Initial Observations

These are quick notes for later report writing, not the final analysis.

Part A read:

- `read` with `bufsize=1` took about 112.72 s for 500 MB, while the best `read` result was about 0.066 s at `bufsize=65536`.
- `getc` and `fgetc` both finished around 1.36 s. They are much faster than byte-by-byte `read` because stdio uses an internal user-space buffer.
- `fread` and `my_fread` show the expected trend: small buffers are dominated by call overhead, while larger buffers reach a plateau.
- The best `fread` result is around 0.067 s at `bufsize=65536`; the best `my_fread` result is around 0.076 s at `bufsize=16384`.

Part A write:

- Ordinary 500 MB `write` shows a strong system-call-count effect: `bufsize=1` took about 519.89 s, while the larger-buffer results were around 2 to 4 s.
- In the 1 MB comparison, ordinary `write` with `bufsize=1` took about 1.11 s, while `write_sync` with `bufsize=1` took about 560.83 s.
- `O_SYNC` cost decreases rapidly as buffer size grows because the number of synchronous write operations decreases.

Part B:

- `poll` throughput was about 79.9k, 115.2k, and 113.1k requests/s for 10, 100, and 500 connections.
- Thread-pool throughput was about 68.9k, 114.4k, and 119.7k requests/s for 10, 100, and 500 connections.
- The thread-pool average latency is higher than P99 in some runs. This can happen because a small number of connection-level queue waits become extreme outliers and raise the average, while they occupy less than 1% of all requests and therefore may not appear in P99.

## 6. Cleanup

Do not commit large generated binary files:

```sh
rm -f partA/testfile.bin partA/out*.bin partA/out_sync*.bin
make -C partA clean
make -C partB clean
```

The CSV files under `results/` are the data that should be committed.
