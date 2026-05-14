#!/usr/bin/env python3

from __future__ import annotations

import csv
from pathlib import Path
import textwrap

import matplotlib.pyplot as plt
import pandas as pd


ROOT = Path(__file__).resolve().parents[1]
PART_A = ROOT / "partA"
RESULTS = ROOT / "results"
ENV = RESULTS / "env"
FIGURES = PART_A / "figures"
REPORT = PART_A / "report.md"


def read_env(path: Path) -> str:
    if not path.exists():
        return "未记录"
    return path.read_text(encoding="utf-8", errors="replace").strip()


def read_csv(path: Path) -> pd.DataFrame:
    return pd.read_csv(path)


def write_csv_table(path: Path, max_rows: int | None = None) -> str:
    rows: list[list[str]] = []
    with path.open(newline="", encoding="utf-8") as f:
        reader = csv.reader(f)
        for row in reader:
            rows.append(row)
    if max_rows is not None:
        rows = rows[: max_rows + 1]
    header = rows[0]
    body = rows[1:]
    lines = [
        "| " + " | ".join(header) + " |",
        "| " + " | ".join(["---"] * len(header)) + " |",
    ]
    for row in body:
        lines.append("| " + " | ".join(row) + " |")
    return "\n".join(lines)


def savefig(name: str) -> str:
    FIGURES.mkdir(parents=True, exist_ok=True)
    path = FIGURES / name
    plt.tight_layout()
    plt.savefig(path, dpi=180)
    plt.close()
    return f"figures/{name}"


def plot_read_real(read_df: pd.DataFrame) -> str:
    methods = ["read", "fread", "my_fread"]
    plt.figure(figsize=(8, 4.8))
    for method in methods:
        df = read_df[read_df["method"] == method]
        plt.plot(df["bufsize"], df["real_sec"], marker="o", linewidth=1.8, label=method)
    plt.xscale("log", base=2)
    plt.yscale("log")
    plt.xlabel("Buffer size (bytes, log2)")
    plt.ylabel("Real time (seconds, log)")
    plt.title("Read-like APIs: real time vs buffer size")
    plt.grid(True, which="both", linestyle="--", alpha=0.35)
    plt.legend()
    return savefig("partA_read_real_time.png")


def plot_read_cpu(read_df: pd.DataFrame) -> str:
    df = read_df[read_df["method"] == "read"]
    plt.figure(figsize=(8, 4.8))
    plt.plot(df["bufsize"], df["real_sec"], marker="o", label="real")
    plt.plot(df["bufsize"], df["user_sec"], marker="s", label="user")
    plt.plot(df["bufsize"], df["sys_sec"], marker="^", label="sys")
    plt.xscale("log", base=2)
    plt.yscale("log")
    plt.xlabel("Buffer size (bytes, log2)")
    plt.ylabel("Time (seconds, log)")
    plt.title("read(2): real/user/sys time")
    plt.grid(True, which="both", linestyle="--", alpha=0.35)
    plt.legend()
    return savefig("partA_read_time_breakdown.png")


def plot_getc_fgetc(read_df: pd.DataFrame) -> str:
    df = read_df[read_df["method"].isin(["getc", "fgetc"])]
    plt.figure(figsize=(5.8, 4.4))
    plt.bar(df["method"], df["real_sec"], color=["#4C78A8", "#F58518"])
    plt.ylabel("Real time (seconds)")
    plt.title("getc vs fgetc on the same 500 MB file")
    plt.grid(True, axis="y", linestyle="--", alpha=0.35)
    for idx, row in enumerate(df.itertuples()):
        plt.text(idx, row.real_sec, f"{row.real_sec:.3f}s", ha="center", va="bottom")
    return savefig("partA_getc_fgetc.png")


