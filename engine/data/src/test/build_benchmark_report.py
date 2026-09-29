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

"""Build the standalone report for the eight agreed ECS workloads."""

import argparse
import csv
import hashlib
import json
import math
import statistics
from pathlib import Path

CORE_CASES = [
    {
        "id": "create_population", "title": "Create population", "unit": "instances created",
        "description": "Populate an empty store with SpotLight, PointLight, Player, Enemy, Pickup and Breakable instances from prepared values. Bulk-create each type once.",
        "tests": "Allocation, ID creation and value initialization. Input generation and type registration are excluded.",
    },
    {
        "id": "spawn_wave", "title": "Spawn wave", "unit": "instances added",
        "description": "Add 10% of the initial population in batches of 100, preserving the six-type mix. Movement, Explosion and nearby-light queries already exist and remain alive.",
        "tests": "Insertion into a populated store, including capacity growth and maintaining live queries. Input generation is excluded.",
    },
    {
        "id": "despawn_wave", "title": "Despawn wave", "unit": "instances removed",
        "description": "After the spawn wave, remove 1% of the initial population individually by ID. A seeded shuffle selects original instances across all six types; the three queries remain alive.",
        "tests": "Scattered deletion, storage compaction or reuse, and maintaining live queries. Choosing IDs and checking stale handles are excluded.",
    },
    {
        "id": "movement", "title": "Movement", "unit": "Player/Enemy rows updated",
        "description": "Visit all Players and Enemies once. Read velocity and update all three position coordinates using position += velocity × 1/64 second.",
        "tests": "Query traversal and sequential component reads/writes. Query creation and restoring starting positions are excluded.",
    },
    {
        "id": "explosion_r50_first", "title": "Explosion", "unit": "candidate rows scanned",
        "description": "Query position and health across Players, Enemies and Breakables. Scan every candidate; within radius 50 of the origin, subtract 25 health, clamped at zero. The radius test runs in the row loop, without a spatial index.",
        "tests": "Field reads across types, distance filtering and conditional health writes. Query creation and restoring health are excluded.",
    },
    {
        "id": "nearby_lights", "title": "Nearby light contribution", "unit": "light rows scanned",
        "description": "Scan SpotLights and PointLights. For lights within radius 50 of the origin, read inline Light.color and Light.intensity and accumulate (r + g + b) × intensity × (1 − distance² / 2500). No values change.",
        "tests": "Nested struct access, distance filtering and a scalar reduction in one query pass. Query creation is excluded.",
    },
    {
        "id": "position_lookup", "title": "Shuffled position lookup", "unit": "IDs looked up",
        "description": "Read every instance's position once through a seeded shuffled list of IDs spanning all six types. Reads use public ID access APIs, including Defold's typed batch getter; accumulate the coordinates in shuffled order without changing them.",
        "tests": "Random ID lookup and field access. Initial shuffling is excluded; mapping entries to runtime IDs, filling caller buffers and accumulating values are included.",
    },
    {
        "id": "threaded_update", "title": "Threaded update",
        "description": "A frame overlaps component updates with incoming-content preparation, then applies content changes after the updates finish.",
        "roles": [
            {"label": "Main thread", "description": "Starts update scheduling and prepares incoming resource handles/templates while workers run. Once all update jobs finish, it replaces mixed content, spawns/despawns enemy waves, and releases temporary resources."},
            {"label": "Worker threads", "description": "Process query partitions: move Players/Enemies, apply explosion damage, regenerate health (+0.25, capped at 100), and sum nearby light contributions. Jobs read/write existing rows; additions and removals stay on the main thread."},
            {"label": "Synchronization", "description": "Conflicting updates wait; independent jobs may overlap. Defold/Flecs/EnTT admission, dispatch, completion processing and retries run on the main thread. Bevy uses an extra schedule coordinator thread. Worker counts refer only to threads doing row work."},
        ],
        "tests": "Complete-frame cost: dispatch, query work, conflict waiting, content preparation and committed additions/removals. Initial population, thread-pool setup, validation and file I/O are excluded.",
    },
]
CORE_BACKENDS = (("data", "Defold"), ("flecs_columns", "Flecs"), ("bevy_columns", "Bevy"), ("entt", "EnTT"))

