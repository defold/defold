#!/usr/bin/env python3
"""Build both LZ4 releases and record serial, alternating before/after measurements."""

import argparse
import hashlib
import json
import random
import statistics
import subprocess
from pathlib import Path


def build(source, output):
    subprocess.run([
        "cmake", "-S", str(Path(__file__).resolve().parent), "-B", str(output), "-G", "Ninja",
        f"-DLZ4_SOURCE_DIR={source}", "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_C_FLAGS_RELEASE=-O2 -g -fno-omit-frame-pointer",
        "-DCMAKE_CXX_FLAGS_RELEASE=-O2 -g -fno-omit-frame-pointer",
    ], check=True)
    subprocess.run(["cmake", "--build", str(output)], check=True)
    return output / "lz4_benchmark"


def default_corpus(repo):
    scripts = b"\n".join(p.read_bytes() for p in sorted(
        (repo / "engine/gamesys/src/gamesys/test").rglob("*.script")))
    assets = b"".join((repo / p).read_bytes() for p in [
        "engine/modelc/src/test/assets/kay/Knight.glb",
        "editor/resources/DejaVuSansMono.ttf",
        "engine/graphics/src/test/texture_formats/testimage.rgba",
        "engine/sound/src/test/booster_on_sfx.wav",
    ])
    large_size = 64 * 1024 * 1024
    return {
        "scripts-4KiB": scripts[:4096],
        "scripts-64KiB": scripts[:65536],
        "mixed-assets": assets,
        "mixed-assets-64MiB": (assets * ((large_size + len(assets) - 1) // len(assets)))[:large_size],
        "random-4MiB": random.Random(0).randbytes(4 * 1024 * 1024),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True, help="Upstream 1.10.0 lib directory")
    parser.add_argument("--after", type=Path, required=True, help="Upstream 1.11.0 lib directory")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--input", type=Path, action="append", help="Use these files instead of the repository corpus")
    parser.add_argument("--rounds", type=int, default=5)
    parser.add_argument("--seconds", type=float, default=0.2, help="Minimum time per operation per round")
    parser.add_argument("--workers", type=int, nargs="+", default=[1, 2, 4, 8])
    parser.add_argument("--levels", type=int, nargs="+", default=[9])
    parser.add_argument("--formats", nargs="+", choices=["raw", "raw-reuse", "frame"], default=["raw", "frame"])
    args = parser.parse_args()
    if args.rounds < 1 or args.seconds <= 0 or any(w < 1 for w in args.workers):
        parser.error("rounds, seconds and worker counts must be positive")
    if any(level < 1 or level > 12 for level in args.levels):
        parser.error("levels must be 1 to 12")

    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    binaries = {
        "before": build(args.before.resolve(), output / "before"),
        "after": build(args.after.resolve(), output / "after"),
    }
    repo = Path(__file__).resolve().parents[3]
    corpus = ([(str(p), p.read_bytes()) for p in args.input] if args.input
              else list(default_corpus(repo).items()))
    manifest = []
    samples = []
    # Each invocation produces one measured round, keeping the releases interleaved.
    # Contexts are warmed inside the executable; its first compression is also recorded.
    with (output / "samples.jsonl").open("w") as samples_file:
        for index, (name, data) in enumerate(corpus):
            corpus_path = output / f"input-{index}.bin"
            corpus_path.write_bytes(data)
            manifest.append({"name": name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
            modes = [(version, mode, 0, level) for level in args.levels for mode in args.formats for version in binaries]
            if "frame" in args.formats:
                modes.extend(("after", "frame", workers, level) for level in args.levels for workers in args.workers)
            for round_index in range(args.rounds):
                # Reverse the ordering on alternate rounds to reduce order bias.
                for version, mode, workers, level in (modes if round_index % 2 == 0 else list(reversed(modes))):
                    result = subprocess.run([
                        str(binaries[version]), mode, str(corpus_path), str(workers), str(args.seconds), "1", str(level),
                    ], check=True, text=True, stdout=subprocess.PIPE)
                    sample = json.loads(result.stdout)
                    sample.update(corpus=name, round=round_index, build=version)
                    samples.append(sample)
                    samples_file.write(json.dumps(sample) + "\n")
                    samples_file.flush()
                print(f"{name}: round {round_index + 1}/{args.rounds}", flush=True)

    summary = []
    groups = sorted({(s["corpus"], s["build"], s["format"], s["workers"], s["level"]) for s in samples})
    for corpus_name, build_name, mode, workers, level in groups:
        group = [s for s in samples if (s["corpus"], s["build"], s["format"], s["workers"], s["level"])
                 == (corpus_name, build_name, mode, workers, level)]
        row = dict(group[0])
        del row["round"]
        for field in ["compress_MBps", "decompress_MBps", "first_compress_ms"]:
            row[field] = statistics.median(s[field] for s in group)
            row[field + "_min"] = min(s[field] for s in group)
            row[field + "_max"] = max(s[field] for s in group)
        summary.append(row)
    (output / "summary.json").write_text(json.dumps({
        "rounds": args.rounds, "seconds": args.seconds, "corpus": manifest, "results": summary,
    }, indent=2) + "\n")


if __name__ == "__main__":
    main()