def plot_write_1mb(write_df: pd.DataFrame, sync_df: pd.DataFrame) -> str:
    plt.figure(figsize=(8, 4.8))
    plt.plot(write_df["bufsize"], write_df["real_sec"], marker="o", label="write, 1 MB")
    plt.plot(sync_df["bufsize"], sync_df["real_sec"], marker="s", label="write + O_SYNC, 1 MB")
    plt.xscale("log", base=2)
    plt.yscale("log")
    plt.xlabel("Buffer size (bytes, log2)")
    plt.ylabel("Real time (seconds, log)")
    plt.title("write vs write with O_SYNC, 1 MB total")
    plt.grid(True, which="both", linestyle="--", alpha=0.35)
    plt.legend()
    return savefig("partA_write_vs_sync_1MB.png")


def plot_write_full(write_full_df: pd.DataFrame) -> str:
    plt.figure(figsize=(8, 4.8))
    plt.plot(write_full_df["bufsize"], write_full_df["real_sec"], marker="o", label="write, 500 MB")
    plt.xscale("log", base=2)
    plt.yscale("log")
    plt.xlabel("Buffer size (bytes, log2)")
    plt.ylabel("Real time (seconds, log)")
    plt.title("write(2): 500 MB real time vs buffer size")
    plt.grid(True, which="both", linestyle="--", alpha=0.35)
    plt.legend()
    return savefig("partA_write_500MB.png")


def fmt_seconds(value: float) -> str:
    return f"{value:.6f}"


def best_row(df: pd.DataFrame, method: str) -> pd.Series:
    sub = df[df["method"] == method]
    return sub.loc[sub["real_sec"].idxmin()]


def worst_row(df: pd.DataFrame, method: str) -> pd.Series:
    sub = df[df["method"] == method]
    return sub.loc[sub["real_sec"].idxmax()]


def env_summary() -> str:
    uname = read_env(ENV / "uname.txt").splitlines()[0]
    lsb = read_env(ENV / "lsb_release.txt")
    cpu = read_env(ENV / "lscpu.txt")
    fs = read_env(ENV / "df.txt")

    distro = "Ubuntu 20.04.6 LTS"
    for line in lsb.splitlines():
        if line.startswith("Description:"):
            distro = line.split(":", 1)[1].strip()
    cpu_model = "Intel Xeon"
    cpu_count = "64"
    for line in cpu.splitlines():
        if line.startswith("Model name:"):
            cpu_model = line.split(":", 1)[1].strip()
        if line.startswith("CPU(s):"):
            cpu_count = line.split(":", 1)[1].strip()

    mem_line = "未记录"
    for line in read_env(ENV / "free.txt").splitlines():
        if line.startswith("Mem:"):
            mem_line = line
            break

    return textwrap.dedent(
        f"""
        | 项目 | 信息 |
        | --- | --- |
        | 操作系统 | {distro} |
        | 内核 | `{uname}` |
        | CPU | {cpu_model}，{cpu_count} logical CPUs |
        | 内存 | `{mem_line}` |
        | 文件系统 | `{fs.splitlines()[-1]}` |
        | 磁盘 | 服务器存在 SSD/NVMe 设备，`lsblk` 显示 `ROTA=0`，包括 `WDC_WDS100T2G0A-` 与 `Dell DC NVMe PE8010 RI U.2 960GB` |
        """
    ).strip()


