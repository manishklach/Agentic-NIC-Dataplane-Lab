# Benchmark Plan

The point of this benchmark plan is to compare networking paths under `agent-shaped` traffic, not just ideal lab streams.

## Compare These Paths

1. kernel TCP baseline
2. kernel TCP + `io_uring`
3. kernel TCP + `io_uring` zero-copy Rx if available
4. `AF_XDP` on selected queues
5. `RDMA` for bulk lanes

## Workload Classes

### Class A: Agent RPC

- payloads: `256 B` to `8 KB`
- request fan-out: `1` to `32`
- response streaming: optional
- focus: tail latency and CPU cost

### Class B: Retrieval / Memory Service

- payloads: `1 KB` to `64 KB`
- mixed reads and writes
- bursty arrivals
- focus: queue pressure and copy overhead

### Class C: Bulk East-West State

- payloads: `64 KB` to `4 MB`
- repeated transfer patterns
- focus: throughput and host CPU per GB

## Test Variables

- single flow vs many concurrent flows
- one NUMA node vs cross-NUMA placement
- IRQ and worker CPU alignment
- zero-copy enabled vs disabled
- memory pre-registration vs on-demand registration
- TLS on vs off for kernel RPC path

## Instrumentation

- `perf stat`
- `perf record`
- `ethtool -S`
- `bpftool prog show`
- `/proc/softirqs`
- `sar -n DEV`
- NIC vendor counters

## Success Criteria

- lower p99 latency on Class A without exploding complexity
- lower CPU cost on Class B hot services
- clear throughput and CPU win on Class C bulk movement
- stable behavior under burst and fan-out

## Reporting Format

For every run, record:

- kernel version
- NIC model
- firmware version
- driver versions
- queue counts
- RSS/XPS settings
- IRQ placement
- workload class
- median, p99, and max latency
- throughput
- CPU utilization by core
- dropped packets or CQ errors

## Recommended First Comparison

1. `ice` + kernel TCP
2. `ice` + `io_uring`
3. `ice` + `AF_XDP`
4. `ice` + `irdma`

Then repeat for:

1. `bnxt_en` + kernel TCP
2. `bnxt_en` + XDP path if supported as needed
3. `bnxt_re` for RoCE bulk path
