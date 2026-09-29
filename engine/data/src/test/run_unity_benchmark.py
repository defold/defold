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

"""Build and run Unity DOTS standalone timing cases. Unity memory is not measured."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import plistlib
import re
import subprocess
import sys

from build_benchmark_report import read_csv, validate_unity_core, validate_unity_threaded

ROOT = Path(__file__).resolve().parents[4]
PROJECT = Path(__file__).with_name("unity")
RESULTS = ROOT / "engine/data/benchmarks"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--editor", type=Path, required=True, help="Unity.app/Contents/MacOS/Unity, version from ProjectVersion.txt")
    parser.add_argument("--build", type=Path, default=ROOT / "engine/data/build/unity")
    parser.add_argument("--output", type=Path, default=RESULTS / "unity")
    parser.add_argument("--reference", type=Path, default=RESULTS, help="Existing core/threaded benchmark results")
    parser.add_argument("--core-only", action="store_true", help="Run only the seven standalone cases; otherwise include the threaded workload")
    args = parser.parse_args()
    args.editor = args.editor.resolve()
    args.build = args.build.resolve()
    args.output = args.output.resolve()
    args.build.mkdir(parents=True, exist_ok=True)
    core = json.loads((args.reference / "core/run.json").read_text())
    threaded = None if args.core_only else json.loads((args.reference / "threaded/manifest.json").read_text())
    rows, samples = core["rows"], core["samples"]
    if rows < 1000 or rows % 1000:
        parser.error("The shared fixture requires at least 1,000 rows in multiples of 1,000")
    if threaded and threaded["rows"] != rows:
        parser.error("Standalone/threaded populations must match")
    # Keep incomplete runs away from published result files.
    run_dir = args.build / datetime.now(timezone.utc).strftime("run-%Y%m%d-%H%M%S")
    run_dir.mkdir()
    version = re.search(r"m_EditorVersion: (.+)", (PROJECT / "ProjectSettings/ProjectVersion.txt").read_text())[1]
    commands = []

    def execute(command, log, environment=None):
        command = [str(v) for v in command]
        commands.append({"command": command, "environment": environment or {}})
        env = dict(os.environ)
        # Never inherit sanitizer injection or Burst overrides into timing runs.
        for key in list(env):
            if key.startswith(("ASAN_", "TSAN_", "UBSAN_", "UNITY_BURST_")) or key == "DYLD_INSERT_LIBRARIES":
                del env[key]
        env.update(environment or {})
        with log.open("w") as stream:
            result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, env=env)
        if result.returncode:
            raise RuntimeError(f"Unity exited {result.returncode}; see {log}")

    def build():
        name = "release"
        app = args.build / (name + ".app")
        log = args.build / (name + "-build.log")
        print(f"Building Unity {name} player (IL2CPP + Burst)", flush=True)
        execute([args.editor, "-batchmode", "-nographics", "-quit", "-projectPath", PROJECT,
                 "-executeMethod", "Defold.Data.Benchmarks.BenchmarkBuild.Build", "-benchmark-build", app,
                 "-logFile", log], args.build / (name + "-editor.log"))
        if "BENCHMARK BUILD PASSED" not in log.read_text():
            raise ValueError(f"Build completion marker missing: {log}")
        info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
        binary = app / "Contents/MacOS" / info["CFBundleExecutable"]
        return app, binary

    def run(binary, validation, mode, workers=1):
        name = ("validation-" if validation else "") + mode + (f"-{workers}" if mode == "threaded" else "")
        csv_path, log = run_dir / (name + ".csv"), run_dir / (name + ".log")
        command = [binary, "-batchmode", "-nographics"]
        if validation:
            command += ["-quit", "-projectPath", PROJECT,
                        "-executeMethod", "Defold.Data.Benchmarks.BenchmarkBuild.Validate"]
        command += ["-logFile", log, "-job-worker-count", workers,
                   "-benchmark-mode", mode, "-benchmark-output", csv_path, "-benchmark-rows", rows,
                   "-benchmark-samples", 1 if validation else samples, "-benchmark-validation", str(validation).lower()]
        if mode == "threaded":
            command += ["-benchmark-frames", threaded["frames"]]
        print(f"Unity {name}: {rows:,} instances", flush=True)
        execute(command, run_dir / (name + "-stdout.log"))
        text = log.read_text()
        config = f"Burst=active; collections_safety={validation}; development={validation}"
        if "BENCHMARK PASSED" not in text or f"Benchmark Unity={version};" not in text or config not in text or re.search(r"Exception:|Assertion failed|InvalidOperationException", text):
            raise ValueError(f"Unity validation/configuration failed: {log}")
        if validation and "BENCHMARK SAFETY: burst_force=True" not in text:
            raise ValueError(f"Forced Burst safety checks missing: {log}")
        return csv_path.read_text()

    reference = read_csv((args.reference / "core/data-timing.csv").read_text())
    validate_unity_core(read_csv(run(args.editor, True, "core")), reference, rows, 1)
    if threaded:
        reference_threaded = read_csv((args.reference / "threaded/timing.csv").read_text())
        for workers in threaded["workers"]:
            validate_unity_threaded(read_csv(run(args.editor, True, "threaded", workers)), reference_threaded, threaded, [workers])

    release_app, release_binary = build()
    # Check the actual shipped executable, Burst code and IL2CPP binary. Library
    # names differ between Unity versions, so inspect all Mach-O files in the app.
    binaries = {}
    for path in sorted(release_app.rglob("*")):
        if not path.is_file() or path.is_symlink():
            continue
        kind = subprocess.check_output(["file", "-b", str(path)], text=True)
        if "Mach-O" not in kind:
            continue
        if "arm64" not in kind:
            raise ValueError(f"Non-native Unity timing binary: {path}")
        symbols = subprocess.check_output(["nm", "-u", str(path)], text=True, stderr=subprocess.DEVNULL)
        if any(token in symbols for token in ("___asan_", "___tsan_", "___ubsan_")):
            raise ValueError(f"Sanitizer in Unity timing binary: {path}")
        binaries[str(path.relative_to(release_app))] = digest(path)
    if not any("burst" in name.lower() for name in binaries):
        raise ValueError("No Burst AOT binary found in the player")
    timing = run(release_binary, False, "core")
    validate_unity_core(read_csv(timing), reference, rows, samples)
    files = {"timing.csv": timing}
    if threaded:
        all_rows = []
        for workers in threaded["workers"]:
            text = run(release_binary, False, "threaded", workers)
            all_rows += read_csv(text)
            files[f"threaded-{workers}.csv"] = text
        validate_unity_threaded(all_rows, reference_threaded, threaded, threaded["workers"])
    packages = dict(line.split("=", 1) for line in Path(str(release_app) + ".packages.txt").read_text().splitlines())
    required = json.loads((PROJECT / "Packages/manifest.json").read_text())["dependencies"]
    if any(packages.get(name) != version for name, version in required.items()):
        raise ValueError("Resolved Unity package versions differ from the pinned manifest")
    sources = [p for sub in ("Assets", "Packages", "ProjectSettings") for p in (PROJECT / sub).rglob("*") if p.is_file()]
    sources += [Path(__file__), Path(__file__).with_name("build_benchmark_report.py")]
    manifest = {
        "completed_utc": datetime.now(timezone.utc).isoformat(), "rows": rows, "samples": samples,
        "platform": platform.platform(), "unity": version, "packages": packages,
        "configuration": "macOS arm64 standalone player; IL2CPP Release; Burst AOT; strict floating point; no development/safety/sanitizer instrumentation; no memory measurements",
        "memory_measured": False, "sanitizer": "none", "safety_checks": False,
        "validation": {"core": "Unity editor with Collections/Jobs and forced Burst safety checks; per-row, count, hit and checksum validation",
                       "threaded": "unity-safety" if threaded else None,
                       "note": "Unity's prebuilt Jobs runtime is not TSAN-instrumented. Safety validation is separate from timings."},
        "threaded": {"frames": threaded["frames"], "warmup": threaded["warmup"], "workers": threaded["workers"]} if threaded else None,
        "reference": {"core": digest(args.reference / "core/run.json"),
                      "threaded": digest(args.reference / "threaded/manifest.json") if threaded else None},
        "commands": commands, "binaries": binaries,
        "sources": {str(p.relative_to(ROOT)): digest(p) for p in sources},
        "validation_files": {p.name: digest(p) for p in run_dir.glob("validation-*")},
        "files": {name: hashlib.sha256(text.encode()).hexdigest() for name, text in files.items()},
    }
    args.output.mkdir(parents=True, exist_ok=True)
    for name, text in files.items():
        (args.output / name).write_text(text)
    (args.output / "run.json").write_text(json.dumps(manifest, indent=2) + "\n")
    if args.output == RESULTS / "unity":
        subprocess.run([sys.executable, str(Path(__file__).with_name("build_benchmark_report.py"))], check=True)
    print("Unity timings validated and published. Unity memory tests were skipped.", flush=True)


if __name__ == "__main__":
    main()
