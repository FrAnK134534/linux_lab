# Part A: UNIX I/O Benchmark

This directory contains the benchmark program for Lab 1 Part A.

## Build

```sh
make
```

## Generate a Test File

For the formal experiment, use a large file on Linux, for example:

```sh
dd if=/dev/urandom of=testfile.bin bs=1M count=500
```

## Run

The program prints CSV to stdout:

```sh
./io_bench read testfile.bin > read.csv
./io_bench fread testfile.bin > fread.csv
./io_bench my_fread testfile.bin > my_fread.csv
./io_bench getc testfile.bin > getc.csv
./io_bench fgetc testfile.bin > fgetc.csv
./io_bench all_read testfile.bin > all_read.csv
./io_bench write out.bin 524288000 > write.csv
./io_bench write_sync out_sync.bin 524288000 > write_sync.csv
./io_bench all_write out.bin 524288000 > all_write.csv
```

CSV columns:

```text
method,bufsize,bytes,real_sec,user_sec,sys_sec
```

## Buffer Sizes

The benchmark uses the buffer sizes required by the lab document:

```text
1, 2, 4, 8, 16, 32, 64, 128, 256, 512,
1024, 2048, 4096, 8192, 16384, 32768,
65536, 16777216
```

## Notes

- `getc` and `fgetc` are tested once because they read one byte per call at the API level.
- `my_fread` is a small user-space buffered reader implemented on top of `read(2)`.
- `write_sync` opens the output file with `O_SYNC`.
- On Linux, clear page cache before cold-cache read experiments if allowed:

```sh
sync
echo 3 | sudo tee /proc/sys/vm/drop_caches
```
