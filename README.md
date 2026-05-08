# Agentic NIC Dataplane Lab

`Agentic-NIC-Dataplane-Lab` is a Linux-first reference repo for building and benchmarking a split data-plane architecture for agentic AI systems.

The core idea is simple:

- use the kernel TCP stack for most agent RPC
- use `io_uring` zero-copy receive where available to reduce copy overhead
- use `AF_XDP` for the hottest packet paths
- use `RDMA` only for bulk east-west movement that can amortize setup cost

This repo turns that idea into something practical:

- an opinionated reference architecture
- Linux kernel and NIC driver tuning guidance
- vendor-specific notes for Intel and Broadcom
- a benchmark plan for comparing sockets, `AF_XDP`, and `RDMA`
- starter code skeletons for `AF_XDP` and `RDMA`

## Why this exists

Agentic AI is not just model inference. It is a coordination workload:

- many small RPCs
- retrieval and state fetches
- tool execution
- policy checks
- fan-out and fan-in
- retries and streaming

That shifts bottlenecks toward:

- CPU time in the networking and storage path
- packet steering and queue placement
- copy overhead
- memory registration and pinning
- east-west service traffic

This repo proposes a `tri-path` architecture that matches those realities.

## Tri-Path Architecture

1. `Path A: Kernel RPC`
Use the normal kernel TCP stack for the majority of agent traffic. Add queue steering, `busy_poll`, and `io_uring` zero-copy Rx where supported.

2. `Path B: AF_XDP Fast Path`
Use `XDP` + `AF_XDP` only for the hottest service endpoints such as request routers, retrieval front doors, schedulers, or token gateways.

3. `Path C: RDMA Bulk Path`
Use `RDMA` for bulk state movement, vector/index sync, checkpoint transfer, GPU-adjacent data flow, or repeated shard-to-shard transport.

## Repo Layout

- [`docs/reference-architecture.md`](C:/Users/ManishKL/Documents/Playground/Agentic-NIC-Dataplane-Lab/docs/reference-architecture.md)
- [`docs/kernel-driver-tuning.md`](C:/Users/ManishKL/Documents/Playground/Agentic-NIC-Dataplane-Lab/docs/kernel-driver-tuning.md)
- [`docs/benchmark-plan.md`](C:/Users/ManishKL/Documents/Playground/Agentic-NIC-Dataplane-Lab/docs/benchmark-plan.md)
- [`diagrams/tri-path-agentic-dataplane.mmd`](C:/Users/ManishKL/Documents/Playground/Agentic-NIC-Dataplane-Lab/diagrams/tri-path-agentic-dataplane.mmd)
- [`src/af_xdp/main.c`](C:/Users/ManishKL/Documents/Playground/Agentic-NIC-Dataplane-Lab/src/af_xdp/main.c)
- [`src/rdma/verbs_ping.c`](C:/Users/ManishKL/Documents/Playground/Agentic-NIC-Dataplane-Lab/src/rdma/verbs_ping.c)
- [`scripts/benchmark-matrix.sh`](C:/Users/ManishKL/Documents/Playground/Agentic-NIC-Dataplane-Lab/scripts/benchmark-matrix.sh)

## Vendor Recommendation

If you want one practical Linux-first starting point:

- `Intel E810/E830`
- `ice` for Ethernet, queueing, and `AF_XDP`
- `irdma` for RDMA

If you are standardized on Broadcom:

- `bnxt_en` for Ethernet
- `bnxt_re` for RoCE

The repo explains where each stack fits and what to benchmark before deciding.

## Early Principles

- do not force `RDMA` onto every flow
- do not bypass the kernel for traffic that benefits from normal TCP semantics
- reserve `AF_XDP` for the few services where CPU and latency savings justify complexity
- align queues and CPU affinity deliberately
- benchmark with agent-shaped traffic, not only large synthetic streams

## Next Steps

1. Fill in the benchmark harnesses with your target NIC and kernel version.
2. Add perf, BPF, and NIC counters to every run.
3. Compare throughput, tail latency, CPU cost, and operational complexity by path.
4. Promote only the paths that win on real agentic workloads.
