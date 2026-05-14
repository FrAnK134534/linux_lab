#!/usr/bin/env python3

from __future__ import annotations

import csv
from pathlib import Path
import textwrap

import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch
import pandas as pd


ROOT = Path(__file__).resolve().parents[1]
PART_B = ROOT / "partB"
RESULTS = ROOT / "results"
ENV = RESULTS / "env"
FIGURES = PART_B / "figures"
PAPER = PART_B / "paper.md"


SERVER_ORDER = ["poll", "threadpool", "epoll"]
CONNECTION_ORDER = [10, 100, 500]


def read_env(path: Path) -> str:
    if not path.exists():
        return "未记录"
    return path.read_text(encoding="utf-8", errors="replace").strip()


def load_results() -> pd.DataFrame:
    frames = []
    for server in SERVER_ORDER:
        for conn in CONNECTION_ORDER:
            path = RESULTS / f"partB_{server}_{conn}.csv"
            frames.append(pd.read_csv(path))
    df = pd.concat(frames, ignore_index=True)
    df["connections"] = df["connections"].astype(int)
    return df


def savefig(name: str) -> str:
    FIGURES.mkdir(parents=True, exist_ok=True)
    path = FIGURES / name
    plt.tight_layout()
    plt.savefig(path, dpi=180)
    plt.close()
    return f"figures/{name}"


def grouped_bar(df: pd.DataFrame, metric: str, ylabel: str, title: str, filename: str, logy: bool = False) -> str:
    pivot = df.pivot(index="connections", columns="server", values=metric).loc[CONNECTION_ORDER, SERVER_ORDER]
    ax = pivot.plot(kind="bar", figsize=(8.2, 4.8), width=0.78)
    ax.set_xlabel("Concurrent connections")
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.grid(True, axis="y", linestyle="--", alpha=0.35)
    if logy:
        ax.set_yscale("log")
    ax.legend(title="Server")
    for container in ax.containers:
        ax.bar_label(container, fmt="%.1f", fontsize=7, padding=2)
    return savefig(filename)


def plot_latency_500(df: pd.DataFrame) -> str:
    sub = df[df["connections"] == 500].set_index("server").loc[SERVER_ORDER]
    metrics = ["avg_ms", "p50_ms", "p95_ms", "p99_ms"]
    labels = ["avg", "p50", "p95", "p99"]
    ax = sub[metrics].plot(kind="bar", figsize=(8.2, 4.8), width=0.78)
    ax.set_xlabel("Server")
    ax.set_ylabel("Latency (ms, log)")
    ax.set_title("Latency distribution at 500 concurrent connections")
    ax.set_yscale("log")
    ax.grid(True, axis="y", linestyle="--", alpha=0.35)
    ax.legend(labels)
    return savefig("partB_latency_500.png")


def add_box(ax, xy, w, h, text, fc="#F7F7F7"):
    patch = FancyBboxPatch(
        xy,
        w,
        h,
        boxstyle="round,pad=0.02,rounding_size=0.03",
        linewidth=1.3,
        edgecolor="#333333",
        facecolor=fc,
    )
    ax.add_patch(patch)
    ax.text(xy[0] + w / 2, xy[1] + h / 2, text, ha="center", va="center", fontsize=10)


def arrow(ax, start, end):
    ax.annotate("", xy=end, xytext=start, arrowprops=dict(arrowstyle="->", lw=1.3, color="#333333"))


def plot_architecture() -> str:
    fig, ax = plt.subplots(figsize=(9, 4.8))
    ax.set_xlim(0, 10)
    ax.set_ylim(0, 5)
    ax.axis("off")

    add_box(ax, (0.4, 2.0), 1.4, 0.8, "Clients", "#E8F0FE")
    add_box(ax, (2.2, 2.0), 1.6, 0.8, "Acceptor\nThread", "#E6F4EA")
    add_box(ax, (4.4, 3.1), 1.8, 0.8, "I/O Thread 1\nepoll loop", "#FFF4E5")
    add_box(ax, (4.4, 2.0), 1.8, 0.8, "I/O Thread 2\nepoll loop", "#FFF4E5")
    add_box(ax, (4.4, 0.9), 1.8, 0.8, "I/O Thread N\nepoll loop", "#FFF4E5")
    add_box(ax, (7.0, 2.0), 1.8, 0.8, "Worker\nThread Pool", "#FCE8E6")
    add_box(ax, (8.9, 2.0), 0.8, 0.8, "App\nLogic", "#F3E8FD")

    arrow(ax, (1.8, 2.4), (2.2, 2.4))
    arrow(ax, (3.8, 2.4), (4.4, 3.5))
    arrow(ax, (3.8, 2.4), (4.4, 2.4))
    arrow(ax, (3.8, 2.4), (4.4, 1.3))
    arrow(ax, (6.2, 2.4), (7.0, 2.4))
    arrow(ax, (8.8, 2.4), (8.9, 2.4))
    arrow(ax, (7.0, 2.05), (6.2, 1.35))

    ax.text(5.3, 4.25, "non-blocking sockets + event dispatch", ha="center", fontsize=10)
    ax.text(7.4, 1.25, "business tasks / backpressure", ha="center", fontsize=10)
    return savefig("partB_architecture.png")


