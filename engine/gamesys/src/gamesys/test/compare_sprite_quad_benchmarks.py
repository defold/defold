#!/usr/bin/env python3
"""Run the opt-in null-backend quad benchmark in rotating mode order.

From a Defold build shell:
  cmake -S . -B engine/build/x86_64-win32 -DDEFOLD_ENABLE_FEATURES=performance_tests
  cmake --build engine/build/x86_64-win32 --target benchmark_sprite_quads
  python engine/gamesys/src/gamesys/test/compare_sprite_quad_benchmarks.py \
    --binary engine/gamesys/build/x86_64-win32/src/gamesys/test/benchmark_sprite_quads.exe \
    --runtime engine/gamesys/build/x86_64-win32/gamesys-test-runtime \
    --output tmp/sprite-quad-benchmark

Measures real sprite update/sort/batch/packing plus null-backend buffer copies.
Disables the null backend's artificial indexed-vertex capture in every mode.
cpu_prepare_us excludes timed null uploads (allocation/copy, not driver work).
There is no GPU execution, driver upload, fragment work, or presentation. Input
mutation, fixture construction and correctness validation are outside timing.
The cold row is the first rendered frame after one setup update; summaries use
warm samples only. These are sprite-pipeline CPU times, not whole-game frames.
The matrix/compact modes are benchmark-only, single-texture alpha quad writers,
not production material support. The unpaged baseline omits a page attribute;
both instance layouts include its four bytes. Shared quad VB/IB use 44 bytes.
Fragmentation uses synthetic compatibility keys at distinct depth groups.
"""

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import time