def read_csv(text):
    return list(csv.DictReader(line for line in text.splitlines() if not line.startswith("#")))

def threaded_activity_html(rows, population):
    """One shared population timeline for the identical backend/worker schedules."""
    fields = ('frame', 'live_rows', 'live_enemies', 'content_spawned', 'content_despawned', 'enemies_spawned', 'enemies_despawned')
    activity = [{key: int(row[key]) for key in fields} for row in rows
                if row['backend'] == 'Defold' and int(row['workers']) == 1]
    values = [0] + [row['live_rows'] - population for row in activity]
    maximum = max(values) or 1
    # A step occurs when streaming commits at the end of each frame.
    x = lambda i: 70 + 600 * i / len(activity)
    y = lambda n: 154 - 130 * n / maximum
    path = f'M {x(0):.2f} {y(0):.2f}' + ''.join(f' H {x(i):.2f} V {y(n):.2f}' for i, n in enumerate(values[1:], 1))
    grid = ''.join(f'<line x1="70" x2="670" y1="{y(n):.2f}" y2="{y(n):.2f}" stroke="#dbe3dd"/>'
                   f'<text x="60" y="{y(n) + 4:.2f}" text-anchor="end">{n:,.0f}</text>' for n in (0, maximum / 2, maximum))
    ticks = sorted({0, len(activity), *range(8, len(activity), 8)})
    labels = ''.join(f'<text x="{x(i):.2f}" y="177" text-anchor="middle">{i}</text>' for i in ticks)
    points = ''.join(f'<circle cx="{x(i):.2f}" cy="{y(values[i]):.2f}" r="3" fill="#b65a24"><title>'
                     f'After frame {r["frame"]}: {r["live_rows"]:,} total, {r["live_enemies"]:,} Enemies; '
                     f'wave +{r["enemies_spawned"]:,} / −{r["enemies_despawned"]:,}</title></circle>'
                     for i, r in enumerate(activity, 1))
    table_rows = ''.join('<tr>' + ''.join(f'<td>{r[key]:,}</td>' for key in fields) + '</tr>' for r in activity)
    return f'''<h3>Enemy waves · live after streaming</h3>
<p style="font-size:13px;line-height:1.6;margin:8px 0">Total population ranges from {population:,} to {population + max(values):,}.
The chart shows added wave enemies; the original mixed population includes {population * 24 // 100:,} Enemies.
All backends and worker counts use this same seeded schedule, including warmup frames.</p>
<svg viewBox="0 0 700 205" role="img" aria-label="Live enemy wave population after each frame" style="display:block;width:100%;font:12px system-ui;fill:#52666e">
<title>Live enemy wave population</title><desc>Bursts and removals change the next frame's query population. Wave enemies are fully despawned twice per eight-frame cycle.</desc>
{grid}<path d="{path} L 670 154 Z" fill="#b65a24" fill-opacity=".12"/><path d="{path}" fill="none" stroke="#b65a24" stroke-width="2"/>{points}{labels}
<text x="370" y="200" text-anchor="middle">Completed frames</text></svg>
<details><summary>Spawn/despawn counts for every frame</summary><div style="overflow:auto"><table>
<thead><tr><th>Frame</th><th>Live rows</th><th>Live Enemies</th><th>Mixed +</th><th>Mixed −</th><th>Wave +</th><th>Wave −</th></tr></thead>
<tbody>{table_rows}</tbody></table></div></details>'''


