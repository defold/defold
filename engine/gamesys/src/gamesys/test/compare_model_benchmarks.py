#!/usr/bin/env python3
"""Alternate headless model benchmark binaries and summarize their CSV samples."""

import argparse
import csv
import json
from pathlib import Path
import statistics
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--runtime", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--instances", type=int, default=10000)
    parser.add_argument("--frames", type=int, default=60)
    parser.add_argument("--samples", type=int, default=7)
    parser.add_argument("--warmup", type=int, default=30)
    parser.add_argument("--runs", type=int, default=3)
    args = parser.parse_args()
    if min(args.instances, args.frames, args.samples, args.runs) < 1 or args.warmup < 0:
        parser.error("instances, frames, samples and runs must be positive; warmup must be nonnegative")

    args.output.mkdir(parents=True, exist_ok=True)
    runs = []
    for run in range(args.runs):
        variants = ("baseline", "candidate") if run % 2 == 0 else ("candidate", "baseline")
        for variant in variants:
            binary = getattr(args, variant).resolve()
            command = [str(binary), f"--instances={args.instances}", f"--frames={args.frames}",
                       f"--samples={args.samples}", f"--warmup={args.warmup}"]
            result = subprocess.run(command, cwd=args.runtime, text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT)
            log = args.output / f"{run}-{variant}.log"
            log.write_text(result.stdout)
            result.check_returncode()
            rows = []
            for line in result.stdout.splitlines():
                if not line.startswith("MODEL_BENCHMARK,"):
                    continue
                fields = next(csv.reader([line]))
                _, case, state, instances, sample, frames, update, submit, draw, total, draws, size, caches = fields
                rows.append({"case": case, "state": state, "instances": int(instances),
                             "sample": int(sample), "frames": int(frames), "update_us": float(update),
                             "submit_us": float(submit), "draw_us": float(draw), "cpu_frame_us": float(total),
                             "draw_calls": int(draws), "instance_bytes": int(size), "custom_caches": int(caches)})
            if not rows:
                raise RuntimeError(f"No benchmark samples in {log}")
            runs.append({"run": run, "variant": variant, "binary": str(binary), "samples": rows})
            print(f"Finished {variant} run {run + 1}/{args.runs}", flush=True)

    cases = sorted({row["case"] for run in runs for row in run["samples"]})
    summary = []
    for case in cases:
        variants = {}
        for variant in ("baseline", "candidate"):
            rows = [row for run in runs if run["variant"] == variant
                    for row in run["samples"] if row["case"] == case and row["state"] == "warm"]
            if len(rows) != args.runs * args.samples:
                raise RuntimeError(f"Missing warm samples for {variant}/{case}")
            counters = {(row["instances"], row["draw_calls"], row["instance_bytes"]) for row in rows}
            if len(counters) != 1:
                raise RuntimeError(f"Workload counters changed in {variant}/{case}: {counters}")
            variants[variant] = {field: statistics.median(row[field] for row in rows)
                                 for field in ("update_us", "submit_us", "draw_us", "cpu_frame_us", "custom_caches")}
            variants[variant]["counters"] = list(counters.pop())
            variants[variant]["run_medians_us"] = [
                statistics.median(row["cpu_frame_us"] for row in run["samples"]
                                  if row["case"] == case and row["state"] == "warm")
                for run in runs if run["variant"] == variant]
        if variants["baseline"]["counters"] != variants["candidate"]["counters"]:
            raise RuntimeError(f"Baseline/candidate workloads differ for {case}")
        before = variants["baseline"]["cpu_frame_us"]
        after = variants["candidate"]["cpu_frame_us"]
        reduction = (before - after) / before * 100 if before else None
        summary.append({"case": case, **variants, "reduction_percent": reduction})

    report = {"method": "Median of fixed-length sample means from alternating process runs; CPU timings with null graphics and active profiler counters. Object position changes, scene construction and frame cleanup are outside the timed region. Cold rows are excluded from warm comparisons. Phase times are included in cpu_frame_us.",
              "options": {key: value for key, value in vars(args).items() if isinstance(value, int)},
              "runs": runs, "summary": summary}
    (args.output / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print("case,baseline_us,candidate_us,reduction_percent,draw_calls,instance_bytes,baseline_caches,candidate_caches")
    for item in summary:
        baseline, candidate = item["baseline"], item["candidate"]
        reduction = f"{item['reduction_percent']:.1f}" if item["reduction_percent"] is not None else "n/a"
        print(f"{item['case']},{baseline['cpu_frame_us']:.3f},{candidate['cpu_frame_us']:.3f},{reduction},"
              f"{baseline['counters'][1]},{baseline['counters'][2]},"
              f"{baseline['custom_caches']:g},{candidate['custom_caches']:g}")


if __name__ == "__main__":
    main()
