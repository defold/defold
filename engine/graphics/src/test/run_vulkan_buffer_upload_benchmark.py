"""Run interleaved upload/backend trials and retain raw results and process logs."""

import argparse
import csv
import hashlib
import itertools
import json
import os
from pathlib import Path
import random
import shutil
import statistics
import subprocess


def gpu_snapshot(nvidia_smi):
    """Capture device-wide state outside the timed benchmark process."""
    if not nvidia_smi:
        return None
    fields = ("timestamp", "memory.used", "memory.free", "utilization.gpu", "temperature.gpu",
              "pstate", "clocks.current.graphics", "clocks.current.memory")
    process = subprocess.run(
        [nvidia_smi, "--query-gpu=" + ",".join(fields), "--format=csv,noheader,nounits"],
        text=True, capture_output=True, timeout=15)
    if process.returncode:
        return {"error": process.stderr.strip()}
    return [dict(zip(fields, (value.strip() for value in row)))
            for row in csv.reader(process.stdout.splitlines())]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--reference-executable", type=Path, help="Interleave a saved before executable with the current build")
    parser.add_argument("--trials", type=int, default=5)
    parser.add_argument("--warmup", type=int, default=120)
    parser.add_argument("--frames", type=int, default=480)
    parser.add_argument("--sizes", type=int, nargs="+", default=[64, 1024, 16384], help="Vertex sizes in KiB")
    parser.add_argument("--workloads", nargs="+", choices=("resident", "stream", "stream4", "occasional"),
                        default=["resident", "stream", "stream4", "occasional"])
    parser.add_argument("--verify", action="store_true")
    parser.add_argument("--modes", nargs="+", choices=("host", "bar", "staged", "vulkan", "dx12", "opengl"),
                        default=["host", "bar", "staged"])
    parser.add_argument("--telemetry", action="store_true", help="Record NVIDIA state before and after each process")
    args = parser.parse_args()
    executable = args.executable.resolve()
    variants = [("", executable)]
    if args.reference_executable:
        variants = [("before", args.reference_executable.resolve()), ("after", executable)]
    nvidia_smi = shutil.which("nvidia-smi") if args.telemetry else None
    args.output.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    if args.verify:
        env["VK_LAYER_ENABLES"] = (
            "VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,"
            "VALIDATION_CHECK_ENABLE_SYNCHRONIZATION_VALIDATION_QUEUE_SUBMIT"
        )
    else:
        for key in ("VK_LAYER_ENABLES", "VK_INSTANCE_LAYERS", "VK_VALIDATION_VALIDATE_SYNC"):
            env.pop(key, None)
    metadata = {
        "executable": str(executable),
        "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
        "seed": 13751,
        "trials": args.trials,
        "warmup": args.warmup,
        "frames": args.frames,
        "verify": args.verify,
        "sizes": args.sizes,
        "workloads": args.workloads,
        "modes": args.modes,
        "telemetry_executable": nvidia_smi,
        "serial_environment": env.get("DEFOLD_BUFFER_BENCHMARK_SERIAL"),
    }
    if args.reference_executable:
        reference = args.reference_executable.resolve()
        metadata["reference_executable"] = str(reference)
        metadata["reference_executable_sha256"] = hashlib.sha256(reference.read_bytes()).hexdigest()
    (args.output / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    patterns = {"resident": (0, 1), "stream": (1, 1), "stream4": (1, 4), "occasional": (60, 1)}
    workloads = list(itertools.product(args.sizes, (patterns[name] for name in args.workloads)))
    rng = random.Random(13751)
    results = []
    total = args.trials * len(workloads) * len(args.modes) * len(variants)
    for trial in range(args.trials):
        rng.shuffle(workloads)
        for kib, (interval, draws) in workloads:
            modes = args.modes.copy()
            rng.shuffle(modes)
            cases = [(mode, variant, path) for mode in modes for variant, path in variants]
            if args.reference_executable:
                rng.shuffle(cases)
            for mode, variant, path in cases:
                command = [str(path), mode, str(kib), str(interval), str(draws),
                           str(args.warmup), str(args.frames), str(int(args.verify))]
                before = gpu_snapshot(nvidia_smi)
                process = subprocess.run(command, env=env, text=True, capture_output=True, timeout=120)
                after = gpu_snapshot(nvidia_smi)
                log = process.stdout + process.stderr
                name = f"trial-{trial}-{kib}k-interval-{interval}-draws-{draws}-{mode}"
                if variant:
                    name += f"-{variant}"
                (args.output / f"{name}.log").write_text(log, encoding="utf-8")
                if process.returncode == 77:
                    print(f"SKIP {name}: memory mode unavailable", flush=True)
                    continue
                if process.returncode or any(marker in log for marker in ("Validation Error", "SYNC-HAZARD", "VUID-")):
                    raise RuntimeError(f"Benchmark failed: {name}\n{log}")
                entries = [json.loads(line[7:]) for line in process.stdout.splitlines() if line.startswith("RESULT ")]
                if len(entries) != 1:
                    raise RuntimeError(f"Missing benchmark result: {name}\n{log}")
                result = entries[0]
                result["trial"] = trial
                result["vertex_kib"] = kib
                if variant:
                    result["variant"] = variant
                if args.telemetry:
                    result["telemetry_before"] = before
                    result["telemetry_after"] = after
                results.append(result)
                (args.output / "samples.json").write_text(json.dumps(results, indent=2) + "\n")
                print(f"{len(results)}/{total} {name}: GPU {result['gpu_mean_us']:.1f} us; "
                      f"upload CPU {result['upload_cpu_us']:.1f} us", flush=True)
    grouped = {}
    group_fields = ("vertex_kib", "interval", "draws", "mode")
    if args.reference_executable:
        group_fields += ("variant",)
    for result in results:
        key = tuple(result[field] for field in group_fields)
        grouped.setdefault(key, []).append(result)
    with (args.output / "summary.csv").open("w", newline="") as output:
        writer = csv.writer(output)
        writer.writerow((*group_fields, "trials",
                         "gpu_us", "gpu_min_us", "gpu_max_us", "upload_cpu_us", "wall_us", "draw_gpu_us"))
        for key, samples in sorted(grouped.items()):
            gpu = [sample["gpu_mean_us"] for sample in samples]
            writer.writerow((*key, len(samples), statistics.median(gpu), min(gpu), max(gpu),
                             statistics.median(sample["upload_cpu_us"] for sample in samples),
                             statistics.median(sample["wall_us"] for sample in samples),
                             statistics.median(sample["draw_gpu_mean_us"] for sample in samples)
                             if "draw_gpu_mean_us" in samples[0] else ""))


if __name__ == "__main__":
    main()
