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

"""Record Defold/Flecs/Bevy together and verify every Bevy workload against Flecs."""

import argparse
import csv
import datetime
import hashlib
import json
import platform
import subprocess
import sys
from pathlib import Path


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_csv(path):
    with path.open() as stream:
        return list(csv.DictReader(line for line in stream if not line.startswith("#")))


def validate(cpp, rust, samples):
    reference = {(r["sample"], r["operation"], r["backend"]): r for r in read_csv(cpp)
                 if r["backend"].startswith("flecs_")}
    actual = {(r["sample"], r["operation"], r["backend"].replace("bevy_", "flecs_")): r
              for r in read_csv(rust)}
    if actual.keys() != reference.keys():
        raise ValueError(f"Workload mismatch: missing {reference.keys() - actual.keys()}, extra {actual.keys() - reference.keys()}")
    if {int(k[0]) for k in actual} != set(range(1, samples + 1)):
        raise ValueError("Missing samples")
    for key, row in actual.items():
        for field in ("rows", "operations", "hits", "checksum"):
            if row[field] != reference[key][field]:
                raise ValueError(f"{key}: different {field}: {row[field]} != {reference[key][field]}")
    return len(actual)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="New result directory; existing results are never overwritten")
    parser.add_argument("--label", required=True, help="Name of this measured comparison")
    parser.add_argument("--defold-configuration", required=True, help="Measured runtime experiment, e.g. E6")
    parser.add_argument("--rows", type=int, default=1_000_000)
    parser.add_argument("--samples", type=int, default=7)
    parser.add_argument("--packed-rows", type=int, choices=(1, 4, 16), default=16,
                        help="Rows per packed registration; one size per run (default: 16)")
    parser.add_argument("--cpp-build", type=Path, default=Path("engine/data/build/arm64-macos"))
    parser.add_argument("--bevy-build", type=Path, default=Path("engine/data/build/bevy"))
    parser.add_argument("--bevy-memory-build", type=Path, default=Path("engine/data/build/bevy-memory"))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[4]
    sources = [root / path for path in (
        "engine/data/src/data.cpp", "engine/data/src/data_io.cpp", "engine/data/src/data.h",
        "engine/data/src/dmsdk/data/data.h", "engine/data/src/test/benchmark_data_mixed.cpp",
        "engine/data/src/test/benchmark_memory.cpp", "engine/data/src/test/benchmark_memory.h",
        "engine/data/src/test/benchmark_data_blob_load.cpp", "engine/data/src/test/test_data.cpp",
        "engine/data/src/test/test_data_c.c",
        "engine/data/src/test/CMakeLists.txt", "engine/data/src/test/run_benchmark_comparison.py",
        "engine/data/src/test/summarize_benchmark.py")]
    for path in sorted(Path(__file__).parent.glob("benchmark_data_*.cpp")):
        if path not in sources:
            sources.append(path)
    sources.append(Path(__file__).with_name("benchmark_data_common.h"))
    crate = Path(__file__).with_name("bevy")
    sources += [crate / "Cargo.toml", crate / "Cargo.lock", *sorted((crate / "src").glob("*.rs"))]
    binaries = {
        "timing": {"cpp": args.cpp_build / "src/test/benchmark_data_mixed",
                   "bevy": args.bevy_build / "release/benchmark-data-bevy"},
        "memory": {"cpp": args.cpp_build / "src/test/benchmark_data_memory",
                   "bevy": args.bevy_memory_build / "release/benchmark-data-bevy"},
    }
    cache = (args.cpp_build / "CMakeCache.txt").read_text().splitlines()
    flecs_path = next(line.split("=", 1)[1] for line in cache if line.startswith("DEFOLD_DATA_FLECS_DIR:PATH="))
    manifest = {
        "id": args.output.name, "label": args.label,
        "date_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "platform": platform.platform(), "machine": platform.machine(), "cpu": "Unavailable in sandbox",
        "rows": args.rows, "samples": args.samples,
        "packed_rows_per_registration": args.packed_rows,
        "compiler_cpp": subprocess.check_output(["clang++", "--version"], text=True).splitlines()[0],
        "compiler_rust": subprocess.check_output(["rustc", "--version", "--verbose"], text=True),
        "optimization": "C++ Release -O2, no LTO; Data assertions retained, Flecs NDEBUG. Rust opt-level=2, no LTO, codegen-units=1; normal Bevy change tracking; standalone std feature, no reflection or multithreaded scheduler.",
        "measurement": "Seven samples by default after one warmup. Timing and memory are separate binaries/processes. C++ and Rust run sequentially, reversing process order between layouts; backend order rotates within each executable. Not an interleaved all-five-backend run. Requested heap only; fixtures and caller-owned IDs excluded, no RSS/allocator overhead.",
        "contracts": "Same seed, six-type mix, f32 vectors/f64 numbers, owner/component identities, operations, checksums and hit counts. Struct variants use a typed adapter for common fields; columns query components directly. Packed cases repeat prototype values; Bevy/Flecs copy mutable values and have no measured shared-resource reset/unload contract. Bevy batches count matched tables per query pass. No scheduler/rendering/serialization/disk timing.",
        "workloads": "Explosion scans health/position candidates once, subtracting 25 health (clamped at zero) within radius 50 of the origin. Shuffled position visits each instance once, reads its position and increments X by one. Both begin with reset values and include first writes; setup and per-row validation are outside measurement.",
        "defold_configuration": args.defold_configuration,
        "defold_label": args.defold_configuration,
        "inline_composition": True,
        "row_field_iterators": False,
        "structural_lock": True,
        "separate_workloads": True,
        "case_query_setup": True,
        "dense_query_count": 5,
        "named_field_selection": True,
        "field_pointer_access": True,
        "query_field_handles": True,
        "mutable_instance_rows": True,
        "shared_resource_tables": True,
        "pooled_registration_slots": True,
        "native_c_layout": True,
        "file_version": 1,
        "traversal": "Defold holds public DataStoreLock/DataStoreUnlock across traversal (included in timing). Each workload has separate Defold/Flecs functions, with query construction in its case source file. Explosion owns a query separate from read-only health/position scans: Flecs declares health read/write and position read-only for Explosion, both read-only for the scan. Dense setup creates five live logical queries on all backends; packed setup creates two. Defold resolves full field paths and kinds once during query creation using DataQueryFindField; the query-local handles are reused across all batches and traversals through public typed DataFieldGet pointers with DataRowIterator. Lookup cost is included in create_queries. Instance creation copies fixed-size rows into shared dense tables per loaded resource/table. Registration handles and row membership indices use reusable pooled slots; identical source tables share query bindings across registrations. Different resource handles remain separate. The million-row packed fixture uses six runtime tables and 62,500 logical registrations. Explosion writes existing health values only for hit rows; scalar/math writes allocate nothing. Decoded tables retain added defaults; packed instances share blob defaults and payloads. Reset copies defaults into existing rows. Field order is unspecified; the hot row loop performs no name, kind or lifetime checks. Fixed rows use C offsetof/sizeof layouts; blob version 1 enforces member, struct and row-stride alignment. Flecs ecs_query_iter/ecs_query_next/ecs_field_w_size followed by C member access. Bevy QueryState::iter/iter_mut followed by Rust member access. No private ECS storage traversal.",
        "flecs_describe": subprocess.check_output(["git", "-C", flecs_path, "describe", "--tags", "--always"], text=True).strip(),
        "flecs_revision": subprocess.check_output(["git", "-C", flecs_path, "rev-parse", "HEAD"], text=True).strip(),
        "bevy_version": "0.19.1 (exact crate version and transitive dependencies in Cargo.lock)",
        "source_sha256": {p.resolve().relative_to(root).as_posix(): digest(p) for p in sources},
        "binary_sha256": {f"{mode}/{backend}": digest(path) for mode, paths in binaries.items() for backend, path in paths.items()},
        "commands": [], "validation": {},
    }
    args.output.mkdir(parents=True, exist_ok=False)
    checks = 0
    for mode, paths in binaries.items():
        folder = args.output / mode
        folder.mkdir()
        merged = []
        for index, group in enumerate((0, args.packed_rows)):
            name = "dense" if group == 0 else f"packed-rows{group}"
            outputs = {backend: folder / f"{name}-{backend}.csv" for backend in paths}
            for backend in (("cpp", "bevy") if index % 2 == 0 else ("bevy", "cpp")):
                command = [str(paths[backend].resolve()), str(args.rows), str(args.samples), "all", str(group)]
                manifest["commands"].append(command)
                print(f"{mode}: {name}, {backend}", flush=True)
                with outputs[backend].open("wb") as stream, outputs[backend].with_suffix(".log").open("wb") as log:
                    subprocess.run(command, stdout=stream, stderr=log, check=True)
            checks += validate(outputs["cpp"], outputs["bevy"], args.samples)
            rows = read_csv(outputs["cpp"]) + read_csv(outputs["bevy"])
            combined = folder / f"{name}.csv"
            with combined.open("w") as stream:
                stream.write(f"# packed_rows={group}; merged unchanged samples from separate C++ and Rust processes\n")
                writer = csv.DictWriter(stream, fieldnames=rows[0].keys())
                writer.writeheader()
                writer.writerows(rows)
            merged.append(str(combined))
        with (folder / "summary.csv").open("wb") as stream:
            subprocess.run([sys.executable, str(Path(__file__).with_name("summarize_benchmark.py")), *merged], stdout=stream, check=True)
    manifest["validation"] = {
        "cross_backend_samples": checks,
        "checks": "Every Bevy operation/sample matches Flecs operation count, hit count and exact checksum. Per-backend checks include query counts, per-row damage, identity, stale IDs after removal, resets between workloads, and zero tracked blocks after world/query teardown.",
    }
    manifest["completed_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    manifest["results_sha256"] = {p.relative_to(args.output).as_posix(): digest(p) for p in args.output.rglob("*.csv")}
    (args.output / "run.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Recorded {args.output}; {checks} matching Bevy/Flecs samples", flush=True)


if __name__ == "__main__":
    main()