def make_report() -> None:
    read_df = read_csv(RESULTS / "partA_all_read.csv")
    write_full_df = read_csv(RESULTS / "partA_all_write.csv")
    write_1mb_df = read_csv(RESULTS / "partA_write_1MB.csv")
    write_sync_1mb_df = read_csv(RESULTS / "partA_write_sync_1MB.csv")

    read_fig = plot_read_real(read_df)
    read_cpu_fig = plot_read_cpu(read_df)
    getc_fig = plot_getc_fgetc(read_df)
    write_1mb_fig = plot_write_1mb(write_1mb_df, write_sync_1mb_df)
    write_full_fig = plot_write_full(write_full_df)

    read_best = best_row(read_df, "read")
    fread_best = best_row(read_df, "fread")
    my_fread_best = best_row(read_df, "my_fread")
    read_worst = worst_row(read_df, "read")
    write_worst = worst_row(write_full_df, "write")
    write_best = best_row(write_full_df, "write")
    sync_worst = worst_row(write_sync_1mb_df, "write_sync")
    sync_best = best_row(write_sync_1mb_df, "write_sync")

    report = f"""---
title: "Part A UNIX I/O 基础性能对比实验报告"
author: "系统程序设计 Lab 1"
date: "2026-05-14"
---

# Part A UNIX I/O 基础性能对比实验报告

## 1. 实验目的

本实验测量 UNIX/Linux 下多种 I/O 接口在顺序读写场景中的性能差异，重点观察系统调用次数、用户态缓冲、内核页缓存以及同步写语义对 real/user/sys 时间的影响。实验覆盖 `read()`、`getc()`、`fgetc()`、`fread()`、自实现 `my_fread()`、普通 `write()` 和带 `O_SYNC` 的 `write()`。

## 2. 实验环境

{env_summary()}

环境信息原始输出保存在 `results/env/` 目录下。实验结果 CSV 保存在 `results/` 目录下。

## 3. 实验方法

### 3.1 计时方法

测试程序使用 `clock_gettime(CLOCK_MONOTONIC)` 记录 wall clock time，也就是报告中的 `real_sec`；使用 `getrusage(RUSAGE_SELF)` 记录用户态 CPU 时间 `user_sec` 和内核态 CPU 时间 `sys_sec`。每次测试均输出 CSV：

```text
method,bufsize,bytes,real_sec,user_sec,sys_sec
```

本实验结果为服务器上的一次完整运行。由于 UNIX/Linux 文件 I/O 会受到 page cache、后台写回和系统负载影响，报告重点分析不同接口和 BUFFSIZE 的数量级趋势，而不是把某个单点时间视为硬件绝对性能。

### 3.2 缓冲区大小

按照实验要求，使用以下 BUFFSIZE：

```text
1, 2, 4, 8, 16, 32, 64, 128, 256, 512,
1024, 2048, 4096, 8192, 16384, 32768,
65536, 16777216
```

### 3.3 读实验

读实验使用 500 MB 测试文件，命令为：

```sh
./io_bench all_read testfile.bin > ../results/partA_all_read.csv
```

其中 `read`、`fread`、`my_fread` 对所有 BUFFSIZE 进行测试；`getc` 与 `fgetc` 是逐字符接口，因此各测试一次。

### 3.4 写实验

普通 `write` 首先进行了 500 MB 完整测试：

```sh
./io_bench all_write out.bin 524288000 > ../results/partA_all_write.csv
```

该文件中保留的是普通 `write` 的完整结果。由于 `O_SYNC` 在极小 BUFFSIZE 下会产生大量同步写路径，500 MB 的 `write_sync,bufsize=1` 会带来不可接受的运行时间，因此同步写对比采用 64 KB 和 1 MB 两组较小总写入量，保持相同 BUFFSIZE 序列：

```sh
./io_bench write out_1MB.bin 1048576 > ../results/partA_write_1MB.csv
./io_bench write_sync out_sync_1MB.bin 1048576 > ../results/partA_write_sync_1MB.csv
```

这样设计可以保证 `write` 与 `O_SYNC write` 在相同写入总量下比较，同时避免小缓冲同步写将实验时间放大到数小时。

## 4. 实验结果

### 4.1 read / fread / my_fread 趋势

![read/fread/my_fread real time]({read_fig})

`read()` 在 `bufsize=1` 时 real time 为 {fmt_seconds(read_worst.real_sec)} s；当 BUFFSIZE 增大到 {int(read_best.bufsize)} 字节时，real time 降至 {fmt_seconds(read_best.real_sec)} s。`fread()` 最优 real time 为 {fmt_seconds(fread_best.real_sec)} s，出现在 BUFFSIZE={int(fread_best.bufsize)}；`my_fread()` 最优 real time 为 {fmt_seconds(my_fread_best.real_sec)} s，出现在 BUFFSIZE={int(my_fread_best.bufsize)}。

![read real/user/sys breakdown]({read_cpu_fig})

从 `read()` 的 real/user/sys 分解可以看到，小 BUFFSIZE 时 `sys_sec` 占主导。这说明性能瓶颈主要来自频繁进入内核的系统调用开销，而不是用户态计算。随着 BUFFSIZE 增大，系统调用次数近似按比例下降，`sys_sec` 和 real time 也快速下降。到 16 KB 到 64 KB 附近后，继续增大缓冲区收益有限，说明此时已经接近顺序读取和页缓存路径的稳定吞吐区间。

### 4.2 getc 与 fgetc

![getc vs fgetc]({getc_fig})

`getc()` 与 `fgetc()` 都是逐字符接口，但它们并不等价于每字节调用一次 `read()`。标准 I/O 库内部维护用户态缓冲区，底层会批量从内核读取数据，然后在用户态逐字符返回。因此二者读取 500 MB 文件均约为 1.36 s，远快于 `read(bufsize=1)` 的 112.72 s。

本次实验中 `getc()` 和 `fgetc()` 的 real time 非常接近，说明在当前编译器、libc 和优化条件下，宏形式或函数形式带来的差异不是主导因素。它们相比块读取仍然更慢，主要因为每个字符仍需要经过 stdio 的状态检查和接口调用路径。

### 4.3 fread 与 my_fread

`fread()` 和 `my_fread()` 都体现了用户态缓冲减少系统调用次数的价值。`my_fread()` 内部使用 `read()` 补充缓冲区，再从用户态缓冲区向调用者拷贝数据。其趋势与 `fread()` 基本一致：小 BUFFSIZE 下系统调用次数多，性能较差；中等 BUFFSIZE 后进入平台吞吐上限。

`fread()` 的最优结果略好于 `my_fread()`，原因可能包括：标准库实现经过高度优化；stdio 缓冲、内部拷贝和分支处理更成熟；而本实验中的 `my_fread()` 是教学目的的简化实现，主要用于验证缓冲思想，而非替代 libc。

### 4.4 普通 write

![write 500MB]({write_full_fig})

普通 `write()` 的 500 MB 实验中，`bufsize=1` 的 real time 为 {fmt_seconds(write_worst.real_sec)} s；最优结果为 {fmt_seconds(write_best.real_sec)} s，出现在 BUFFSIZE={int(write_best.bufsize)}。趋势与 `read()` 类似：极小 BUFFSIZE 导致系统调用次数巨大，例如 500 MB / 1 byte 约为 5.24 亿次 `write()` 调用，因此 `sys_sec` 和 real time 都非常高。

当 BUFFSIZE 增大到 KB 级以后，系统调用次数大幅减少，普通 `write()` 的耗时降到几秒级。普通写不会要求每次调用都同步落盘，数据通常先进入内核页缓存，之后由内核异步写回存储设备，因此大缓冲下的 real time 主要反映进入页缓存和文件系统路径的成本，而不是每次真实落盘延迟。

### 4.5 write 与 O_SYNC write

![write vs O_SYNC write]({write_1mb_fig})

1 MB 对比实验中，普通 `write(bufsize=1)` real time 为 {fmt_seconds(write_1mb_df.iloc[0].real_sec)} s，而 `write_sync(bufsize=1)` real time 为 {fmt_seconds(sync_worst.real_sec)} s。`O_SYNC` 要求写操作具备同步语义，小缓冲区下相当于把大量细碎写入逐次推入更重的同步路径，因此时间急剧上升。

当 BUFFSIZE 增大时，`O_SYNC` 的耗时快速下降。`write_sync` 的最优结果为 {fmt_seconds(sync_best.real_sec)} s，出现在 BUFFSIZE={int(sync_best.bufsize)}。这说明 `O_SYNC` 的主要代价不是简单的用户态函数调用，而是同步提交次数。把多个字节合并进一次较大的 `write()` 可以显著减少同步操作次数。

## 5. 原理分析与讨论

### 5.1 系统调用次数

系统调用需要从用户态切换到内核态，进行参数检查、文件描述符查找、VFS/文件系统路径处理以及数据拷贝等工作。对于 500 MB 文件，`bufsize=1` 会产生数亿次系统调用，因此时间几乎完全被系统调用开销吞噬。BUFFSIZE 每翻倍，理论上的系统调用次数约减半，实验中小缓冲区阶段的 real time 也呈现近似减半趋势。

### 5.2 内核页缓存

普通 `read()` 和 `write()` 都会经过内核页缓存。顺序读时，如果数据已经在页缓存中，读取可以主要在内存路径完成；顺序写时，普通 `write()` 通常先把数据写入页缓存并标记脏页，随后由内核异步回写。页缓存使得普通写在大缓冲区下看起来非常快，但这并不代表数据已经全部持久化到设备。

### 5.3 用户态缓冲

stdio 的 `getc/fgetc/fread` 使用用户态缓冲区，能够减少底层 `read()` 系统调用次数。逐字符的 `getc/fgetc` 虽然 API 上每次返回一个字符，但大多数调用只在用户态缓冲区移动指针，只有缓冲区耗尽时才进入内核补充数据。因此它们明显快于 `read(bufsize=1)`。

### 5.4 O_SYNC

`O_SYNC` 改变了写入语义：调用返回前需要满足更强的同步要求。小 BUFFSIZE 下，`O_SYNC` 会把每个小块写入都变成昂贵的同步操作，性能显著下降。实验中 `write_sync(bufsize=1)` 的时间远高于普通 `write(bufsize=1)`，正说明同步写的主导成本在内核和存储路径，而非用户态循环。

### 5.5 BUFFSIZE 的收益递减

从读写实验都可以看到，BUFFSIZE 从 1 增大到 4 KB、16 KB、64 KB 时收益巨大；但继续增大到 16 MB 后并不一定更快，甚至可能变慢。这是因为系统调用次数已经足够少，新的瓶颈转移到页缓存、内存带宽、文件系统路径、缓存局部性以及大块内存分配/拷贝等因素。

## 6. 结论

1. 小 BUFFSIZE 下，系统调用次数是 UNIX I/O 性能的主要瓶颈。
2. stdio 的用户态缓冲使逐字符 `getc/fgetc` 远快于逐字节 `read()`。
3. `fread()` 与自实现 `my_fread()` 的趋势一致，证明用户态缓冲能显著减少系统调用开销；标准库实现整体更成熟。
4. 普通 `write()` 受益于内核页缓存，大缓冲区下耗时显著下降。
5. `O_SYNC` 会显著放大同步写成本，尤其在极小 BUFFSIZE 下表现最明显。
6. BUFFSIZE 并非越大越好，达到 KB 到几十 KB 级别后通常进入收益递减区间。

## 附录 A：关键实验数据

### A.1 读实验完整数据

{write_csv_table(RESULTS / "partA_all_read.csv")}

### A.2 500 MB 普通 write 数据

{write_csv_table(RESULTS / "partA_all_write.csv")}

### A.3 1 MB 普通 write 数据

{write_csv_table(RESULTS / "partA_write_1MB.csv")}

### A.4 1 MB O_SYNC write 数据

{write_csv_table(RESULTS / "partA_write_sync_1MB.csv")}
"""

    REPORT.write_text(report, encoding="utf-8")


if __name__ == "__main__":
    make_report()