def format_table_cell(header: str, value: str) -> str:
    if header in {"total_time_sec", "throughput_req_s", "avg_ms", "p50_ms", "p95_ms", "p99_ms"}:
        try:
            return f"{float(value):.6f}" if header != "throughput_req_s" else f"{float(value):.2f}"
        except ValueError:
            return value
    return value


def markdown_table(df: pd.DataFrame) -> str:
    headers = [
        "server",
        "connections",
        "msg_size",
        "total_requests",
        "total_time_sec",
        "throughput_req_s",
        "avg_ms",
        "p50_ms",
        "p95_ms",
        "p99_ms",
    ]
    rows = []
    for row in df.sort_values(["connections", "server"]).itertuples(index=False):
        raw = {h: getattr(row, h) for h in headers}
        rows.append([format_table_cell(h, str(raw[h])) for h in headers])
    widths = [max(len(headers[i]), *(len(row[i]) for row in rows)) for i in range(len(headers))]

    def fmt(row: list[str]) -> str:
        return "| " + " | ".join(row[i].ljust(widths[i]) for i in range(len(row))) + " |"

    lines = [fmt(headers), "| " + " | ".join("-" * w for w in widths) + " |"]
    lines.extend(fmt(row) for row in rows)
    return "\n".join(lines)


def env_summary() -> str:
    uname = read_env(ENV / "uname.txt").splitlines()[0]
    lsb = read_env(ENV / "lsb_release.txt")
    cpu = read_env(ENV / "lscpu.txt")
    free = read_env(ENV / "free.txt")
    df = read_env(ENV / "df.txt")
    lsblk = read_env(ENV / "lsblk.txt")

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
    mem_line = next((line for line in free.splitlines() if line.startswith("Mem:")), "未记录")
    fs_line = df.splitlines()[-1] if df.splitlines() else "未记录"
    disk_note = "ROTA=0 SSD/NVMe devices"
    if "Dell DC NVMe" in lsblk:
        disk_note = "SSD/NVMe, including Dell DC NVMe PE8010 RI U.2 960GB"

    return textwrap.dedent(
        f"""
        | 项目 | 信息 |
        | --- | --- |
        | 操作系统 | {distro} |
        | 内核 | `{uname}` |
        | CPU | {cpu_model}，{cpu_count} logical CPUs |
        | 内存 | `{mem_line}` |
        | 文件系统 | `{fs_line}` |
        | 存储 | {disk_note} |
        """
    ).strip()


