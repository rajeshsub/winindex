#!/usr/bin/env python3
"""Renders a markdown trend table from accumulated Google Benchmark JSON runs.

Reads every *.json file in the given directory (Google Benchmark's
--benchmark_out=... format, one file per CI run, filenames carry a timestamp so
sorted order is chronological), keeps the last 10 runs, and for each benchmark
name prints its real_time across those runs plus the delta from the previous run.
Intended to be piped into $GITHUB_STEP_SUMMARY.
"""

import glob
import json
import os
import sys


def load_runs(results_dir):
    files = sorted(glob.glob(os.path.join(results_dir, "*.json")))
    runs = []
    for path in files[-10:]:
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
        label = os.path.splitext(os.path.basename(path))[0]
        by_name = {b["name"]: b for b in data.get("benchmarks", [])}
        runs.append((label, by_name))
    return runs


def main():
    if len(sys.argv) != 2:
        print("usage: render_benchmark_trend.py <benchmark-results-dir>", file=sys.stderr)
        return 1

    runs = load_runs(sys.argv[1])
    if not runs:
        print("No benchmark results found.")
        return 0

    names = []
    seen = set()
    for _, by_name in runs:
        for name in by_name:
            if name not in seen:
                seen.add(name)
                names.append(name)

    print("## Benchmark trend (last {} runs)\n".format(len(runs)))
    header = "| benchmark | " + " | ".join(label for label, _ in runs) + " | delta (last two) |"
    sep = "|" + "---|" * (len(runs) + 2)
    print(header)
    print(sep)

    for name in names:
        row = [name]
        times = []
        for _, by_name in runs:
            b = by_name.get(name)
            if b is None:
                row.append("-")
                times.append(None)
            else:
                t = b.get("real_time")
                unit = b.get("time_unit", "")
                row.append("{:.3f} {}".format(t, unit) if t is not None else "-")
                times.append(t)

        delta = "-"
        valid = [t for t in times if t is not None]
        if len(valid) >= 2:
            prev, last = valid[-2], valid[-1]
            if prev:
                pct = (last - prev) / prev * 100.0
                delta = "{:+.1f}%".format(pct)
        row.append(delta)
        print("| " + " | ".join(row) + " |")

    return 0


if __name__ == "__main__":
    sys.exit(main())
