#!/usr/bin/env python3
"""Build one gamesys content lane, retaining its source tree and JVM."""

import argparse
from contextlib import contextmanager
import os
from pathlib import Path
import shutil
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[5] / "scripts/cmake"))
from bob_worker import BobWorker, dirty_jobs
from build_test_data import copy_raw_outputs, copy_prebuilt_outputs, read_build_inputs, replace_tree


@contextmanager
def timed_phase(durations, phase):
    started = time.monotonic()
    try:
        yield
    finally:
        durations[phase] = time.monotonic() - started


def build_job(job, args, root, stage, output, worker):
    folder, stamp = job["folder"], Path(job["stamp"])
    started = time.monotonic()
    durations = {}
    status = "failed"
    try:
        with timed_phase(durations, "prepare"):
            stamp.unlink(missing_ok=True)
            # Bob's project/options are fresh for every request. Also reset on-disk
            # metadata before reusing the lane's staged sources with new settings.
            for path in (stage / "build", stage / ".internal"):
                if path.exists():
                    shutil.rmtree(path)
            inputs = [stage / folder / "build.inputs", stage / "common_build.inputs"]
            command = ["--root", str(stage), "--output", "build/" + folder,
                       "--use-uncompressed-lua-source", "--debug-output-spirv", "true", "--platform", args.platform]
            for path in inputs:
                command.extend(["--build-input-file", str(path)])
            if args.max_cpu_threads:
                command.extend(["--max-cpu-threads", args.max_cpu_threads])
            if (stage / folder / "game.project").exists():
                command.extend(["--settings", str(stage / folder / "game.project")])
        with timed_phase(durations, "bob"):
            worker.invoke([*command, "build"])
        with timed_phase(durations, "publish"):
            destination = output / folder
            replace_tree(stage / "build" / folder, destination)
            copy_raw_outputs(root, destination, [entry for path in inputs for entry in read_build_inputs(path)])
            copy_prebuilt_outputs(root / folder, destination / folder)
            stamp.parent.mkdir(parents=True, exist_ok=True)
            stamp.write_text("ok\n", encoding="utf-8")
        status = "completed"
    finally:
        total = time.monotonic() - started
        phases = " ".join(f"{phase}={duration:.3f}s" for phase, duration in durations.items())
        print(f"Gamesys test content timing [{args.platform}] {folder}: {status} total={total:.3f}s {phases}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("manifest", "root", "stage", "output-root", "source-stamp", "bob-worker", "classpath", "java", "platform"):
        parser.add_argument("--" + name, required=True)
    parser.add_argument("--max-cpu-threads")
    args = parser.parse_args()
    dependencies = [__file__, Path(__file__).with_name("build_test_data.py"), args.source_stamp,
                    Path(args.bob_worker) / "BobWorker.class"]
    # The staged root is the first classpath entry; only JARs/archives are file dependencies.
    dependencies.extend(path for path in args.classpath.split(os.pathsep) if Path(path).is_file())
    jobs = list(dirty_jobs(args.manifest, dependencies))
    if not jobs:
        return
    root, stage, output = map(Path, (args.root, args.stage, args.output_root))
    with BobWorker(args.java, args.bob_worker, args.classpath, cwd=root) as worker:
        for job in jobs:
            build_job(job, args, root, stage, output, worker)


if __name__ == "__main__":
    main()
