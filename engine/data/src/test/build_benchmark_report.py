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

"""Build the current offline comparison from the latest completed benchmark run."""

import argparse
import csv
import hashlib
import json
import statistics
from decimal import Decimal
from pathlib import Path


CASE_DESCRIPTIONS = {
    "all_light_color": {
        "title": "Sum all light colors",
        "description": "Find SpotLight and PointLight instances tagged as lights and sum all three components of light.color. Five read passes per sample.",
    },
    "explosion_r50_first": {
        "title": "Explosion",
        "description": "Scan health and position; subtract 25 health, clamped at zero, from instances within radius 50 of the origin. One pass from reset defaults.",
    },
    "shuffled_position": {
        "title": "Shuffled position",
        "description": "Visit each instance once in shuffled order, read its position, increase X by 1, and write it back. Starts with reset defaults.",
    },
}


def duration(nanoseconds):
    rounded = float(f"{nanoseconds:.3g}")
    scale, unit = next((scale, unit) for scale, unit in
                       ((1e9, "s"), (1e6, "ms"), (1e3, "µs"), (1, "ns"))
                       if abs(rounded) >= scale or scale == 1)
    value = format(Decimal(f"{nanoseconds / scale:.3g}"), "f")
    if "." in value:
        value = value.rstrip("0").rstrip(".")
    return f"{value} {unit}"


def heap_change(value):
    scale, unit = next((scale, unit) for scale, unit in
                       ((1073741824, "GiB"), (1048576, "MiB"), (1024, "KiB"), (1, "B"))
                       if abs(value) >= scale or scale == 1)
    amount = format(Decimal(f"{value / scale:.3g}"), "f")
    if "." in amount:
        amount = amount.rstrip("0").rstrip(".")
    return f"{'+' if value > 0 else ''}{amount} {unit}"


def read_csv(text):
    return list(csv.DictReader(line for line in text.splitlines() if not line.startswith("#")))


def numeric_rows(text):
    rows = read_csv(text)
    for row in rows:
        for key, value in row.items():
            if key not in ("backend", "operation"):
                row[key] = float(value) if "." in value else int(value)
    return rows


def add_memory_metrics(rows, files, prefix, dense_name, packed_name):
    # Derive each sample's requests and net change before calculating statistics.
    raw = {}
    for row in rows:
        name = packed_name.format(row["packed_rows"]) if row["packed_rows"] else dense_name
        if name not in raw:
            raw[name] = read_csv(files[prefix + name]["text"])
        samples = [r for r in raw[name]
                   if r["backend"] == row["backend"] and r["operation"] == row["operation"]]
        assert len(samples) == row["samples"]
        metrics = {
            "requests": [int(r["allocations"]) + int(r["reallocations"]) for r in samples],
            "change_bytes": [int(r["live_bytes"]) - int(r["before_bytes"]) for r in samples],
        }
        for metric, values in metrics.items():
            row["median_" + metric] = statistics.median(values)
            row["min_" + metric] = min(values)
            row["max_" + metric] = max(values)


