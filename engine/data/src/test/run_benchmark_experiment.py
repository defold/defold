# Copyright 2026 The Defold Foundation
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

"""Record one explicitly named allocation experiment; build and test its sources first."""

import argparse
import datetime
import hashlib
import json
import platform
import subprocess
import sys
from pathlib import Path


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("name")
    parser.add_argument("--label", required=True)
    parser.add_argument("--hypothesis", required=True)
    parser.add_argument("--parent")
    parser.add_argument("--backend", choices=("all", "data"), default="data")
    parser.add_argument("--build", type=Path, default=Path("engine/data/build/arm64-macos"))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[4]
    output = root / "engine/data/benchmarks/experiments" / args.name
    output.mkdir(parents=True, exist_ok=False)
    build = args.build.resolve()
    paths = ["engine/data/src/data.cpp", "engine/data/src/data_io.cpp", "engine/data/src/data.h",
             "engine/data/src/dmsdk/data/data.h", "engine/data/src/test/benchmark_data_mixed.cpp",
             "engine/data/src/test/benchmark_memory.cpp", "engine/data/src/test/benchmark_memory.h",
             "engine/data/src/test/CMakeLists.txt", "engine/data/src/test/test_data.cpp",
             "engine/data/src/test/run_benchmark_experiment.py"]
    binaries = {mode: build / "src/test" / executable for mode, executable in
                (("timing", "benchmark_data_mixed"), ("memory", "benchmark_data_memory"))}
    manifest = {
        "id": args.name, "label": args.label, "hypothesis": args.hypothesis, "parent": args.parent,
        "date_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "platform": platform.platform(), "machine": platform.machine(),
        "cpu": "Unavailable in sandbox",
        "compiler": subprocess.check_output(["clang++", "--version"], text=True).splitlines()[0],
        "optimization": "Release -O2, no LTO; Data assertions retained; Flecs FLECS_NO_CPP/FLECS_NDEBUG",
        "source_sha256": {path: digest(root / path) for path in paths},
        "binary_sha256": {mode: digest(path) for mode, path in binaries.items()},
        "commands": [], "rows": 1000000, "samples": 7, "backend": args.backend,
        "measurement": "Timing and requested-heap accounting are separate processes. Seven samples after one warmup; backend order rotates for all-backend runs. Variants run sequentially, not randomized. No RSS or allocator overhead.",
        "validation": "Release runtime suite passed before recording. Each case validates values/counts/reset; memory runs self-test accounting and require zero tracked allocations at teardown.",
    }
    for mode, binary in binaries.items():
        folder = output / mode
        folder.mkdir()
        csvs = []
        for group in (0, 1, 4, 16):
            path = folder / ("dense.csv" if not group else f"packed-rows{group}.csv")
            command = [str(binary), "1000000", "7", args.backend, str(group)]
            manifest["commands"].append(command)
            print(f"{args.name}: {mode}, rows/table={group}", flush=True)
            with path.open("wb") as stream, path.with_suffix(".log").open("wb") as log:
                subprocess.run(command, stdout=stream, stderr=log, check=True)
            csvs.append(str(path))
        with (folder / "summary.csv").open("wb") as stream:
            subprocess.run([sys.executable, str(Path(__file__).with_name("summarize_benchmark.py")), *csvs], stdout=stream, check=True)
    manifest["completed_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    manifest["results_sha256"] = {path.relative_to(output).as_posix(): digest(path) for path in output.rglob("*.csv")}
    (output / "run.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Recorded {output}", flush=True)


if __name__ == "__main__":
    main()
