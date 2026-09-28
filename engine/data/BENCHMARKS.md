# Data-store benchmark design

The current C++ and Rust harnesses implement the broader mixed-instance suite.
The smaller core suite and threaded execution described below are proposed, not
implemented. See the [build commands and measurement contracts](README.md#mixed-instance-benchmark)
and [current comparison](benchmarks/comparisons/c21-case-queries/RESULTS.md).

## Population

Use one million data-component instances. One Defold row corresponds to one Flecs
or Bevy entity in this fixture, not necessarily one engine game object. Owner IDs
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
other numeric properties use double precision. Field order differs between types
to exercise table-specific property offsets.

All backends receive the same seeded positions, colors, health, identities and
shuffled operation order. The current single-thread suite uses health in
`[100, 200]`. The proposed regeneration workload uses `[0, 100]`, including values
at and just below the cap. Validation uses a sequential reference and checks
counts, hits and values; floating-point reductions allow rounding differences
when parallel execution changes summation order.

## Proposed core suite

Keep eight workloads, with performance followed by memory comparisons for the
same operations. The report still contains the broader implemented suite until
these cases are implemented and measured.

| Workload | Operation |
| --- | --- |
| Create population | Instantiate one million rows across the six types from prepared input data. |
| Spawn wave | Add 100,000 instances with existing live queries. |
| Despawn wave | Remove a deterministic scattered subset. |
| Movement | Update Player and Enemy position from velocity and a fixed timestep. |
| Explosion | Read position and health; subtract 25 health, clamped at zero, within radius 50 of the origin. |
| Nearby light contribution | Match lights, read position and nested color/intensity, and sum `color * intensity * (1 - distance_squared / radius_squared)` within a fixed radius. |
| Shuffled position lookup | Read positions through a shuffled list of instance IDs. |
| Threaded update with content streaming | Run component updates on caller-scheduled jobs, while preparing content and applying loads/unloads at synchronization points. |

The light calculation uses a simple distance falloff. The proposed light task
replaces the overlapping SpotLight/all-light scans; the current implementation
still sums light colors without a radius test. The proposed shuffled lookup is
read-only; the current shuffled-position case reads position and increments X.

The threaded workload orders Movement, Explosion and Regenerate, with independent
light jobs. Regenerate writes `min(100, health + 0.25)` and tests write/write
conflicts with Explosion. It is part of the eighth workload, not a ninth case.
The caller owns threads, jobs, ranges and dependencies. See the
[threaded loop and submission pseudocode](README.md#proposed-threaded-update-benchmark).
Run this same workload with 1, 2, 4 and 8 workers and include dispatch, completion
handling and committed content changes in frame timing.

## Current implementations

The main report compares Defold with separate Flecs/Bevy components. Complete
struct variants remain in detailed tables. Flecs is an optional external checkout;
Bevy ECS 0.19.1 is pinned in the Rust harness. Neither is a runtime dependency.

Defold uses fixed-stride rows with shared metadata, cached field bindings and
public typed field pointers. Light is inline. Registrations of a loaded resource
share dense physical tables; the current resource fixture has six tables and
62,500 logical groups of 16 rows. Numeric writes modify mutable rows directly.
The source blobs supply shared defaults and payloads for reset.

Flecs traverses queries through `ecs_query_iter`, `ecs_query_next` and
`ecs_field_w_size`. Bevy uses cached `QueryState` iteration and normal component
change tracking. These use registered component identities; complete-struct
variants have a fixed typed adapter for shared fields. They do not dynamically
look up arbitrary property names. Neither measured store retains Defold-style
reset defaults; fixture restoration occurs outside timing.

Each C++ workload defines its queries beside its backend-specific functions.
Query creation and field binding occur outside traversal timing. The current
suite includes creation, five dense query constructions, light/health scans,
Explosion, shuffled position updates, insertion/removal/replacement and resource
lifecycle/access cases. The resource fixture creates two queries. Read scans
repeat five times; writes use one pass. Input generation, reset and validation
are outside the measured operation.

## Measurement and validation

Use seven samples after warmup for the main comparison. Record compiler settings,
library revisions, source/binary hashes and raw samples. The comparison runner
checks Flecs/Bevy workload coverage, counts, hits and checksums; each harness also
validates its operations. Keep timings from separate executable runs clearly
identified; compiler and API differences are part of this comparison.

Memory instrumentation runs in separate binaries. Report operation heap change,
peak requested bytes and allocation requests including resizes, with retained
before/after totals available in detail. Identify fixture inputs, shared source
blobs and caller ID arrays separately. Allocator overhead and RSS are excluded.
The current trackers are single-threaded and must be adapted before measuring the
threaded workload.

Current raw results and the standalone HTML are committed together. Earlier
experiments and profiles remain local. The report builder uses only the latest
completed comparison and verifies recorded result hashes. No historical result
is presented as a current feature or measurement.
