#!/usr/bin/env bash
set -euo pipefail

echo "agentic-nic-lab benchmark matrix"
echo
echo "Planned paths:"
echo "  1. kernel TCP baseline"
echo "  2. kernel TCP + io_uring"
echo "  3. kernel TCP + io_uring zero-copy Rx"
echo "  4. AF_XDP selected queues"
echo "  5. RDMA bulk path"
echo
echo "For each run, capture:"
echo "  - kernel version"
echo "  - NIC model and firmware"
echo "  - driver versions"
echo "  - queue counts and affinity"
echo "  - workload class"
echo "  - p50 / p99 / max latency"
echo "  - throughput"
echo "  - CPU utilization"
echo "  - NIC counters and drops"
