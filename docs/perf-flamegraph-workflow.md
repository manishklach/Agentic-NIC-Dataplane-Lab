# Perf And Flamegraph Workflow

This repo is not only about architecture diagrams. It should also be easy to profile where time is going when comparing `Path A` and `Path B`.

## Why profile this lab

The central thesis is that `agentic workloads` can become dominated by networking and orchestration overhead:

- socket handling
- wakeups
- softirq processing
- userspace polling
- queueing jitter
- copy overhead

Even one profile that separates `softirq` time from userspace polling time makes the lab more credible.

## Helper script

Use [`../tools/perf_capture.sh`](../tools/perf_capture.sh):

```bash
sudo ./tools/perf_capture.sh --output-dir perf/kernel-udp -- ./build/udp_client --host 127.0.0.1 --port 9000 --packet-size 256 --count 2000
sudo ./tools/perf_capture.sh --output-dir perf/af-xdp-mock -- ./build/af_xdp_main --mock --packet-size 256 --count 2000
```

The helper writes:

- `perf.data`
- `perf-report.txt`
- `perf-script.txt`

If Brendan Gregg's `FlameGraph` repository is available locally, the same helper can also emit:

- `perf.folded`
- `flamegraph.svg`

Example:

```bash
git clone https://github.com/brendangregg/FlameGraph.git ~/FlameGraph
sudo ./tools/perf_capture.sh --output-dir perf/kernel-udp --flamegraph-dir ~/FlameGraph -- ./build/udp_client --host 127.0.0.1 --port 9000 --packet-size 256 --count 2000
```

## What to look for

For `kernel_udp_baseline`, look for:

- `udp_sendmsg`
- `udp_recvmsg`
- `ip_local_deliver`
- `net_rx_action`
- `__softirqentry_text_start`
- scheduler wakeup and context-switch cost

For `AF_XDP` mock mode, remember:

- it is not a real hardware dataplane benchmark
- it is only useful for validating local workflow and output format
- any profile is measuring mock userspace loop cost, not NIC zero-copy behavior

For future real `AF_XDP` runs, the interesting comparison is:

- kernel softirq work versus userspace polling work
- queue wakeups versus busy loops
- throughput gains versus CPU burn

## Minimal research framing

The profiling path is meant to help answer questions like:

- At what packet sizes do kernel socket costs dominate?
- When does polling shift work out of softirq and into userspace spin?
- Does lower latency come from real dataplane efficiency or just more CPU burn?
- Can queue affinity reduce jitter without hurting fairness?