def make_paper() -> None:
    df = load_results()
    throughput_fig = grouped_bar(
        df,
        "throughput_req_s",
        "Throughput (requests/s)",
        "Throughput comparison",
        "partB_throughput.png",
    )
    avg_fig = grouped_bar(
        df,
        "avg_ms",
        "Average latency (ms, log)",
        "Average latency comparison",
        "partB_avg_latency.png",
        logy=True,
    )
    p99_fig = grouped_bar(
        df,
        "p99_ms",
        "P99 latency (ms, log)",
        "P99 latency comparison",
        "partB_p99_latency.png",
        logy=True,
    )
    latency_500_fig = plot_latency_500(df)
    arch_fig = plot_architecture()

    best_throughput = df.loc[df["throughput_req_s"].idxmax()]
    epoll_100 = df[(df["server"] == "epoll") & (df["connections"] == 100)].iloc[0]
    thread_500 = df[(df["server"] == "threadpool") & (df["connections"] == 500)].iloc[0]

    paper = f"""---
title: "高吞吐量 I/O 服务器模型设计与性能分析"
author: "系统程序设计 Lab 1"
date: "2026-05-14"
---

# 高吞吐量 I/O 服务器模型设计与性能分析

## 摘要

高并发服务器的核心问题是如何在大量连接之间高效分配 CPU、内核 I/O 事件和用户态处理逻辑。本文围绕阻塞式 I/O、多路复用 I/O、异步 I/O 和多线程 I/O 的工作机制进行分析，并实现了三种 TCP echo server：基于 `poll()` 的单线程多路复用服务器、基于线程池的并发服务器，以及 Linux `epoll` 服务器。实验使用自编写的 `client_bench` 产生 10、100、500 个并发连接，每个连接发送 1000 个 64 字节 echo 请求，并统计吞吐量、平均延迟和 P99 延迟。实验结果表明，在 100 并发下 `epoll` 取得最高吞吐量 118200.15 req/s；在 500 并发下线程池模型达到最高吞吐量 119708.45 req/s。结果说明 I/O 模型的理论优势需要结合具体实现、负载规模和调度方式分析。最后，本文提出一种基于主从 Reactor、`epoll` 和 worker 线程池组合的高吞吐服务器架构。

**关键词**：UNIX I/O；poll；epoll；线程池；Reactor；高吞吐服务器

## 1. 引言

网络服务器需要同时处理大量客户端连接。最直接的阻塞式模型实现简单，但一个线程在一次阻塞 `read()` 或 `write()` 中等待时，无法同时处理其他连接。随着并发连接数上升，单纯阻塞模型会面临线程数量膨胀、上下文切换增加、内存占用上升等问题。

UNIX/Linux 提供了多种 I/O 并发机制：`select()`、`poll()` 和 `epoll` 属于 I/O 多路复用；POSIX AIO 与 Linux `io_uring` 代表异步 I/O 方向；线程池模型则通过多个 worker 并行处理连接。不同模型不是简单的优劣关系，而是在实现复杂度、连接规模、业务逻辑成本、系统调用次数、调度开销之间做权衡。

本文的目标是：第一，梳理常见 I/O 模型的工作原理；第二，通过 echo server 实验比较 `poll`、`epoll` 和线程池模型在相同负载下的吞吐量和延迟；第三，基于实验结果设计一个更适合高吞吐场景的服务器架构。

## 2. 相关工作

Stevens 的《UNIX Network Programming》系统介绍了阻塞 I/O、非阻塞 I/O 和 I/O 多路复用模型，是网络服务器编程的经典参考。APUE 对 UNIX 文件描述符、系统调用和高级 I/O 机制有更底层的讨论。Linux man-pages 给出了 `poll(2)` 和 `epoll(7)` 的接口语义。实际系统中，Nginx 采用事件驱动和 worker 进程模型，Redis 长期使用单线程事件循环处理网络 I/O，这些系统都体现了高吞吐服务器中“事件分发 + 非阻塞 I/O”的设计思想。

近年来，Linux `io_uring` 进一步降低异步 I/O 提交和完成路径上的系统调用开销。它通过共享 submission queue 和 completion queue 让用户态批量提交请求、批量收割完成事件。本文没有实现 `io_uring` 服务器，因为该项是实验可选加分项，且当前实验已经覆盖最低要求的 `poll` 和线程池模型，并额外实现了 `epoll` 对比；但在原理分析中仍将其作为异步 I/O 的代表机制讨论。

## 3. I/O 模型原理分析

### 3.1 阻塞式 I/O

阻塞式 I/O 中，线程调用 `accept()`、`read()` 或 `write()` 后，如果条件不满足，就会进入睡眠等待。例如客户端没有发送数据时，`read()` 会阻塞；发送缓冲区满时，`write()` 也可能阻塞。其优点是程序结构简单，容易理解；缺点是单线程无法同时服务大量连接。若采用“每连接一线程”，则会带来线程栈内存、调度和上下文切换成本。

典型流程如下：

```text
user thread -> blocking read/write -> kernel waits for I/O -> wakeup -> user handles data
```

### 3.2 select / poll 多路复用

`select()` 和 `poll()` 允许一个线程同时等待多个 fd 的事件。应用把一组 fd 交给内核，内核在其中任意 fd 就绪时返回，用户态再检查哪些 fd 可读或可写。

`poll()` 相比 `select()` 没有固定的 `FD_SETSIZE` 位图限制，接口也更直接。但 `poll()` 每次调用仍需要传入整个 `pollfd` 数组，返回后应用通常需要线性扫描数组来找就绪 fd。因此当总连接数很大、活跃连接较少时，`poll()` 的 O(n) 扫描成本会越来越明显。

### 3.3 epoll

`epoll` 是 Linux 提供的可扩展 I/O 多路复用机制。它把“注册 fd”和“等待事件”拆开：应用先通过 `epoll_ctl()` 把 fd 加入内核维护的 interest list，再通过 `epoll_wait()` 获取 ready list 中的活跃事件。相对于 `poll()` 每次传入整个 fd 数组，`epoll` 更适合大量连接、少量活跃的场景。

`epoll` 有水平触发（LT）和边缘触发（ET）两种常见模式。LT 模式下，只要 fd 仍处于可读或可写状态，`epoll_wait()` 就可能持续返回该事件，编程模型较安全。ET 模式只在状态变化时通知一次，减少重复通知，但要求应用使用非阻塞 fd，并循环读写直到 `EAGAIN`，否则可能遗漏数据。本实验的 `epoll_server` 使用 LT 思路，优先保证实现稳定。

### 3.4 异步 I/O

异步 I/O 的目标是让应用提交 I/O 请求后不阻塞等待，等内核完成后再通过信号、回调或完成队列通知用户态。POSIX AIO 提供 `aio_read()`、`aio_write()` 等接口，但在 Linux 网络 I/O 实践中使用并不广泛。Linux `io_uring` 通过共享环形队列提交和完成请求，减少系统调用次数并支持批量操作，在高性能存储和网络场景中有很强潜力。

多路复用和异步 I/O 的本质区别在于：多路复用通常通知“fd 已就绪”，应用随后自己执行 `read/write`；异步 I/O 则提交“具体 I/O 操作”，内核完成后通知“操作已完成”。

### 3.5 多线程 I/O

多线程模型通过多个 worker 并发处理连接或任务。线程池避免了每次请求都创建线程的开销，也能利用多核 CPU。代价是需要任务队列、互斥锁和条件变量，可能出现锁竞争、队列等待和上下文切换。对于 echo 这类简单 I/O 任务，线程池模型可能因为并行处理多个连接而取得不错吞吐；但在极大连接数下，线程数量和阻塞 I/O 仍然需要谨慎控制。

## 4. 实验设计与实现

### 4.1 实验环境

{env_summary()}

### 4.2 程序实现

本文实现了三个 TCP echo server：

- `poll_server`：单线程 `poll()` 事件循环，维护 `pollfd` 数组，监听连接和客户端读事件。
- `threadpool_server`：主线程负责 `accept()`，固定数量 worker 线程从队列取连接并使用阻塞 I/O 处理 echo。
- `epoll_server`：Linux-only，监听 fd 和客户端 fd 均使用非阻塞模式，并通过 `epoll_wait()` 获取活跃事件。

压测程序 `client_bench` 使用多个客户端线程，每个线程维护一个 TCP 连接，循环发送固定长度请求并等待 echo 响应。每个请求记录一次延迟，最终输出总吞吐量、平均延迟、P50、P95 和 P99。

### 4.3 实验参数

```text
message size              = 64 bytes
connections               = 10, 100, 500
requests per connection   = 1000
total requests            = connections * 1000
poll_server port          = 8080
threadpool_server port    = 8081
epoll_server port         = 8082
threadpool workers        = 4
```

实验运行命令记录在 `docs/EXPERIMENT_FLOW.md` 中。

## 5. 实验结果与分析

### 5.1 原始结果表

{markdown_table(df)}

### 5.2 吞吐量

![Throughput comparison]({throughput_fig})

吞吐量结果显示，10 并发时 `epoll` 和 `poll` 接近，分别为 80856.10 req/s 与 79925.39 req/s，线程池为 68868.60 req/s。100 并发时 `epoll` 达到 118200.15 req/s，是该并发下最高值。500 并发时线程池达到 119708.45 req/s，是本实验中的最高吞吐量；`poll` 为 113053.79 req/s，`epoll` 为 100766.96 req/s。

这个结果说明，理论上更可扩展的 I/O 模型并不必然在所有实验规模下胜出。本实验中的 echo 逻辑非常轻量，且 `epoll_server` 是单线程实现，所有连接的读取和写回都在一个事件循环中完成；线程池模型则可以让多个 worker 同时处理连接，在 500 并发下获得了更高吞吐。

### 5.3 平均延迟

![Average latency comparison]({avg_fig})

平均延迟随连接数增加整体上升。`poll` 在 500 并发下平均延迟为 4.186389 ms，`epoll` 为 4.850345 ms，线程池为 2.036434 ms。线程池在高并发下平均延迟较低，说明多个 worker 并行处理 echo 请求降低了单事件循环排队压力。

### 5.4 P99 延迟

![P99 latency comparison]({p99_fig})

P99 反映尾部延迟。`poll` 在 500 并发下 P99 达到 7.717189 ms，`epoll` 为 5.959882 ms，而线程池为 0.049261 ms。需要注意的是，线程池数据中平均延迟高于 P99，这是因为每个 worker 以连接为单位处理请求，少数连接可能在进入 worker 前经历较长队列等待，这些极端等待拉高平均值；但如果它们占全部请求的比例不足 1%，就不会体现在 P99 中。

![Latency distribution at 500 connections]({latency_500_fig})

### 5.5 结果讨论

`poll` 的实现简单，但每轮需要扫描 fd 数组。连接数从 100 增加到 500 后，吞吐量没有明显增长，平均延迟和 P99 都上升，符合线性扫描成本和单线程处理能力受限的预期。

`epoll` 在 100 并发下吞吐量最高，体现了只返回活跃事件的优势。但在 500 并发下低于 `poll` 和线程池，可能与本实验实现有关：服务器使用单个事件循环，写回路径仍在同一线程中完成；echo 请求很小，事件分发收益可能被实现细节、调度和缓存效应抵消。

线程池模型在 500 并发下吞吐量最高，说明在当前机器 64 logical CPUs 的环境中，即使只使用 4 个 worker，也能通过并行连接处理获得收益。不过线程池模型并非没有代价：它需要任务队列、锁和条件变量；当连接数进一步增大或请求处理时间更长时，线程数量、队列长度和调度策略会成为关键瓶颈。

## 6. 高吞吐量服务器架构设计

基于以上分析，本文推荐使用“主从 Reactor + epoll + worker thread pool”的组合架构。

![Recommended high-throughput server architecture]({arch_fig})

设计要点如下：

1. acceptor 线程只负责监听端口和接收连接，避免复杂业务阻塞新连接接入。
2. 多个 I/O 线程各自维护一个 `epoll` 实例，连接按 fd、哈希或负载均衡策略分配到不同 I/O 线程。
3. I/O 线程使用非阻塞 socket，只负责事件分发、协议解析和轻量级读写。
4. CPU 密集或可能阻塞的业务逻辑交给 worker 线程池处理。
5. 每个连接维护输入/输出缓冲区；当发送缓冲区积压过多时启用 backpressure，避免慢客户端耗尽内存。
6. 对于大规模部署，可以使用多进程或多实例绑定 CPU/NUMA 节点，减少跨 NUMA 访问和锁竞争。

这种架构融合了 `epoll` 处理大量连接的能力和线程池利用多核 CPU 的能力。它也解释了实验结果中的现象：单线程事件循环适合轻量事件分发，但在处理逻辑或写回路径变重时，需要 worker 并行化来保持吞吐。

## 7. 局限性

本实验仍有一些限制：

- echo 协议过于简单，不能完全代表真实业务服务器。
- 每种并发规模只记录一次结果，没有多次重复取平均值。
- 测试发生在本机回环地址 `127.0.0.1`，不包含真实网络延迟和丢包。
- 本文比较了 10、100、500 三档并发连接，但没有继续扫描服务器可承受的绝对最大连接数。
- 本文没有单独采集 CPU 使用率和内存占用曲线，后续可使用 `pidstat`、`top`、`perf` 等工具补充。
- `epoll_server` 使用简化 LT 模式实现，没有进一步实现 ET 模式、输出缓冲队列和多 Reactor。
- 本文没有实现 `io_uring_server`，仅在原理层面对异步 I/O 进行讨论。

这些限制不影响本实验对不同 I/O 模型趋势的观察，但在正式生产级性能评估中需要更完整的负载模型和多轮统计。

## 8. 结论

本文分析并实现了多种高并发 I/O 服务器模型。实验表明，`poll` 简单直接，但在连接数增加时会受到线性扫描和单线程处理能力限制；`epoll` 在 100 并发下取得最高吞吐，但简化单线程实现并不保证所有场景下优于其他模型；线程池模型在 500 并发下取得最高吞吐，说明多核并行对当前 echo 负载有效。

综合原理和实验结果，高吞吐服务器不应机械选择单一模型。更合理的工程方案是使用 `epoll` 或类似事件机制进行连接事件分发，并把耗时业务交给 worker 线程池，从而在大量连接管理和多核计算之间取得平衡。

## 参考文献

[1] W. Richard Stevens, Stephen A. Rago. *Advanced Programming in the UNIX Environment*. Addison-Wesley.

[2] W. Richard Stevens, Bill Fenner, Andrew M. Rudoff. *UNIX Network Programming, Volume 1: The Sockets Networking API*. Addison-Wesley.

[3] Linux man-pages project. `poll(2)` and `epoll(7)` manual pages.

[4] Nginx Documentation. Nginx architecture and event processing model.

[5] Redis Documentation. Redis event loop and networking implementation notes.
"""

    PAPER.write_text(paper, encoding="utf-8")


if __name__ == "__main__":
    make_paper()