def write_comparison_markdown(comparison, folder):
    backends = ("data", "flecs_rows", "flecs_columns", "bevy_rows", "bevy_columns")
    labels = ("Defold", "Flecs structs", "Flecs columns", "Bevy structs", "Bevy columns")
    run = comparison["run"]
    composition = ("Light is inline in all backends. Defold binds light.color once per table and reads it with the public Vector3 field getter."
                   if run.get("inline_composition") else
                   "Light is dynamic nested data in Defold and inline in Flecs/Bevy.")
    if run.get("row_field_iterators"):
        composition += " Defold traverses public batch, row and field cursors; getters take the current field without row/field indices."
    if run.get("structural_lock"):
        composition += " Defold holds a structural lock during traversal (included in timing); cursor lifetimes are caller obligations, with no revision or parent-step checks."
        if not run.get("named_field_selection"):
            composition += " Position precedes health so writes use the current field without copying an iterator."
    if run.get("inline_index_cursors"):
        composition += " Row/field factories and Next are inline index operations. Typed getters resolve shared metadata and byte addresses; metadata inspection uses explicit type/name accessors."
    if run.get("direct_field_cursors"):
        composition += " Field cursors reference the batch and selected row index directly; parent lifetime rules are unchanged."
    if run.get("inline_getter_wrappers"):
        composition += " Public typed getter wrappers inline and forward batch/row/field arguments to the library; the unchanged benchmark still calls only the public iterator API."
    if run.get("separate_workloads"):
        composition += " Each workload has its own Defold/Flecs function and source file; backend/workload selection happens outside row loops."
    if run.get("case_query_setup"):
        composition += " Query construction lives in each C++ case file. Explosion owns a separate query with writable health and read-only position; Flecs health/position scans declare both fields read-only. All backends create five dense queries and two resource-instance queries. Creation and field binding remain outside timed traversal and are measured separately."
    if run.get("shared_resource_tables"):
        composition += " Defold appends mutable rows to shared tables per loaded resource, with pooled registration records and 32-bit row membership indices. The million-instance packed fixture has six physical tables and 62,500 logical registrations. Each registration still resets/unloads independently; different resource handles remain separate. Query-local field handles select cached offsets, and typed pointers access mutable bytes directly. Numeric writes allocate nothing. Packed defaults stay shared; replacement payloads belong to each registration. Decoded tables retain owned reset defaults."
    elif run.get("mutable_instance_rows"):
        composition += " Defold copies fixed rows into mutable instance storage during creation/registration, sharing packed metadata and default payloads. Query-local handles select cached offsets; typed pointers access mutable bytes directly. Scalar/math writes allocate nothing. Reset copies defaults back; string/container replacements use table-owned payload blocks. Creation includes mutable row storage, and decoded tables also retain owned reset defaults."
    elif run.get("query_field_handles"):
        composition += " Defold resolves full property paths and kinds with DataQueryFindField during query creation, reusing query-local handles across all batches and traversals. Lookup cost is included in create_queries; table-specific offsets remain cached internally. Public typed pointer getters and first-write override semantics are unchanged."
    elif run.get("field_pointer_access"):
        composition += " Defold resolves full property paths and kinds with DataIterFindField once per batch, then uses public typed read-only/writable field pointers and a row iterator. Explosion prepares an override only for hit rows. No name, kind or lifetime checks occur inside pointer getters; field order is unspecified. Fixed rows use C offsetof/sizeof layouts and version 8 alignment."
    elif run.get("single_field_light_query"):
        composition += " Light queries request only light.color and advance to that sole field without a name lookup. Multi-property queries select fields by name using public iteration. No descriptor-order guarantee is used."
    elif run.get("named_field_selection"):
        composition += " Defold finds fields by name using the public field iterator. No descriptor-order guarantee is used; this lookup work is included in timings."
    lines = ["# Defold, Flecs and Bevy benchmarks", "",
             f"{run['rows']:,} instances; {run['samples']} measured samples after one warmup. "
             "All five backends were measured in this run.", "",
             "Bevy uses the standalone ECS crate with default table storage and normal change tracking, "
             "without a renderer or scheduler. Struct variants use a fixed typed adapter for shared fields; "
             "columns query components directly. " + composition, "",
             "Timings are **median time per operation**, with ns/µs/ms/s chosen to fit each value: queries use visited rows, explosions use "
             "candidate rows, insertion/removal use affected instances, and random access uses accesses. "
             "Lower is better. Read queries use five passes. C++ and Rust ran in separate processes; "
             "their order alternated between layouts. Different compilers and API contracts are included in the result.", ""]

    def table(rows):
        lines.extend(["| Measurement | " + " | ".join(labels) + " |",
                      "| --- | " + " | ".join(["---:"] * len(labels)) + " |"])
        lines.extend("| " + " | ".join(row) + " |" for row in rows)
        lines.append("")

    dense_cases = (
        ("create_bulk", "Bulk creation"), ("create_individual", "Individual creation"),
        ("add_10pct_live_queries_batch100", "Add 10%, batches of 100, live queries"),
        ("spot_color", "SpotLight color query"), ("all_light_color", "Sum all light colors"),
        ("health_position", "Health + position query"), ("enemy_health_position", "Enemy health + position query"),
        ("explosion_r50_first", "Explosion"), ("shuffled_position", "Shuffled position"),
        ("explosion_r17_filter", "Explosion filter, ~1% hits"),
        ("explosion_r36_filter", "Explosion filter, ~10% hits"),
        ("explosion_r63_filter", "Explosion filter, ~50% hits"),
        ("explosion_r36_first", "Explosion damage, ~10% hits, first writes"),
        ("explosion_r36_repeat", "Explosion damage, ~10% hits, repeat writes"),
        ("random_get_position", "Random position get"), ("random_update_first", "Random position update, first"),
        ("random_update_repeat", "Random position update, repeat"), ("remove_1pct", "Remove 1%"),
        ("replace_1pct", "Replace 1%"), ("health_after_churn", "Health query after churn"))
    packed_cases = (("packed_populate_and_ids", "Population + ID collection"),
                    ("packed_light_color", "Light color query"),
                    ("packed_10pct_health_query", "Health query after 10% writes"),
                    ("packed_10pct_update_first", "First scalar writes to 10%"),
                    ("packed_10pct_update_repeat", "Repeat scalar writes to 10%"),
                    ("packed_10pct_random_get", "Random scalar get after writes"))
    timing = {(r["packed_rows"], r["operation"], r["backend"]): r for r in comparison["timing"]}
    packed_rows = comparison["run"].get("packed_rows_per_registration", 16)
    for group in (0, packed_rows):
        lines.extend(["## " + ("Runtime-created instances" if not group else "Instances from resource blobs"), ""])
        if group:
            if run.get("shared_resource_tables"):
                lines.extend([f"Defold stores {run['rows']:,} mutable rows in six shared runtime tables, "
                              f"across {run['rows'] // group:,} registrations of {group} rows. "
                              "Registration size determines the reset/unload group, not physical table capacity. "
                              "Only this registration size is measured in the current run.", ""])
            else:
                lines.extend([f"Historical fixture: {group} rows per Defold registration.", ""])
            lines.extend(["Flecs and Bevy instantiate mutable copies of the same prototype values. "
                          "These cases do not measure file I/O or native borrowed-blob reset/unload.", ""])
        else:
            for op, info in CASE_DESCRIPTIONS.items():
                if (group, op, "data") in timing:
                    lines.extend([f"**{info['title']}**", "", info["description"], ""])
        table([[label] + [duration(timing[group, op, b]['median_ns_per_operation']) for b in backends]
               for op, label in (dense_cases if not group else packed_cases)
               if (group, op, "data") in timing])
    lines.extend(["## Runtime memory", "",
                  "Requested heap; fixture inputs, caller ID arrays, shared source assets, allocator overhead and RSS "
                  "are excluded. Heap change is after minus before, calculated per sample before taking the median. "
                  "Allocation requests include resizes; retained totals and live blocks include earlier phases.", ""])
    memory = {(r["packed_rows"], r["operation"], r["backend"]): r for r in comparison["memory"]}
    cases = ((0, "create_bulk", "live_bytes", "Bulk creation, retained MiB"),
             (0, "create_bulk", "requests", "Bulk creation, allocation requests"),
             (0, "create_queries", "change_bytes", f"Create {run.get('dense_query_count', 4)} live queries, heap change"),
             (packed_rows, "packed_populate_and_ids", "live_bytes", "Packed population, retained MiB"),
             (packed_rows, "packed_populate_and_ids", "requests", "Packed population, allocation requests"),
             (packed_rows, "packed_create_queries", "requests", "Create two queries, allocation requests"),
             (packed_rows, "packed_10pct_update_first", "live_bytes", "Packed tables + 10% writes, retained MiB"),
             (packed_rows, "packed_10pct_update_first", "live_blocks", "Packed tables + 10% writes, live blocks"),
             (packed_rows, "packed_10pct_update_first", "requests", "First writes to 10%, allocation requests"))
    if run.get("mutable_instance_rows"):
        cases += ((0, "explosion_r50_first", "change_bytes", "Explosion, heap change"),
                  (0, "explosion_r50_first", "requests", "Explosion, allocation requests"),
                  (0, "shuffled_position", "change_bytes", "Shuffled position, heap change"),
                  (0, "shuffled_position", "requests", "Shuffled position, allocation requests"))
    rows = []
    for group, op, metric, label in cases:
        values = [memory[group, op, b]["median_" + metric] for b in backends]
        rows.append([label] + [heap_change(v) if metric == "change_bytes" else
                              f"{v / 1048576:,.2f}" if metric.endswith("bytes") else f"{v:,.0f}" for v in values])
    table(rows)
    lines.extend(["## Validation and limits", "",
                  f"All {run['validation']['cross_backend_samples']:,} Bevy samples match Flecs operation counts, "
                  "hit counts and exact checksums. Each harness validates results and tracked teardown.", "",
                  "This is a microbenchmark of these APIs and representations, not a whole-engine ranking. " +
                  ("Bevy/Flecs do not retain reset defaults inside the measured store. " if run.get("mutable_instance_rows") else
                   "Bevy/Flecs do not provide the measured Defold default/override ownership contract. ") +
                  "Observed timing ranges, raw samples, compiler versions and hashes are retained with the results.", "",
                  "[Timing summary](timing/summary.csv) · [Memory summary](memory/summary.csv) · "
                  "[Run manifest](run.json) · [Standalone HTML](../../report.html#comparison)", ""])
    (folder / "RESULTS.md").write_text("\n".join(lines), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[2] / "benchmarks"
    # An in-progress run must not replace the last completed comparison.
    completed = []
    for path in (source / "comparisons").glob("*/run.json"):
        run = json.loads(path.read_text(encoding="utf-8"))
        if run.get("completed_utc"):
            completed.append((run["completed_utc"], path, run))
    if not completed:
        parser.error("No completed benchmark comparison found")
    _, manifest, recorded_run = max(completed, key=lambda item: item[0])
    folder = manifest.parent
    prefix = folder.relative_to(source).as_posix() + "/"
    files = {}
    for kind in ("timing", "memory"):
        for path in sorted((folder / kind).glob("*.csv")):
            raw = path.read_bytes()
            relative = path.relative_to(folder).as_posix()
            sha = hashlib.sha256(raw).hexdigest()
            if sha != recorded_run["results_sha256"][relative]:
                raise ValueError(f"Recorded result hash differs: {path}")
            files[prefix + relative] = {"text": raw.decode("utf-8"), "sha256": sha}
    # Include measurement provenance, without implementation history or parent comparisons.
    keys = (
        "id", "date_utc", "completed_utc", "platform", "machine", "cpu", "rows", "samples",
        "packed_rows_per_registration", "compiler_cpp", "compiler_rust", "optimization",
        "measurement", "contracts", "workloads", "traversal", "flecs_describe", "flecs_revision",
        "bevy_version", "inline_composition", "row_field_iterators", "structural_lock",
        "separate_workloads", "case_query_setup", "dense_query_count", "named_field_selection",
        "field_pointer_access", "query_field_handles", "mutable_instance_rows",
        "shared_resource_tables", "pooled_registration_slots", "native_c_layout", "file_version",
        "source_sha256", "binary_sha256", "commands"
    )
    run = {key: recorded_run[key] for key in keys if key in recorded_run}
    run["validation"] = {key: recorded_run["validation"][key] for key in (
        "cross_backend_samples", "checks", "cpp_workloads", "profile_modes", "rust_build"
    ) if key in recorded_run["validation"]}
    run["results_sha256"] = {name[len(prefix):]: file["sha256"] for name, file in files.items()}
    comparison = {
        "run": run,
        "timing": numeric_rows(files[prefix + "timing/summary.csv"]["text"]),
        "memory": numeric_rows(files[prefix + "memory/summary.csv"]["text"]),
    }
    add_memory_metrics(comparison["memory"], files, prefix + "memory/", "dense.csv", "packed-rows{}.csv")
    metadata = json.dumps(run, indent=2) + "\n"
    files["measurement.json"] = {"text": metadata, "sha256": hashlib.sha256(metadata.encode("utf-8")).hexdigest()}
    write_comparison_markdown(comparison, folder)
    payload = {"comparison": comparison, "files": files, "case_descriptions": CASE_DESCRIPTIONS}
    template = Path(__file__).with_name("benchmark_report.html").read_text(encoding="utf-8")
    assert template.count("__BENCHMARK_DATA__") == 1
    # Data cannot close its containing script element.
    serialized = json.dumps(payload, separators=(",", ":"), allow_nan=False).replace("<", "\\u003c")
    output = args.output or source / "report.html"
    output.write_text(template.replace("__BENCHMARK_DATA__", serialized), encoding="utf-8")
    print(f"Created {output}: {run['id']}, {len(comparison['timing'])} timing rows, "
          f"{len(comparison['memory'])} memory rows, {len(files)} embedded files")


if __name__ == "__main__":
    main()
