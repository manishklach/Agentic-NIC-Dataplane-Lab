#!/usr/bin/env python3
import json
import pathlib
import sys

import matplotlib.pyplot as plt


def load_results(path: pathlib.Path) -> dict:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def find_result(data: dict, mode: str) -> dict | None:
    for record in data.get("results", []):
        if record.get("mode") == mode:
            return record
    return None


def write_throughput_chart(records: list[dict], out_path: pathlib.Path) -> None:
    labels = []
    pps = []
    mbps = []

    for record in records:
        label = record["mode"]
        if label == "af_xdp_mock":
            label = "af_xdp_mock\n(simulated)"
        elif label == "kernel_udp_baseline":
            label = "kernel_udp_baseline\n(localhost)"
        labels.append(label)
        pps.append(record.get("packets_per_sec", 0.0))
        mbps.append(record.get("mb_per_sec", 0.0))

    fig, axes = plt.subplots(1, 2, figsize=(12, 5), constrained_layout=True)
    axes[0].bar(labels, pps, color=["#1f77b4", "#ff7f0e"])
    axes[0].set_title("Packets per second")
    axes[0].set_ylabel("pps")
    axes[0].grid(axis="y", alpha=0.3)

    axes[1].bar(labels, mbps, color=["#1f77b4", "#ff7f0e"])
    axes[1].set_title("Throughput")
    axes[1].set_ylabel("MB/s")
    axes[1].grid(axis="y", alpha=0.3)

    fig.suptitle("Local Path A vs Path B starter benchmark")
    out_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out_path, dpi=200)
    plt.close(fig)


def write_latency_chart(record: dict, out_path: pathlib.Path) -> bool:
    keys = ["avg_latency_us", "p50_latency_us", "p95_latency_us", "p99_latency_us"]
    if not all(key in record for key in keys):
        return False

    labels = ["avg", "p50", "p95", "p99"]
    values = [record["avg_latency_us"], record["p50_latency_us"], record["p95_latency_us"], record["p99_latency_us"]]

    fig, ax = plt.subplots(figsize=(8, 5), constrained_layout=True)
    ax.bar(labels, values, color="#1f77b4")
    ax.set_title("Kernel UDP localhost latency")
    ax.set_ylabel("Latency (us)")
    ax.grid(axis="y", alpha=0.3)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out_path, dpi=200)
    plt.close(fig)
    return True


def main() -> int:
    input_path = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path("results/latest.json")
    output_dir = pathlib.Path("diagrams")
    throughput_path = output_dir / "local-baseline-throughput.png"
    latency_path = output_dir / "local-baseline-latency.png"

    data = load_results(input_path)
    kernel = find_result(data, "kernel_udp_baseline")
    afxdp = find_result(data, "af_xdp_mock")
    if kernel is None or afxdp is None:
        raise SystemExit("results file must contain kernel_udp_baseline and af_xdp_mock entries")

    write_throughput_chart([kernel, afxdp], throughput_path)
    print(f"wrote throughput chart to {throughput_path}")

    if write_latency_chart(kernel, latency_path):
        print(f"wrote latency chart to {latency_path}")
    else:
        print("latency chart skipped: latency fields not present")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
