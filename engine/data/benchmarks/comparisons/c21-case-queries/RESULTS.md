# Defold, Flecs and Bevy benchmarks

1,000,000 instances; 7 measured samples after one warmup. All five backends were measured in this run.

Bevy uses the standalone ECS crate with default table storage and normal change tracking, without a renderer or scheduler. Struct variants use a fixed typed adapter for shared fields; columns query components directly. Light is inline in all backends. Defold binds light.color once per table and reads it with the public Vector3 field getter. Defold holds a structural lock during traversal (included in timing); cursor lifetimes are caller obligations, with no revision or parent-step checks. Each workload has its own Defold/Flecs function and source file; backend/workload selection happens outside row loops. Query construction lives in each C++ case file. Explosion owns a separate query with writable health and read-only position; Flecs health/position scans declare both fields read-only. All backends create five dense queries and two resource-instance queries. Creation and field binding remain outside timed traversal and are measured separately. Defold appends mutable rows to shared tables per loaded resource, with pooled registration records and 32-bit row membership indices. The million-instance packed fixture has six physical tables and 62,500 logical registrations. Each registration still resets/unloads independently; different resource handles remain separate. Query-local field handles select cached offsets, and typed pointers access mutable bytes directly. Numeric writes allocate nothing. Packed defaults stay shared; replacement payloads belong to each registration. Decoded tables retain owned reset defaults.

Timings are **median time per operation**, with ns/µs/ms/s chosen to fit each value: queries use visited rows, explosions use candidate rows, insertion/removal use affected instances, and random access uses accesses. Lower is better. Read queries use five passes. C++ and Rust ran in separate processes; their order alternated between layouts. Different compilers and API contracts are included in the result.

## Runtime-created instances

**Sum all light colors**

Find SpotLight and PointLight instances tagged as lights and sum all three components of light.color. Five read passes per sample.

**Explosion**

Scan health and position; subtract 25 health, clamped at zero, from instances within radius 50 of the origin. One pass from reset defaults.

**Shuffled position**

Visit each instance once in shuffled order, read its position, increase X by 1, and write it back. Starts with reset defaults.

| Measurement | Defold | Flecs structs | Flecs columns | Bevy structs | Bevy columns |
| --- | ---: | ---: | ---: | ---: | ---: |
| Bulk creation | 28.2 ns | 2.82 ns | 2.69 ns | 35.5 ns | 51.7 ns |
| Individual creation | 37.7 ns | 127 ns | 201 ns | 39.9 ns | 67.7 ns |
| Add 10%, batches of 100, live queries | 40.9 ns | 11.3 ns | 11.4 ns | 71.2 ns | 92.9 ns |
| SpotLight color query | 0.724 ns | 0.668 ns | 0.61 ns | 0.753 ns | 0.659 ns |
| Sum all light colors | 0.709 ns | 0.662 ns | 0.646 ns | 0.712 ns | 0.623 ns |
| Health + position query | 1.05 ns | 0.654 ns | 0.644 ns | 0.807 ns | 0.854 ns |
| Enemy health + position query | 1.04 ns | 0.656 ns | 0.627 ns | 0.763 ns | 0.832 ns |
| Explosion | 3.45 ns | 2.64 ns | 2.6 ns | 2.96 ns | 3.07 ns |
| Shuffled position | 176 ns | 68.4 ns | 63 ns | 63.8 ns | 61.9 ns |
| Remove 1% | 79.1 ns | 193 ns | 229 ns | 354 ns | 450 ns |
| Replace 1% | 281 ns | 406 ns | 704 ns | 82.8 ns | 119 ns |
| Health query after churn | 1.03 ns | 0.643 ns | 0.609 ns | 0.788 ns | 0.816 ns |

## Instances from resource blobs

Defold stores 1,000,000 mutable rows in six shared runtime tables, across 62,500 registrations of 16 rows. Registration size determines the reset/unload group, not physical table capacity. Only this registration size is measured in the current run.

Flecs and Bevy instantiate mutable copies of the same prototype values. These cases do not measure file I/O or native borrowed-blob reset/unload.

| Measurement | Defold | Flecs structs | Flecs columns | Bevy structs | Bevy columns |
| --- | ---: | ---: | ---: | ---: | ---: |
| Population + ID collection | 7.95 ns | 2.26 ns | 2.3 ns | 35 ns | 50.8 ns |
| Light color query | 0.702 ns | 0.675 ns | 0.633 ns | 0.713 ns | 0.634 ns |
| Health query after 10% writes | 1.08 ns | 0.668 ns | 0.651 ns | 0.812 ns | 0.84 ns |
| First scalar writes to 10% | 177 ns | 66.9 ns | 65.8 ns | 59.8 ns | 57.2 ns |
| Repeat scalar writes to 10% | 168 ns | 64.8 ns | 58.5 ns | 56.9 ns | 50.9 ns |
| Random scalar get after writes | 67.6 ns | 27.3 ns | 20.8 ns | 52.7 ns | 44.6 ns |

## Runtime memory

Requested heap; fixture inputs, caller ID arrays, shared source assets, allocator overhead and RSS are excluded. Heap change is after minus before, calculated per sample before taking the median. Allocation requests include resizes; retained totals and live blocks include earlier phases.

| Measurement | Defold | Flecs structs | Flecs columns | Bevy structs | Bevy columns |
| --- | ---: | ---: | ---: | ---: | ---: |
| Bulk creation, retained MiB | 124.38 | 92.01 | 88.63 | 132.44 | 142.48 |
| Bulk creation, allocation requests | 22 | 1,104 | 1,165 | 275 | 316 |
| Create 5 live queries, heap change | +3.15 KiB | +125 KiB | +72.8 KiB | +2.72 KiB | +1.34 KiB |
| Packed population, retained MiB | 89.21 | 92.01 | 88.63 | 132.44 | 142.48 |
| Packed population, allocation requests | 440 | 1,104 | 1,165 | 275 | 316 |
| Create two queries, allocation requests | 10 | 39 | 18 | 91 | 36 |
| Packed tables + 10% writes, retained MiB | 89.21 | 92.08 | 88.66 | 132.44 | 142.48 |
| Packed tables + 10% writes, live blocks | 280 | 1,483 | 1,529 | 274 | 286 |
| First writes to 10%, allocation requests | 0 | 0 | 0 | 0 | 0 |
| Explosion, heap change | 0 B | 0 B | 0 B | 0 B | 0 B |
| Explosion, allocation requests | 0 | 0 | 0 | 0 | 0 |
| Shuffled position, heap change | 0 B | 0 B | 0 B | 0 B | 0 B |
| Shuffled position, allocation requests | 0 | 0 | 0 | 0 | 0 |

## Validation and limits

All 896 Bevy samples match Flecs operation counts, hit counts and exact checksums. Each harness validates results and tracked teardown.

This is a microbenchmark of these APIs and representations, not a whole-engine ranking. Bevy/Flecs do not retain reset defaults inside the measured store. Observed timing ranges, raw samples, compiler versions and hashes are retained with the results.

[Timing summary](timing/summary.csv) · [Memory summary](memory/summary.csv) · [Run manifest](run.json) · [Standalone HTML](../../report.html#comparison)