SCENARIOS = {
    "static": (0, 0, 1, 1),
    "moving": (1, 0, 1, 1),
    "animated": (1, 1, 1, 1),
    "fragmented": (1, 1, 64, 1),
    "multipass": (1, 1, 1, 3),
}
MODES = ("expanded", "matrix", "compact")
FIELDS = ("update_us", "submit_us", "draw_us", "cpu_frame_us", "pack_us", "null_upload_us")
COUNTERS = ("instances", "moving", "animated", "batches", "passes", "draw_calls", "vertex_bytes", "index_bytes")
HEADER = "marker,mode,state,instances,moving,animated,batches,passes,sample,frames,warmup,update_us,submit_us,draw_us,cpu_frame_us,pack_us,null_upload_us,draw_calls,vertex_bytes,index_bytes".split(",")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--runtime", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--counts", nargs="+", type=int, default=[1000, 10000, 50000])
    parser.add_argument("--scenarios", nargs="+", choices=SCENARIOS, default=list(SCENARIOS))
    parser.add_argument("--runs", type=int, default=3)
    parser.add_argument("--frames", type=int, default=60)
    parser.add_argument("--samples", type=int, default=7)
    parser.add_argument("--warmup", type=int, default=30)
    args = parser.parse_args()
    if min(*args.counts, args.runs, args.frames, args.samples) < 1 or args.warmup < 0:
        parser.error("counts, runs, frames and samples must be positive; warmup must be nonnegative")
    if "fragmented" in args.scenarios and min(args.counts) < 64:
        parser.error("fragmented requires at least 64 instances")
    args.binary = args.binary.resolve()
    args.runtime = args.runtime.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    metadata = {
        "time_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "platform": platform.platform(), "processor": platform.processor(),
        "processor_identifier": os.environ.get("PROCESSOR_IDENTIFIER"),
        "cpu_count": os.cpu_count(), "binary": str(args.binary),
        "binary_sha256": hashlib.sha256(args.binary.read_bytes()).hexdigest(),
        "runtime": str(args.runtime), "scope": "CPU, null backend; no GPU measurements",
        "null_index_capture": False,
        "cpu_prepare_definition": "cpu_frame_us minus null_upload_us; includes update, submission, sorting, packing and count-only drawing",
        "options": {k: str(v) if isinstance(v, Path) else v for k, v in vars(args).items()},
    }
    if os.name == "nt":
        import winreg
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"HARDWARE\DESCRIPTION\System\CentralProcessor\0") as key:
            metadata["cpu_name"] = winreg.QueryValueEx(key, "ProcessorNameString")[0].strip()
    revision = subprocess.run(["git", "rev-parse", "HEAD"], cwd=args.runtime, text=True, capture_output=True, check=True)
    metadata["base_revision"] = revision.stdout.strip()
    runs = []
    for count in args.counts:
        for scenario in args.scenarios:
            moving, animated, batches, passes = SCENARIOS[scenario]
            for run in range(args.runs):
                for mode in [(run + offset) % 3 for offset in range(3)]:
                    command = [str(args.binary), f"--instances={count}", f"--mode={mode}",
                               f"--moving={moving}", f"--animated={animated}", f"--batches={batches}",
                               f"--passes={passes}", f"--frames={args.frames}", f"--samples={args.samples}",
                               f"--warmup={args.warmup}"]
                    result = subprocess.run(command, cwd=args.runtime, text=True,
                                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    log = args.output / f"{count}-{scenario}-{run}-{MODES[mode]}.log"
                    log.write_text(result.stdout, encoding="utf-8")
                    if result.returncode:
                        raise RuntimeError(f"Benchmark failed ({result.returncode}); see {log}")
                    rows = []
                    for fields in csv.reader(line for line in result.stdout.splitlines() if line.startswith("SPRITE_QUAD,")):
                        if len(fields) != len(HEADER):
                            raise RuntimeError(f"Malformed result in {log}: {fields}")
                        row = dict(zip(HEADER, fields))
                        for field in FIELDS:
                            row[field] = float(row[field])
                        row["cpu_prepare_us"] = row["cpu_frame_us"] - row["null_upload_us"]
                        for field in COUNTERS + ("sample", "frames", "warmup"):
                            row[field] = int(row[field])
                        rows.append(row)
                    warm = [row for row in rows if row["state"] == "warm"]
                    if len(warm) != args.samples or len(rows) != args.samples + 1:
                        raise RuntimeError(f"Missing samples in {log}")
                    for row in rows:
                        if row["mode"] != MODES[mode] or row["instances"] != count or row["draw_calls"] != batches * passes:
                            raise RuntimeError(f"Workload mismatch in {log}: {row}")
                    runs.append({"count": count, "scenario": scenario, "run": run, "mode": MODES[mode],
                                 "command": command, "samples": rows})
                    (args.output / "raw.json").write_text(json.dumps({"metadata": metadata, "runs": runs}, indent=2), encoding="utf-8")
                    print(f"{count:6} {scenario:10} run {run + 1}/{args.runs} {MODES[mode]}", flush=True)

    summary = []
    lines = ["Sprite quad CPU benchmark (null graphics backend)", "",
             "Times are medians of per-process medians of warm sample means, microseconds.",
             "Each mode has the same sorted sprites and draw count. No GPU conclusions.", "",
             "count scenario mode prepare_us frame_us pack_us upload_us prepare_change_pct dynamic_bytes"]
    for count in args.counts:
        for scenario in args.scenarios:
            modes = {}
            for mode in MODES:
                selected = [run for run in runs if (run["count"], run["scenario"], run["mode"]) == (count, scenario, mode)]
                metrics = FIELDS + ("cpu_prepare_us",)
                medians = [{field: statistics.median(row[field] for row in run["samples"] if row["state"] == "warm") for field in metrics} for run in selected]
                counters = {tuple(row[field] for field in COUNTERS) for run in selected for row in run["samples"]}
                if len(counters) != 1:
                    raise RuntimeError(f"Counters vary: {count}/{scenario}/{mode}")
                modes[mode] = {field: statistics.median(row[field] for row in medians) for field in metrics}
                modes[mode]["run_medians"] = medians
                modes[mode]["counters"] = dict(zip(COUNTERS, counters.pop()))
                modes[mode]["cpu_frame_run_min_us"] = min(row["cpu_frame_us"] for row in medians)
                modes[mode]["cpu_frame_run_max_us"] = max(row["cpu_frame_us"] for row in medians)
                modes[mode]["cpu_prepare_run_min_us"] = min(row["cpu_prepare_us"] for row in medians)
                modes[mode]["cpu_prepare_run_max_us"] = max(row["cpu_prepare_us"] for row in medians)
            for mode, data in modes.items():
                data["cpu_frame_change_pct"] = 100 * (data["cpu_frame_us"] / modes["expanded"]["cpu_frame_us"] - 1)
                data["cpu_prepare_change_pct"] = 100 * (data["cpu_prepare_us"] / modes["expanded"]["cpu_prepare_us"] - 1)
                byte_count = data["counters"]["vertex_bytes"] + data["counters"]["index_bytes"]
                lines.append(f"{count} {scenario} {mode} {data['cpu_prepare_us']:.3f} {data['cpu_frame_us']:.3f} {data['pack_us']:.3f} {data['null_upload_us']:.3f} {data['cpu_prepare_change_pct']:+.1f} {byte_count}")
            summary.append({"count": count, "scenario": scenario, "modes": modes})
    (args.output / "summary.json").write_text(json.dumps({"metadata": metadata, "results": summary}, indent=2), encoding="utf-8")
    (args.output / "summary.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines), flush=True)


if __name__ == "__main__":
    main()
