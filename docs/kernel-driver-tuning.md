# Kernel and Driver Tuning

This is a starting checklist for Linux hosts that will carry mixed agentic AI traffic.

## Universal Host Tuning

- pin IRQs intentionally instead of accepting default spreading
- align NIC queue count to real worker parallelism
- enable `RSS` and confirm the indirection table is sane
- use `XPS` so transmit queues line up with worker CPUs
- validate `RFS` only if your service pattern benefits from it
- disable power-saving states that hurt latency consistency on dedicated hosts
- size socket buffers and backlog for bursty fan-out traffic
- capture `perf`, `ethtool -S`, and `bpftool prog` stats during every run

## Kernel RPC Path

- test `busy_poll` and `busy_read` carefully
- evaluate `io_uring` receive paths if the NIC supports zero-copy receive features
- use `SO_REUSEPORT` listeners for per-core scaling
- keep TLS termination placement explicit; do not hide its CPU cost

## AF_XDP Path

- dedicate queues instead of sharing with general-purpose traffic
- keep queue-to-core affinity fixed
- use XDP only for traffic classes that are stable and hot
- start with polling loops that prioritize observability over cleverness
- validate zero-copy support before assuming it exists

## RDMA Path

- isolate RDMA completion work from ordinary service IRQs
- pre-register memory for steady-state benchmarks
- record registration and teardown cost separately from transfer cost
- validate loss behavior and congestion control on RoCE fabrics
- do not compare RDMA against sockets without accounting for pinned-memory overhead

## Intel: `ice` + `irdma`

Recommended when you need one coherent stack for:

- kernel TCP
- `AF_XDP`
- RDMA

Checklist:

- confirm firmware and driver support for `AF_XDP` zero-copy
- validate queue counts and MSI-X vector placement
- check devlink parameters for RoCE enablement where relevant
- keep `ice` and `irdma` versions compatible

## Broadcom: `bnxt_en` + `bnxt_re`

Recommended when:

- you already run Broadcom NICs widely
- your RoCE environment is vendor-tested and stable

Checklist:

- validate firmware bundle consistency early
- confirm RoCE library and driver versions match
- test XDP-related features separately from RDMA bring-up
- measure CPU overhead on mixed traffic, not only RoCE microbenchmarks

## Metrics That Matter

- p50 and p99 latency
- host CPU per GB transferred
- instructions per request
- tail latency under burst
- packet drops and queue overruns
- copy count on the critical path
- registration overhead for RDMA flows
