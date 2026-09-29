# Data-store benchmark design

The report contains exactly eight runtime workloads for Defold, Flecs, Bevy and
EnTT: seven standalone cases plus the threaded update. A Unity DOTS runner adds
timing results for the same workloads; Unity memory tests are excluded. See the
[core runner](README.md#core-benchmark-suite) and [standalone report](benchmarks/report.html).
Diagnostic harnesses remain available for profiling, but do not feed the report.

## Population

Use one million data-component instances. One Defold row corresponds to one Flecs,
Bevy, EnTT or Unity entity in this fixture, not necessarily one engine game object. Owner IDs
allow several component instances to belong to the same game object.

| Type | Instances | Tags | Fields |
| --- | ---: | --- | --- |
| SpotLight | 100,000 | `light`, `spot_light` | `position`, `light { color, intensity }`, `range`, `inner_cone_angle`, `outer_cone_angle` |
| PointLight | 150,000 | `light`, `point_light` | `position`, `light { color, intensity }`, `range` |
| Player | 10,000 | `actor`, `player`, `damageable` | `position`, `health`, `velocity` |
| Enemy | 240,000 | `actor`, `enemy`, `damageable` | `position`, `health`, `velocity` |
| Pickup | 250,000 | `pickup` | `position`, `amount` |
| Breakable | 250,000 | `breakable`, `damageable` | `position`, `health` |

Light fields follow the engine light resources, with inline `Light light`
composition. Gameplay types are fixtures. `Light` contains color and intensity;
position belongs to SpotLight or PointLight. Vectors use three float32 values;
other numeric fields use double precision. Field order differs between types
to exercise table-specific field offsets.

All backends receive the same seeded positions, colors, health, identities and
shuffled operation order. The current single-thread suite uses health in
`[100, 200]`. The threaded regeneration workload uses `[0, 100]`, including values
at and just below the cap. Validation uses a sequential reference and checks
counts, hits and values; floating-point reductions allow rounding differences
when parallel execution changes summation order. For the threaded workload, the
reference replays the actual successful access-reservation order for each run.

## Core suite

Show eight performance cards followed by eight matching memory cards. Threaded
worker counts and its phase/p95 details belong to one workload. Library size is a
separate build metric. Do not add cases by enumerating raw CSV operation names.
Each card explains the starting population, operation, and costs being tested,
including setup excluded from measurement. State the amount of work in meaningful
units (created instances, scanned candidates or looked-up IDs); timings cover the
complete operation, or a complete frame for the threaded workload.

| Workload | Operation |
| --- | --- |
| Create population | Instantiate one million rows across the six types from prepared input data. |
| Spawn wave | Add 100,000 instances in batches of 100 with three live queries. |
| Despawn wave | Remove 10,000 instances from a deterministic shuffled subset, after spawning. |
| Movement | Update Player and Enemy position from velocity and a fixed timestep. |
| Explosion | Read position and health; subtract 25 health, clamped at zero, within radius 50 of the origin. |
| Nearby light contribution | Match lights, read position and nested color/intensity, and sum `color * intensity * (1 - distance_squared / radius_squared)` within a fixed radius. |
| Shuffled position lookup | Read positions through a shuffled list of instance IDs. |
| Threaded update with content streaming | Run component updates on caller-scheduled jobs, while preparing content and applying loads/unloads at synchronization points. |

Nearby light contribution uses a squared-distance falloff within radius 50. It
replaces the overlapping SpotLight/all-light scans. Shuffled position lookup is
read-only. Movement uses a 1/64-second timestep. Each query makes one pass; case
setup, input generation, reset and validation are outside the measurement.

The threaded workload makes Movement, Explosion, Regenerate and light updates
ready together. Regenerate writes `min(100, health + 0.25)` and competes with
Explosion for write access to health. The first successful access reservation
runs; the conflicting task retries after release. Vary the candidate submission
order between frames and allow independent jobs to proceed. There is no fixed
Explosion/Regenerate dependency: this workload must exercise synchronization.
Regenerate is part of the eighth workload, not a ninth case. Defold, Flecs and EnTT use caller-owned threads, jobs, ranges and retries; Bevy uses its
native scheduler and parallel query iterator. See the
[threaded loop and submission pseudocode](README.md#threaded-update-benchmark).
Run this same workload with 1, 2, 4 and 8 workers and include dispatch, completion
handling and committed content changes in frame timing.

The main thread prepares mixed-content replacements and enemy-only waves while
updates run, then commits removals and additions after jobs finish. Seeded waves
alternate large bursts, smaller additions/removals, quiet frames and full wave
despawns. Mixed-content replacements retain the original population; enemy waves
change the number of rows visited by Movement, Explosion and Regenerate next
frame. All backends and worker counts receive identical content changes.
Record spawn/despawn counts and live populations per frame alongside latency and
allocations; validate them and every live value against sequential replay.
The report distinguishes main-thread scheduling, content preparation and committed
structural changes from worker-thread query traversal and value updates. Its worker
counts exclude the main thread and Bevy's additional schedule coordinator; all
their measured frame overhead is included.

## Current implementations

The standalone cases use one dense mutable Defold table per type and separate
Flecs/Bevy components. Light is inline. A fresh store is populated for each sample;
Movement, Explosion and nearby-light queries are created before traversal and
remain alive during spawn/despawn. Query and lookup cases run before the
spawn/despawn phases, keeping their input population at one million. Defold uses cached query field bindings and
public typed field pointers. Flecs uses its C query/field APIs; Bevy uses cached
QueryState iterators with ordinary change tracking. Shuffled lookup uses public
ID getters. Defold uses typed batch reads; gathering IDs into caller buffers and
reducing returned values are timed. Native construction binds scalar overrides
and copies shared defaults inside the timed call, including nested Light fields;
reset baselines and input values remain equivalent to decoded construction.
Each case keeps query creation beside its traversal implementation.
EnTT uses separate sparse component pools and ordinary public views, without
owning groups or custom storage. It bulk-creates IDs and bulk-inserts prepared
component values, including owner/component identities and the same type/tags.
The three views remain alive during spawn/despawn. Its 64-bit IDs, `view.each`,
`registry.get` and `registry.destroy` use the public API; C++20 is confined to
the EnTT benchmark targets. The common allocation tracker covers its pools.

Unity uses a standalone macOS arm64 player with IL2CPP and Burst AOT, pinned to
Unity 6000.5.7f1 / Entities 1.4.8. Separate `IComponentData` components use native
archetype chunks. Public `EntityManager` bulk creation initializes prepared
values through `ComponentLookup`; chunk queries use `IJobChunk.Run` in standalone
cases. Shuffled lookup uses `ComponentLookup<Position>`. Spawn keeps three queries
alive; despawn calls `DestroyEntity` per ID. The threaded case uses
`IJobChunk.ScheduleParallel`; caller-derived dependency handles exclude component
read/write conflicts. Submission order changes each frame. Component-level
conflicts conservatively serialize Movement and lights despite disjoint entities.
The main thread prepares templates, waits for workers, and applies structural
changes. No component row work is executed by the main thread during updates.

The threaded case instantiates shared immutable resources in Defold and uses the
other libraries' native component creation APIs. It retains the seeded enemy
waves and mixed-content streaming described above. Its 1/2/4/8 worker comparisons
are presented together, with one complete-frame latency chart and memory metrics.

Defold retains reset defaults; the other measured stores do not. Standalone
fixture restoration occurs outside timing. The report uses one configuration,
without struct/column alternatives, packed registration variants or extra scans.

## Measurement and validation

Use seven samples after warmup for the main comparison. Record compiler settings,
library revisions, source/binary hashes and raw samples. The core runner
requires exactly seven cases per sample and compares counts, hits and checksums
across all four backends and both measurement modes; each harness also
validates its operations. Keep timings from separate executable runs clearly
identified; compiler and API differences are part of this comparison.

Memory instrumentation runs in separate binaries. Report operation heap change,
peak requested bytes and allocation requests including resizes, with retained
before/after totals available in detail. Identify fixture inputs, shared source
blobs and caller ID arrays separately. Allocator overhead and RSS are excluded.
The standalone tracker is single-threaded. The threaded workload uses a
separate tracker with thread-local attribution and synchronized counters for store,
job-system, caller and resource allocations. Flecs adds its OS allocation hooks.
Bevy uses the Rust global allocator across application, coordinator and worker
threads and reports combined world/scheduler allocation totals. C++ new/delete
includes dlib containers and EnTT component pools; libc/pthread allocations and OS thread stacks are excluded.

The [threaded runner](src/test/run_threaded_benchmark.py) enforces ThreadSanitizer
(TSAN) validation before collecting release results. Run the deterministic
conflict tests and streaming update workload with 1, 2, 4 and 8 workers, with the
data library, job system, Flecs, EnTT templates, Bevy dependencies, Rust standard library and tests
instrumented. TSAN diagnostics fail
validation. Collect timings and CPU profiles separately using optimized builds
with all sanitizers (including ASAN, TSAN and UBSAN) and allocation instrumentation
disabled. Sanitized workload timings are validation artifacts and must never be
included in performance comparisons.

Unity's prebuilt Jobs runtime cannot undergo the same full TSAN instrumentation.
Its threaded result therefore requires an explicit exception to use Unity's
Collections/Jobs and forced Burst safety checks in a separate development player,
plus per-row sequential replay. The runner defaults to the seven standalone
cases until that exception is enabled. Release timing disables development and
safety checks and verifies that Burst is active; startup, validation and CSV I/O
are outside timing. The report accepts `unity/run.json` only after matching the
shared fixture's counts/checksums and wave schedule. Missing results are shown as
pending, never as zero-cost bars. There are no Unity memory measurements.

Current raw results and the standalone HTML are committed together. Earlier
experiments and profiles remain local. The report builder reads the completed `core/run.json` and `threaded/manifest.json`
and verifies recorded result hashes. No historical result
is presented as a current feature or measurement.

The current threaded comparison includes EnTT alongside Defold, Flecs and Bevy.
EnTT uses public sparse-pool views with caller-owned HJobSystem jobs; caller
read/write pool declarations prevent conflicting updates. Pools remain structurally
unchanged until jobs finish. The report identifies its conservative pool-level
scheduling and 64-bit entity configuration.

Library size is measured separately from runtime heap use. Compare stripped
`data` and Flecs archives, and report EnTT as header-only. Equivalent small linked
programs measure create/query/update/get/destroy code for all three libraries,
relative to a shared empty-program baseline, with dead stripping and no LTO or
sanitizers. Include required linked dependencies in the program delta. Archive
sizes and workload-specific linked sizes answer different questions; do not
present them as interchangeable or describe header-only code as free.
Also report `cloc` code-line totals for library sources and headers, excluding
comments, blank lines, tests, examples, dependencies and duplicate amalgamations.
Count Defold's `engine/data/src` without `test/`, Flecs's `src/` and `include/`,
and EnTT's `src/entt/`. Include all modules regardless of build flags and show
these scopes explicitly; the libraries provide different feature sets.