def load_threaded(source, files):
    folder = source / "threaded"
    manifest = folder / "manifest.json"
    if not manifest.exists():
        return None
    run = json.loads(manifest.read_text(encoding="utf-8"))
    for name, expected in run["files"].items():
        raw = (folder / name).read_bytes()
        sha = hashlib.sha256(raw).hexdigest()
        if sha != expected:
            raise ValueError(f"Recorded threaded result hash differs: {name}")
        files["threaded/" + name] = {"text": raw.decode("utf-8"), "sha256": sha}
    for name in ("manifest.json", "library-sizes.json"):
        raw = (folder / name).read_bytes()
        sha = hashlib.sha256(raw).hexdigest()
        if name == "library-sizes.json" and sha != run["library_sizes_sha256"]:
            raise ValueError("Recorded library-size hash differs")
        files["threaded/" + name] = {"text": raw.decode("utf-8"), "sha256": sha}

    summary = []
    for backend, prefix in (("Defold", ""), ("Flecs", "flecs-"), ("Bevy", "bevy-"), ("EnTT", "entt-")):
        samples = {}
        for kind in ("timing", "memory"):
            text = files[f"threaded/{prefix}{kind}.csv"]["text"]
            if "# sanitizer=none;" not in text or ("# memory=" in text) != (kind == "memory"):
                raise ValueError(f"Unexpected instrumentation in threaded {backend} {kind}")
            samples[kind] = read_csv(text)
        for workers in run["workers"]:
            selected = {}
            for kind, rows in samples.items():
                selected[kind] = [row for row in rows if row["backend"] == backend and int(row["workers"]) == workers]
                if [int(row["frame"]) for row in selected[kind]] != list(range(run["frames"])):
                    raise ValueError(f"Incomplete threaded {backend} {workers}-worker {kind} results")
                selected[kind] = [row for row in selected[kind] if int(row["frame"]) >= run["warmup"]]
            timing, memory = selected["timing"], selected["memory"]
            times = sorted(int(row["frame_us"]) / 1000 for row in timing)
            requests = lambda row: sum(int(row[f"{domain}_requests"]) for domain in ("store", "job", "caller", "resource"))
            delta = lambda row: sum(int(row[f"{domain}_delta_bytes"]) for domain in ("store", "job", "caller", "resource"))
            summary.append(dict(backend=backend, workers=workers, frame=statistics.median(times),
                p95=times[math.ceil(len(times) * .95) - 1],
                update=statistics.median((int(row["frame_us"]) - int(row["mutation_us"])) / 1000 for row in timing),
                mutation=statistics.median(int(row["mutation_us"]) / 1000 for row in timing),
                requests=statistics.median(requests(row) for row in memory),
                update_requests=statistics.median(requests(row) - int(row["stream_store_requests"]) for row in memory),
                stream_requests=statistics.median(int(row["stream_store_requests"]) for row in memory),
                delta=statistics.median(delta(row) for row in memory) / 1024,
                peak=statistics.median(int(row["peak_additional_bytes"]) for row in memory) / 1024))
    activity = read_csv(files['threaded/timing.csv']['text'])
    return {"run": run, "summary": summary, "activity_html": threaded_activity_html(activity, run['rows']),
            "sizes": json.loads(files["threaded/library-sizes.json"]["text"])}


