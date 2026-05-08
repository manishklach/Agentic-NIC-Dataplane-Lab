# Benchmark Plan

The point of this benchmark plan is to compare networking paths under `agent-shaped` traffic, not just ideal lab streams.

## Compare These Paths

1. kernel TCP baseline
2. kernel TCP + `io_uring`
3. kernel TCP + `io_uring` zero-copy Rx if available
4. `AF_XDP` on selected queues
5. `RDMA` for bulk lanes

The point of the comparison is not to crown one universal winner. It is to identify which path wins for which workload shape and at what operational cost.

## Workload Classes

### Class A: Agent RPC

- payloads: `256 B` to `8 KB`
- request fan-out: `1` to `32`
- response streaming: optional
- focus: tail latency and CPU cost
- likely path winner: kernel TCP, then `io_uring` where supported

Release blocker for wider claims:

- explicitly prove `AF_XDP` is not slower than Path A for sub-`512 B` RPCs before recommending it for hot small-message services
- record `XDP` redirect overhead and compare it to socket-path wakeup and copy cost
- treat `64 B`, `128 B`, `256 B`, and `512 B` as mandatory comparison points, not optional extras

### Class B: Retrieval / Memory Service

- payloads: `1 KB` to `64 KB`
- mixed reads and writes
- bursty arrivals
- focus: queue pressure and copy overhead
- likely path winner: kernel TCP or `AF_XDP`, depending on service heat and queue stability

### Class C: Bulk East-West State

- payloads: `64 KB` to `4 MB`
- repeated transfer patterns
- focus: throughput and host CPU per GB
- likely path winner: `RDMA` if setup and fabric costs are acceptable

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
- `bpftrace` guardian and latency-preemption scripts for bounded-autonomy validation

## Success Criteria

- lower p99 latency on Class A without exploding complexity
- lower CPU cost on Class B hot services
- clear throughput and CPU win on Class C bulk movement
- stable behavior under burst and fan-out
- `AF_XDP` must not regress sub-`512 B` p99 latency relative to Path A without a clearly justified CPU savings story
- guardian intervention must be shown not to violate tail-latency SLOs on protected service classes

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

## Guardian Validation Addendum

The repo should not claim safe bounded autonomy without showing observability around guardian intervention. Minimum evidence:

- trace when guardian decisions preempt or clamp dataplane actions
- correlate intervention windows with service latency histograms
- prove fail-safe entry does not destroy protected control-plane traffic

Starter scripts live in:

- [`../tools/bpftrace/guardian_preemption.bt`](../tools/bpftrace/guardian_preemption.bt)
- [`../tools/bpftrace/guardian_tail_latency_guard.bt`](../tools/bpftrace/guardian_tail_latency_guard.bt)
