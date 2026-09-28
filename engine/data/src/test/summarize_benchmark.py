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

"""Summarize timing or memory benchmark CSV samples; do not mix measurement modes."""

import argparse
import csv
import statistics
import sys
from collections import defaultdict


MEMORY_FIELDS = (
    "before_bytes", "live_bytes", "peak_bytes", "live_blocks", "peak_blocks",
    "allocations", "reallocations", "frees", "allocated_bytes",
    "total_allocations", "total_reallocations", "total_frees", "total_allocated_bytes",
    "fixture_bytes", "fixture_blocks", "shared_bytes", "shared_blocks",
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", nargs="+")
    args = parser.parse_args()
    groups = defaultdict(list)
    mode = None
    for filename in args.files:
        with open(filename, encoding="utf-8") as stream:
            lines = stream.readlines()
        packed = next((line.split("=", 1)[1].split(";", 1)[0]
                       for line in lines if line.startswith("# packed_rows=")), "0")
        reader = csv.DictReader(line for line in lines if not line.startswith("#"))
        current_mode = "memory" if "live_bytes" in (reader.fieldnames or []) else "timing"
        if mode is not None and mode != current_mode:
            raise ValueError("Summarize timing and memory runs separately")
        mode = current_mode
        for row in reader:
            key = (int(row["rows"]), int(packed), row["backend"], row["operation"])
            groups[key].append(row)
    writer = csv.writer(sys.stdout)
    columns = ["rows", "packed_rows", "backend", "operation", "samples", "operations", "hits", "batches"]
    if mode == "memory":
        columns += [f"{stat}_{field}" for field in MEMORY_FIELDS for stat in ("median", "min", "max")]
    else:
        columns += ["median_ms", "median_ns_per_operation", "min_ns_per_operation", "max_ns_per_operation"]
    writer.writerow(columns)
    for key, rows in sorted(groups.items()):
        for name in ("operations", "hits", "checksum"):
            if len({row[name] for row in rows}) != 1:
                raise ValueError(f"Inconsistent {name} for {key}")
        samples = [int(row["sample"]) for row in rows]
        if len(set(samples)) != len(samples):
            raise ValueError(f"Duplicate samples for {key}; summarize separate runs separately")
        if mode == "memory":
            output = [*key, len(rows), rows[0]["operations"], rows[0]["hits"], rows[0]["batches"]]
            for field in MEMORY_FIELDS:
                values = [int(row[field]) for row in rows]
                output += [statistics.median(values), min(values), max(values)]
            writer.writerow(output)
            continue
        ns = [float(row["ns_per_operation"]) for row in rows]
        ms = [float(row["total_ms"]) for row in rows]
        writer.writerow([*key, len(rows), rows[0]["operations"], rows[0]["hits"],
                         rows[0]["batches"], f"{statistics.median(ms):.6f}",
                         f"{statistics.median(ns):.3f}", f"{min(ns):.3f}", f"{max(ns):.3f}"])


if __name__ == "__main__":
    main()