def load_core(source, files):
    folder = source / "core"
    run = json.loads((folder / "run.json").read_text())
    for name, sha in run["files"].items():
        raw = (folder / name).read_bytes()
        if hashlib.sha256(raw).hexdigest() != sha:
            raise ValueError(f"Recorded core result hash differs: {name}")
        files["core/" + name] = {"text": raw.decode(), "sha256": sha}
    raw = (folder / "run.json").read_bytes()
    files["core/run.json"] = {"text": raw.decode(), "sha256": hashlib.sha256(raw).hexdigest()}
    summary = []
    for backend, label in CORE_BACKENDS:
        timed = read_csv(files[f"core/{backend}-timing.csv"]["text"])
        measured = read_csv(files[f"core/{backend}-memory.csv"]["text"])
        for case in CORE_CASES[:-1]:
            samples = [r for r in timed if r["operation"] == case["id"]]
            memory = [r for r in measured if r["operation"] == case["id"]]
            if len(samples) != run["samples"] or len(memory) != run["samples"]:
                raise ValueError(f"Missing samples: {backend} {case['id']}")
            median = lambda key: statistics.median(int(r[key]) for r in memory)
            summary.append(dict(operation=case["id"], backend=label, workers=None,
                time=statistics.median(float(r["total_ms"]) for r in samples),
                minimum=min(float(r["total_ms"]) for r in samples),
                maximum=max(float(r["total_ms"]) for r in samples),
                operations=int(samples[0]["operations"]),
                requests=statistics.median(int(r["allocations"])+int(r["reallocations"]) for r in memory),
                delta=statistics.median(int(r["live_bytes"])-int(r["before_bytes"]) for r in memory),
                peak=statistics.median(int(r["peak_bytes"])-int(r["before_bytes"]) for r in memory)))
    return {"run":run,"summary":summary}

def validate_unity_core(rows, reference, population, samples):
    actual = {(r["operation"], int(r["sample"])): r for r in rows}
    expected = {(c["id"], s) for c in CORE_CASES[:-1] for s in range(1, samples + 1)}
    baseline = {(r["operation"], int(r["sample"])): r for r in reference}
    if len(rows) != len(expected) or set(actual) != expected:
        raise ValueError("Unity must contain exactly seven standalone cases per sample")
    for key, row in actual.items():
        other = baseline[key]
        if row["backend"] != "unity" or int(row["rows"]) != population:
            raise ValueError(f"Unity fixture differs: {key}")
        for field in ("rows", "operations", "hits"):
            if int(row[field]) != int(other[field]):
                raise ValueError(f"Unity {key}: {field} differs from Defold")
        if not math.isclose(float(row["checksum"]), float(other["checksum"]), rel_tol=1e-7, abs_tol=1e-7):
            raise ValueError(f"Unity {key}: checksum differs from Defold")
        elapsed = float(row["total_ms"])
        if not math.isfinite(elapsed) or elapsed <= 0:
            raise ValueError(f"Invalid Unity timing: {key}")


def validate_unity_threaded(rows, reference, run, workers):
    actual = {(int(r["workers"]), int(r["frame"])): r for r in rows}
    expected = {(w, f) for w in workers for f in range(run["frames"])}
    baseline = {int(r["frame"]): r for r in reference if r["backend"] == "Defold" and int(r["workers"]) == 1}
    if len(rows) != len(expected) or set(actual) != expected:
        raise ValueError("Incomplete Unity threaded results")
    for (worker, frame), row in actual.items():
        if row["backend"] != "Unity" or sorted(row["admission_order"]) != list("0123"):
            raise ValueError("Invalid Unity threaded backend/task order")
        for field in ("live_rows", "live_enemies", "content_spawned", "content_despawned", "enemies_spawned", "enemies_despawned"):
            if int(row[field]) != int(baseline[frame][field]):
                raise ValueError(f"Unity frame {frame}: {field} differs from the shared wave schedule")
        if not math.isclose(float(row["light_sum"]), float(baseline[frame]["light_sum"]), rel_tol=1e-7, abs_tol=1e-7):
            raise ValueError(f"Unity frame {frame}: light checksum differs")
        elapsed, mutation = float(row["frame_us"]), float(row["mutation_us"])
        if not (math.isfinite(elapsed) and math.isfinite(mutation) and 0 <= mutation <= elapsed and elapsed > 0):
            raise ValueError(f"Invalid Unity frame timing: {worker} / {frame}")


