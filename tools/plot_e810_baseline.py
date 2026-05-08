#!/usr/bin/env python3
import json
import pathlib
import sys

import matplotlib.pyplot as plt


def load_results(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def filter_records(records, metric, cores):
    by_path = {}
    for record in records:
        if record["cores"] != cores:
            continue
        by_path.setdefault(record["path"], []).append(record)

    for path in by_path:
        by_path[path].sort(key=lambda r: r["size_bytes"])
    return by_path


def plot_metric(ax, grouped, metric, title):
    for path, records in grouped.items():
        x_values = [record["size_bytes"] for record in records]
        y_values = [record[metric] for record in records]
        ax.plot(x_values, y_values, marker="o", label=path)
    ax.set_xscale("log", base=2)
    ax.set_xticks([64, 256, 1024, 4096, 8192])
    ax.set_xticklabels(["64B", "256B", "1KB", "4KB", "8KB"])
    ax.set_title(title)
    ax.set_xlabel("Message size")
    ax.grid(True, alpha=0.3)


def main():
    input_path = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path("results/e810-baseline-2026-05-08.json")
    output_path = pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else pathlib.Path("results/e810-baseline-2026-05-08.png")
    data = load_results(input_path)

    core_counts = [1, 8, 32]
    fig, axes = plt.subplots(len(core_counts), 2, figsize=(14, 12), constrained_layout=True)

    for row, cores in enumerate(core_counts):
        grouped = filter_records(data["results"], "p99_us", cores)
        plot_metric(axes[row][0], grouped, "p99_us", f"p99 latency ({cores} cores)")
        axes[row][0].set_ylabel("Latency (us)")

        grouped = filter_records(data["results"], "throughput_gbps", cores)
        plot_metric(axes[row][1], grouped, "throughput_gbps", f"Throughput ({cores} cores)")
        axes[row][1].set_ylabel("Gbps")

    handles, labels = axes[0][0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center", ncol=3)
    fig.suptitle("Intel E810 Baseline: TCP vs AF_XDP vs RDMA", fontsize=16)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, dpi=200)
    print(f"wrote plot to {output_path}")


if __name__ == "__main__":
    main()
