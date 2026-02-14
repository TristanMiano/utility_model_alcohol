#!/usr/bin/env python3
import argparse
import csv
from collections import defaultdict

import matplotlib.pyplot as plt


def load_histogram_csv(path):
    data = defaultdict(list)
    with open(path, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        required = {"metric", "bin", "left", "right", "count"}
        if set(reader.fieldnames or []) != required:
            missing = required - set(reader.fieldnames or [])
            raise ValueError(f"CSV missing expected columns: {sorted(missing)}")
        for row in reader:
            metric = row["metric"]
            data[metric].append(
                (
                    int(row["bin"]),
                    float(row["left"]),
                    float(row["right"]),
                    int(row["count"]),
                )
            )

    for metric in data:
        data[metric].sort(key=lambda x: x[0])
    return data


def plot_histograms(data, output_path=None):
    metrics = list(data.keys())
    if not metrics:
        raise ValueError("No histogram rows found.")

    cols = 2
    rows = (len(metrics) + cols - 1) // cols
    fig, axes = plt.subplots(rows, cols, figsize=(14, 3.8 * rows), squeeze=False)

    for idx, metric in enumerate(metrics):
        ax = axes[idx // cols][idx % cols]
        rows_data = data[metric]
        lefts = [r[1] for r in rows_data]
        rights = [r[2] for r in rows_data]
        counts = [r[3] for r in rows_data]
        widths = [max(1e-12, r - l) for l, r in zip(lefts, rights)]
        ax.bar(lefts, counts, width=widths, align="edge", alpha=0.8, edgecolor="black", linewidth=0.3)
        ax.set_title(metric)
        ax.set_xlabel("Value")
        ax.set_ylabel("Count")

    for idx in range(len(metrics), rows * cols):
        axes[idx // cols][idx % cols].axis("off")

    fig.tight_layout()
    if output_path:
        fig.savefig(output_path, dpi=180)
        print(f"Saved histogram figure: {output_path}")
    else:
        plt.show()


def main():
    parser = argparse.ArgumentParser(description="Plot histogram CSV exported by sim.cpp")
    parser.add_argument("hist_csv", help="Path to histogram CSV produced by --hist-data-out")
    parser.add_argument("--out", help="Optional output image path (e.g., hist.png)")
    args = parser.parse_args()

    data = load_histogram_csv(args.hist_csv)
    plot_histograms(data, args.out)


if __name__ == "__main__":
    main()