def load_unity(source, files, core, threaded):
    folder = source / "unity"
    manifest = folder / "run.json"
    if not manifest.exists():
        return None
    run = json.loads(manifest.read_text())
    if run["memory_measured"] or run["sanitizer"] != "none" or run["safety_checks"]:
        raise ValueError("Unity report requires release timings without memory/safety/sanitizer instrumentation")
    if (run["rows"], run["samples"]) != (core["run"]["rows"], core["run"]["samples"]):
        raise ValueError("Unity population/sample count differs from the core run")
    if run["reference"]["core"] != hashlib.sha256((source / "core/run.json").read_bytes()).hexdigest():
        raise ValueError("Unity was validated against a different core run; rerun Unity")
    for name, expected in run["files"].items():
        raw = (folder / name).read_bytes()
        sha = hashlib.sha256(raw).hexdigest()
        if sha != expected:
            raise ValueError(f"Recorded Unity result hash differs: {name}")
        files["unity/" + name] = {"text": raw.decode(), "sha256": sha}
    raw = manifest.read_bytes()
    files["unity/run.json"] = {"text": raw.decode(), "sha256": hashlib.sha256(raw).hexdigest()}
    rows = read_csv(files["unity/timing.csv"]["text"])
    validate_unity_core(rows, read_csv(files["core/data-timing.csv"]["text"]), run["rows"], run["samples"])
    for case in CORE_CASES[:-1]:
        selected = [r for r in rows if r["operation"] == case["id"]]
        times = [float(r["total_ms"]) for r in selected]
        core["summary"].append(dict(operation=case["id"], backend="Unity", workers=None,
            time=statistics.median(times), minimum=min(times), maximum=max(times),
            operations=int(selected[0]["operations"]), requests=None, delta=None, peak=None))
    if run["threaded"]:
        if run["validation"]["threaded"] != "unity-safety":
            raise ValueError("Unity threaded results require recorded Jobs/Burst safety validation")
        if run["reference"]["threaded"] != hashlib.sha256((source / "threaded/manifest.json").read_bytes()).hexdigest():
            raise ValueError("Unity was validated against a different threaded run; rerun Unity")
        for field in ("frames", "warmup", "workers"):
            if run["threaded"][field] != threaded["run"][field]:
                raise ValueError(f"Unity threaded {field} differs")
        all_rows = []
        for workers in run["threaded"]["workers"]:
            all_rows += read_csv(files[f"unity/threaded-{workers}.csv"]["text"])
        validate_unity_threaded(all_rows, read_csv(files["threaded/timing.csv"]["text"]), threaded["run"], run["threaded"]["workers"])
        for workers in run["threaded"]["workers"]:
            selected = [r for r in all_rows if int(r["workers"]) == workers and int(r["frame"]) >= run["threaded"]["warmup"]]
            times = sorted(float(r["frame_us"]) / 1000 for r in selected)
            threaded["summary"].append(dict(backend="Unity", workers=workers, frame=statistics.median(times),
                p95=times[max(0, math.ceil(.95 * len(times)) - 1)],
                update=statistics.median((float(r["frame_us"]) - float(r["mutation_us"])) / 1000 for r in selected),
                mutation=statistics.median(float(r["mutation_us"]) / 1000 for r in selected),
                requests=None, delta=None, peak=None, update_requests=None, stream_requests=None))
    return run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[2] / "benchmarks"
    files = {}
    core = load_core(source, files)
    threaded = load_threaded(source, files)
    if not threaded:
        parser.error("The eight-case report requires a completed threaded benchmark run")
    unity = load_unity(source, files, core, threaded)
    payload = {"cases":CORE_CASES,"core":core,"threaded":threaded,"unity":unity,"files":files}
    template = Path(__file__).with_name("benchmark_report.html").read_text()
    assert template.count("__BENCHMARK_DATA__") == 1
    serialized = json.dumps(payload, separators=(",", ":"), allow_nan=False).replace("<", "\\u003c")
    output = args.output or source / "report.html"
    output.write_text(template.replace("__BENCHMARK_DATA__", serialized))
    print(f"Created {output}: exactly 8 workloads, 8 performance cards and 8 memory cards")

if __name__ == "__main__":
    main()
