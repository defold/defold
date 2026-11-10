#!/usr/bin/env python3
"""Compare integrated JNI/Bob geometry compilation with the production native core.

Both paths use the native DMMB writer. The Java path includes JNI output transfer,
model metadata and generated-input content hashing; the C++ CLI omits those costs.
"""

import argparse
from collections import Counter, defaultdict
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import statistics
import struct
import subprocess


def project_models(project):
    settings = (project / "game.project").read_text()
    bootstrap = re.search(r"^main_collection\s*=\s*(.+)$", settings, re.MULTILINE).group(1).strip()
    pending = [bootstrap.removesuffix(".collectionc") + ".collection"]
    seen = set()
    models = []
    excluded = []
    while pending:
        resource = pending.pop()
        if resource in seen:
            continue
        seen.add(resource)
        path = project / resource.lstrip("/")
        if not path.exists():
            continue
        text = path.read_text()
        if path.suffix != ".model":
            pending.extend(re.findall(r'/[^"\\\n]+\.(?:collectionproxy|collection|go|model)', text))
            continue

        mesh = re.search(r'^mesh:\s*"([^"]+)"', text, re.MULTILINE)
        if not mesh:
            continue
        source = project / mesh.group(1).lstrip("/")
        if source.suffix.lower() not in (".gltf", ".glb") or not source.exists():
            excluded.append(resource)
            continue
        selection = -1
        selected_name = re.search(r'^mesh_name:\s*"([^"]+)"', text, re.MULTILINE)
        if selected_name:
            if source.suffix == ".glb":
                data = source.read_bytes()
                length = struct.unpack_from("<I", data, 12)[0]
                document = json.loads(data[20:20 + length])
            else:
                document = json.loads(source.read_text())
            selection = next(i for i, mesh in enumerate(document["meshes"])
                             if mesh.get("name") == selected_name.group(1))
        models.append({"label": resource.lstrip("/"), "source": str(source), "selection": selection})
    return sorted(models, key=lambda model: model["label"]), excluded


def write_manifest(path, models):
    path.write_text("".join(f'{m["label"]}\t{m["source"]}\t{m["selection"]}\n' for m in models))


def read_ranges(path):
    data = path.read_bytes()
    assert data[:4] == b"DMMB", path
    version, compression, decoded, count, vertex_start, vertex_size, index_start, index_size = struct.unpack_from("<8I", data, 4)
    assert (version, compression, decoded) == (2, 0, vertex_size + index_size), path
    assert index_start == vertex_start + vertex_size and index_start + index_size == len(data), path
    assert vertex_start >= 36 + count * 28 and vertex_start % 16 == 0, path

    ranges = Counter()
    for i in range(count):
        vertex_offset, vertices, stride, index_offset, indices, element_size, mask = struct.unpack_from("<7I", data, 36 + i * 28)
        assert vertex_offset % 16 == 0 and index_offset % 4 == 0, path
        assert stride == (104 if mask & 192 == 192 else 72) and element_size in (2, 4), path
        assert vertex_offset + vertices * stride <= vertex_size, path
        assert index_offset + indices * element_size <= index_size, path
        vertex_bytes = data[vertex_start + vertex_offset:vertex_start + vertex_offset + vertices * stride]
        index_bytes = data[index_start + index_offset:index_start + index_offset + indices * element_size]
        ranges[(vertices, stride, indices, element_size, mask, hashlib.sha256(vertex_bytes + index_bytes).hexdigest())] += 1
    return ranges


def read_timings(path):
    rows = []
    for line in path.read_text().splitlines():
        fields = line.split("\t")
        if len(fields) != 7 or not fields[0].isdigit():
            continue
        sample, model = map(int, fields[:2])
        rows.append({"sample": sample, "model": model,
                     "load_ms": float(fields[2]) / 1000,
                     "prepare_ms": float(fields[3]) / 1000,
                     "pack_ms": float(fields[4]) / 1000,
                     "bytes": int(fields[5]), "ranges": int(fields[6])})
    return rows


def execute(command, path, env):
    with path.open("w") as stdout, path.with_suffix(".log").open("w") as stderr:
        subprocess.run(command, stdout=stdout, stderr=stderr, env=env, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", type=Path, required=True)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--native-library", type=Path, required=True)
    parser.add_argument("--bob-jar", type=Path, required=True)
    parser.add_argument("--java-home", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--warmups", type=int, default=3)
    parser.add_argument("--samples", type=int, default=7)
    parser.add_argument("--batches", type=int, default=3)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    models, excluded = project_models(args.project.resolve())
    if not models:
        raise RuntimeError("No reachable glTF models found")
    manifest = args.output / "models.tsv"
    write_manifest(manifest, models)
    (args.output / "manifest.json").write_text(json.dumps(models, indent=2))

    architecture = "arm64" if platform.machine().lower() in ("arm64", "aarch64") else "x86_64"
    host = {"Darwin": "macos", "Linux": "linux", "Windows": "win32"}[platform.system()]
    native_directory = args.output / "bob-root" / f"{architecture}-{host}"
    native_directory.mkdir(parents=True, exist_ok=True)
    shutil.copy2(args.native_library, native_directory / args.native_library.name)
    env = dict(os.environ, DM_BOB_ROOTFOLDER=str(args.output / "bob-root"), DM_LOG_LEVEL="ERROR")
    classes = args.output / "classes"
    classes.mkdir(exist_ok=True)
    root = Path(__file__).resolve().parents[3]
    source = root / "com.dynamo.cr/com.dynamo.cr.bob.test/src/com/dynamo/bob/pipeline/ModelBuffersBenchmark.java"
    subprocess.run([str(args.java_home / "bin/javac"), "-cp", str(args.bob_jar), "-d", str(classes), str(source)], check=True)
    commands = {
        "java": [str(args.java_home / "bin/java"), "--enable-native-access=ALL-UNNAMED", "-Xms1g", "-Xmx4g",
                 "-cp", os.pathsep.join((str(classes), str(args.bob_jar))), "com.dynamo.bob.pipeline.ModelBuffersBenchmark"],
        "native": [str(args.native)]}

    outputs = {}
    for name, command in commands.items():
        directory = args.output / f"{name}-output"
        directory.mkdir(exist_ok=True)
        execute(command + [str(manifest), "0", "1", str(directory)], args.output / f"{name}-validation.tsv", env)
        outputs[name] = directory
    range_count = 0
    for i, model in enumerate(models):
        baseline = read_ranges(outputs["java"] / f"{i}.modelbuffersc")
        candidate = read_ranges(outputs["native"] / f"{i}.modelbuffersc")
        if baseline != candidate:
            raise RuntimeError(f'GPU range mismatch: {model["label"]}')
        range_count += sum(baseline.values())
    print(f"Parity passed: {len(models)} models, {range_count} ranges", flush=True)

    results = {"java": [], "native": []}
    for batch in range(args.batches):
        order = ("java", "native") if batch % 2 == 0 else ("native", "java")
        for name in order:
            output = args.output / f"{name}-{batch}.tsv"
            execute(commands[name] + [str(manifest), str(args.warmups), str(args.samples)], output, env)
            rows = read_timings(output)
            if len(rows) != len(models) * args.samples:
                raise RuntimeError(f"Incomplete timing output: {output}")
            for row in rows:
                row["batch"] = batch
                row["total_ms"] = row["load_ms"] + row["prepare_ms"] + row["pack_ms"]
            results[name].extend(rows)
            print(f"Completed {name} batch {batch + 1}/{args.batches}", flush=True)

    summary = {"platform": platform.platform(), "models": len(models), "ranges": range_count,
               "excluded": excluded, "warmup_passes_per_process": args.warmups,
               "samples_per_process": args.samples, "batches": args.batches,
               "commands": commands, "measurements": {}}
    for name, rows in results.items():
        totals = defaultdict(lambda: defaultdict(float))
        for row in rows:
            for phase in ("load_ms", "prepare_ms", "pack_ms", "total_ms"):
                totals[(row["batch"], row["sample"])][phase] += row[phase]
        summary["measurements"][name] = {}
        for phase in ("load_ms", "prepare_ms", "pack_ms", "total_ms"):
            values = [sample[phase] for sample in totals.values()]
            quartiles = statistics.quantiles(values, n=4, method="inclusive") if len(values) > 1 else values * 3
            summary["measurements"][name][phase] = {"median": statistics.median(values), "q1": quartiles[0], "q3": quartiles[2], "samples": values}
    (args.output / "timings.json").write_text(json.dumps(results, indent=2))
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2))
    print(json.dumps(summary["measurements"], indent=2))


if __name__ == "__main__":
    main()
