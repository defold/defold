# Render layer threading proof of concept plan

Draft for implementation, 2 October 2026. Configuration names and interfaces below
are proposed; this document does not enable or implement another renderer.

Implement a unified immutable `RenderFrame` path and compare it with the original
renderer and the existing component snapshot experiment. The experiment should
establish whether moving the remaining render preparation to the render owner
improves throughput, pacing, memory use or energy use on the same workloads.
Keep threading and timer experiments opt-in.

## Relationship to the existing implementation

The original design explicitly says that Solution 2, threading at the render
layer, and Solution 3, splitting component game/render APIs, overlap. The former
defines the consumed frame; the latter supplies its safe inputs. Component-level
threading does not mean a thread per component.

The existing PoC already implements much of Solution 2 for sprites: immutable
sprite data, captured Lua commands, a bounded worker, and worker-side render-list
construction, culling, sorting, batching, geometry and graphics submission.
Consequently, this comparison measures two implementations of that ownership
boundary. A new name or wrapper around the existing sprite path would not establish
an independent threading result.

The material differences proposed for the new mode are:

| Area | Existing component experiment | New render frame experiment |
| --- | --- | --- |
| Frame ownership | Sprite slots, separate command slots, one shared prepared GUI/particle packet set | One frame owns all admitted component inputs, entries, passes, constants and retained dependencies |
| GUI and particles | Main generates geometry after draining the previous consumer | Main extracts immutable inputs while the previous frame is consumed; render owner generates geometry |
| Dispatch | Sprite-specific engine orchestration plus prepared draw replay | Render-layer orchestration dispatches frame-local records through registered consumers |
| Graphics mutations | Drain CPU/GPU work and temporarily return graphics ownership to main | Typed requests execute on the render owner; conservative drains may still be necessary |
| Views and constants | Restricted shared view/projection and command subset | Owned per-pass state and per-draw constants, with independent per-view visibility |

Sprite-only gains may be small because the current sprite path already overlaps
the same major stages. GUI/text, particles and resource changes are the principal
additional hypotheses to test. Reduced copying or better batching is a separate
source of improvement from threading and must be measured separately.

## Comparison modes

Select the pipeline and execution policy at process startup. Each benchmark trial
starts a fresh process. Live switching is outside this PoC because it introduces
resource migration and drain semantics unrelated to the comparison.

Proposed settings are `render.poc_pipeline=legacy|component|renderframe` and
`render.poc_threaded=0|1`:

| Report condition | Pipeline | Threaded | Purpose |
| --- | --- | --- | --- |
| Original | legacy | 0 | Requested no-threading baseline; default |
| Component threaded | component | 1 | Preserve the earlier PoC as the second primary comparison |
| Render frame threaded | renderframe | 1 | New Solution 2 implementation; third primary comparison |
| Component inline | component | 0 | Existing restructuring control |
| Render frame inline | renderframe | 0 | Execute the new extraction/consumption and resource contracts on main |

Reject `legacy + threaded`. Keep `render.sprite_snapshot=0/1/2` as aliases for
Original, Component inline and Component threaded. Reject simultaneous explicit
old and new selectors rather than silently choosing one. Missing new settings
default to `legacy` and `0`; log and export the resolved condition.

Retain existing component admission flags and behavior on the component path.
The new path has an explicit supported-component set; it must not interpret
`mixed_preparation` as permission to fall back to main-thread prepared geometry.
The runner selects the appropriate admission settings for each named condition.
Physics and sound remain producer-owned in all conditions.

Extend the runner/report schema to record pipeline, execution policy, capability
set, frame schema, queue limits, timer, QoS, backend and engine/content hashes.
Continue reading old numeric-mode reports without relabeling them as new results.
Verify runtime activation, not just command-line arguments. Run the primary three
conditions plus both inline controls when attributing gains.

## Initial scope

Target macOS/Metal, one main collection and the existing synthetic scenes and
offline Underwatermelon replay. Complete sprites, standard GUI boxes/text and
standalone particles through extraction and consumption. Retain the current
gameplay-only physics/sound support and preloaded factory use.

The common comparison suite uses only features supported by all three primary
conditions. Separate capability tests exercise two distinct views, per-draw
constants and a small experimental extension fixture in the new path. The old
threaded path may report those tests unsupported; it must never silently skip work
and enter a performance comparison.

Labels, tilemaps, models/meshes, general collection streaming, custom GUI nodes,
arbitrary third-party graphics extensions, custom render targets, compute and
other graphics backends remain follow-ups. Capture explicit camera matrices;
general camera-component integration is deferred. Test ordinary resize and
surface recreation; device-loss recovery is deferred with a clear error path.
Persistent scene proxies, parallel component simulation, GPU culling and a
general low-level graphics command recorder are outside this experiment.

## Frame and ownership contract

Introduce `RenderFrameBuilder` for producer writes and a const `RenderFrame` view
for consumption in `dmRender`. A proposed frame contains:

- Frame ID, simulation tick/time, surface generation and copied surface dimensions.
- Global entries with component-consumer ID, frame-local payload index, bounds,
  ordering information, predicate tags and batch keys.
- Typed sprite, GUI/text and particle input arrays, plus aligned variable payloads.
- Ordered pass commands, copied view/projection/viewport state, frusta and constants.
- A deduplicated table of exact render-visible resource dependencies and generations.

Use offsets/indices for variable data while building, resolving immutable views
after allocation growth ends. No entry userdata, dispatch context or hidden table
may dereference a live component world, GUI node, emitter, mutable resource wrapper
or Lua object. Consumer registration identifies code and renderer-owned state;
it does not carry component-world pointers into callbacks. Debug validation should
check offsets, consumer IDs, generations and known forbidden memory ranges.

The game owner retains simulation, Lua, extraction, resource-factory bookkeeping,
platform events and required window calls. A render owner holds the render context,
visibility/sort/batch scratch, generated geometry, render objects, glyph/render
caches, dynamic GPU buffers, graphics calls and presentation. Main-side render Lua
records into a producer facade rather than mutating the worker's render context.
Metadata queries use safe copied metadata or explicit request completions.

Initially retain exactly two CPU slots and the existing bounded policy:

```text
Slot:     Free -> Building -> Ready -> Reading -> Free
Game:     simulate and extract N+1 while render consumes N
Handoff:  wait for N CPU consumption, then publish N+1
Render:   consume frame inputs, cull, batch, generate, upload and submit
```

Permit at most one published incomplete frame and one frame under construction.
Preserve ordered consumption without replacement or dropped side effects. Use
mutex publication or release/acquire synchronization. Retirement and cancellation
must release every retained dependency exactly once on its designated owner.

CPU completion means no consumer still reads slot bytes. GPU buffer reuse and
resource destruction require backend completion separately. Copy or transfer
asynchronous uploads to independently retained staging before freeing the CPU
slot. Renderer scratch is shared across sequential consumption, not duplicated
per slot unless actual concurrent readers require it.

Preserve global ordering across component types. Cull independently for each
view, retain stable transparent/stencil ordering, and batch only compatible
contiguous runs. Multiple draws of a frame must not advance animation or simulation.
Clip/stencil operations need explicit ordering dependencies and conservative
culling; an invisible clipping writer cannot be discarded if later draws use it.

### Extraction timing

Establish the logical frame boundary with tests before changing scheduling.
The current inline path renders before post-update; the threaded sprite path
captures after post-update and system messages. Equal gameplay checkpoints alone
do not prove identical visible deletion/message timing.

For the new path, target capture at the original render phase after update/final
transforms and before destructive post-update. Preserve render-script message
ordering and collect owned command state for that same simulation tick before
publication. No additional simulation step may intervene. Test deferred creation,
deletion, render-script messages and resource replacement at this boundary.
If the old component path differs, preserve and document it during the audit;
make any necessary correctness fix explicit and rebaseline every mode before
comparison. Do not count a different visible workload as a speedup.

## Graphics requests and resource lifetime

Give the new mode a bounded owner request lane for resource create/upload/delete,
dynamic buffers, glyph texture updates and lifecycle controls. These are auxiliary
operations; ordinary rendering remains high-level frame consumption followed by
immediate `dmGraphics` calls on the owner.

Each request owns its payload and dependencies and carries a sequence/dependency
boundary, result and completion destination. Data prepared by existing resource
jobs may reach the lane through the game owner initially. Jobs must not bypass
the owner with graphics calls; a general multi-producer loader redesign is deferred.

Reserve logical identities on the producer where needed. Initially allow a native
producer to wait for create/compile/query results through an explicit completion.
The render owner must not call Lua, acquire producer-held factory locks, or wait
for a callback on the blocked producer. Cache immutable reflection/size metadata
for extraction. Report creation failures before publishing dependent frames.

Request serialization alone is insufficient: an N+1 texture update must not
change resources needed by frame N. Define upload-before-use and last-reader
dependencies and service only eligible operations. Keep this lane progressing
during startup, pause and periods without presentation.

Use a conservative resource policy first. Stop admission for affected resources,
drain CPU readers and required GPU work, and execute mutation on the render owner.
Only then update producer metadata/generation and resume extraction. An additional
factory reference does not make in-place recreation immutable. A frame's dependency
table must resolve either an immutable version or a version protected by this
drain policy, including material/program, texture set, texture and geometry data.
Defer non-stalling replacement versions to a measured follow-up.

Audit startup, resource load/recreate/destroy, font caches, render-script immediate
operations, readback, debug/profiler drawing and extensions. Initialize owner
servicing before supported graphics resources are realized, after the required
main-thread platform setup. Document any backend-required initialization exception.
There must be no temporary return of normal graphics mutation ownership to main
in the completed new threaded mode. Reject unsupported paths before side effects.

Proposed initial admission limits are 32 MiB per complete CPU frame slot, including
all component arrays, commands, references and allocation growth coexistence;
64 MiB for the two slots. Bound auxiliary upload requests separately at 8 MiB of
retained payload, 16 MiB during allocation replacement, and 256 outstanding
requests. These are experiment limits, not performance targets or total memory
budgets. Report renderer scratch and GPU allocations separately. Validate limits
against fixtures before freezing them for comparisons. Oversized frames/requests
fail explicitly; queue saturation uses measured backpressure at a safe boundary.

## Implementation milestones

### 1 Preserve comparisons and define the boundary

Add the named condition resolver, legacy aliases, activation metadata and runner
schema. Inventory component dispatch and graphics bypasses. Freeze the current
reference binary/content and add the extraction-timing fixtures described above.

Acceptance: default and aliases select the existing behavior; invalid combinations
fail; old reports remain readable; all common fixtures have an explicit visible
frame definition and supported-capability check.

### 2 Build RenderFrame and the inline sprite reference

Introduce the builder/const frame, global entries, retained-dependency table and
consumer registry. Adapt the existing sprite extraction and geometry helpers to
the new interface while retaining the component mode's orchestration. Consume
the new frame inline. Keep renderer state and allocation accounting separate from
live sprite worlds.

Acceptance: deterministic output and geometry match the original for supported
sprites, including animation, slice9, trimmed geometry, constants and attributes.
Mutate/delete/compact components after capture and before consumption; the frame
must remain unchanged. Exercise overflow, partial capture failure and cleanup.

### 3 Own pass commands and support multiple views

Capture Lua output into the frame, including predicates, copied matrices, frusta,
per-draw constant blocks and state changes. Resolve mutable operands before
publication. Partition render-script recording state from consumer render state.
Support the existing backbuffer commands plus two distinct views and per-draw
constants. Keep other command families explicitly rejected until implemented.

Acceptance: Lua constant/matrix changes after recording cannot change frame N;
each view has correct visibility; transparent interleaving and GUI stencil order
are preserved. Recording never resets worker-owned constant cursors or scratch.

### 4 Add the worker and graphics owner request lane

Reuse the tested queue protocol where possible, with a generic frame callback.
Implement the owned request lane, dependency ordering, conservative reload policy,
owner assertions, CPU retirement and GPU retirement. Route initialization and
shutdown as well as steady-state operations. Preserve main-thread window work
through an explicit synchronized surface handoff.

Acceptance: artificially slow producer/consumer tests prove bounded overlap;
owner assertions detect every audited wrong-thread path; loading and upload
requests complete without frames. Texture mutation/deletion during a delayed frame
preserves that frame's content. No consumer touches resource-factory state.

At this point a sprite-only three-way comparison is useful, but does not complete
the mixed-component Solution 2 experiment.

### 5 Move GUI text and particle geometry into consumption

For GUI, capture resolved transforms, layout values, color, texture/material
bindings, order and clipping information. Copy text and layout inputs; game-side
layout/query APIs may retain CPU work, while glyph quad generation, render cache
mutation, upload and drawing move to the render owner. All glyph data crossing
owners needs an immutable lifetime; warm-cache tests must be supplemented by
new-glyph tests.

For particles, capture evaluated particle render values, emitter bindings,
animation/attribute data, bounds and constants. Implement geometry generation
over those values instead of live particle-context arrays. Keep simulation,
random-number advancement and callbacks on the producer.

Remove the new mode's reliance on `PrepareMixedFrame` and the shared prepared
geometry/upload set. The existing component mode retains that implementation for
comparison. Reuse pure geometry math where practical without sharing mutable state.

Acceptance: extraction for all admitted types overlaps the previous consumer;
no main-thread GUI/particle vertex generation remains in the new mode. Verify
changing text, clipping, node deletion, particles after another simulation update,
cross-component ordering and repeated passes. Measure particle count and copied
bytes, not only emitter capacity. Any retained layout work is reported explicitly.

### 6 Validate extensions and lifecycle behavior

Add a private experimental extension fixture with render-owner initialize,
per-frame and finalize hooks, an owned frame payload and one frameless graphics
task. Keep this internal until its contracts are proven. Invoke frame hooks after
captured passes with a defined target/viewport and graphics-state handling.
Accepted tasks receive exactly one game-owner completion, including failure or
cancellation. Unsupported legacy graphics hooks fail admission; never silently
move them to another thread.

Exercise pause, resume, resize, resource churn, failed initialization and shutdown
with frames/uploads outstanding. Tag surface work with a generation and drain
before replacement in this native PoC. Shutdown stops admission, drains accepted
work, finalizes render callbacks while dependencies exist, delivers game callbacks,
retires GPU objects and tears down contexts in order. Owner servicing must remain
alive throughout the required drain.

Acceptance: no deadlocks, stale frame/resource reads, lost completions or callbacks
after extension/world destruction. A deliberately unsupported graphics call fails
with a useful reason. No claim of general extension or device-loss compatibility.

### 7 Collect a controlled comparison and decision report

Extend the existing reusable runner, deterministic replay, stage traces, memory
probes and energy capture rather than introducing a second harness. Run focused
CMake tests, ASan and TSan before optimized performance collection. Each new test
declaration gets the repository-required comment describing its verification.

Run all five conditions on the common suite; show the requested three primary
conditions prominently. Compare each threaded path to its own inline control,
both threaded paths to Original, and Render frame threaded to Component threaded.
Attribute draw-count/batching and extraction changes separately from overlap.

Acceptance: publish one report with raw trials, frozen inputs, correctness results,
capability/admission results, performance, memory, energy availability and limitations.
Record excluded/failed runs and the reasons; do not quietly retry away failures.

## Benchmark protocol and decision criteria

Use sprite-only controls at existing low/high counts; GUI/text-heavy scenes;
particle-heavy scenes; mixed CPU-heavy scenes; the fill-heavy negative control;
and the deterministic offline Underwatermelon replay. Put dynamic textures,
atlas changes, glyph churn and pause/reload in a separate resource/lifetime suite.

For steady-state synthetic tests, start with six counterbalanced repetitions,
10 seconds warm-up and 60 seconds measured per condition at requested uncapped,
60 and 120 updates/s. Confirm actual presentation/update behavior and discard no
outliers merely for being slow. For gameplay, record and validate a longer fixed
replay so the shortest uncapped condition still has a useful measurement window;
use the identical tick/event sequence in every condition. Add 10-minute steady
and bounded-churn memory runs for both threaded paths and the original.

Use one optimized engine binary and identical compiled content for the new
comparisons. Historical runs are context only. Keep timer policy 0 and default
QoS, identical queue lead/framebuffer/draw workload, AC power and verified awake
state. Launch visible windows without taking focus; hidden/minimized rendering
is a separate condition because it can change scheduling or presentation behavior.
Track display/occlusion information where available and flag throughput regime
changes like the prior real-game result. Do not pool unexplained regimes.

Collect update and consumed-frame throughput; update and presentation intervals
where measurable; p50/p95/p99; producer extraction and blocked time; consumer
cull/sort/batch/geometry/upload/submit time; draw counts; frame age; CPU and wakeups;
and GPU timings when supported. Keep traced diagnostic trials separate from the
untraced performance collection. CPU timestamps are not physical input-to-display
latency; any claim about the latter requires a dedicated end-to-end measurement.

Measure active/retained/growth-peak frame bytes, request/upload bytes, per-consumer
scratch, GPU logical allocations, retained old resource versions, thread stack,
process RSS/footprint and available backend allocation counters. Scopes overlap
and must not be summed. Verify cleanup live counts and repeated-cycle capacity
stability rather than requiring allocator-retained RSS to return to startup.

Measure power with actual time-aligned capture of a stated scope. Report joules
per equivalent replay/tick and power at fixed caps, with coverage and background
activity limitations. Missing energy is unavailable, not inferred from CPU or
wakeups. Use sufficiently long runs for the sampling interval.

Provisional performance gates should be frozen before collection:

- Correctness is mandatory: matching supported deterministic pixels/geometry and
  gameplay checkpoints, preserved ordering/lifetimes, and passing race/lifetime tests.
  Particle image comparisons require controlled randomness; otherwise compare
  captured inputs/geometry without claiming image equality.
- Seek at least 10% median paired throughput improvement over Component threaded
  on a preselected mixed preparation-heavy workload, with a paired confidence
  interval excluding zero. This is a proposed value threshold, not a predicted gain.
- Retain the earlier provisional capped p99 limit of at most 5% median paired
  regression against Original, also showing every trial and Component threaded.
  Verify equal average rates and report misses separately.
- Require bounded owned memory and cleanup, and report incremental RSS and retained
  capacities at equal workload. A production memory ceiling depends on the target
  device budget and remains a rollout decision.
- Treat actual energy, stable representative-game measurements, physical latency
  and a second physical target as prerequisites for broad adoption conclusions.

If threading adds no benefit over its inline control, or the new path adds no
benefit over the old worker, report that result directly. Correctness and a cleaner
ownership model can justify further development independently; they do not prove
a performance win. Stop at the comparison milestone before broad component or
platform migration unless evidence supports that next investment.

## Expected source areas and deliverables

| Area | Expected work |
| --- | --- |
| `engine/engine/src/engine.cpp` | Mode selection, extraction boundary, worker orchestration and lifecycle |
| `engine/render/src/render/` | New frame builder/views/consumers and request service; command ownership; render scratch |
| `engine/gamesys/src/gamesys/components/` | Sprite adapter and GUI/particle extraction/consumption |
| `engine/particle/src/` and GUI/text implementation | Pure render generation from captured inputs; separate mutable render caches |
| `engine/graphics/src/` and Metal adapter | Owner checks, request execution, completion/retirement and surface coordination |
| Resource loaders and extension integration | Dependency retention, routed graphics operations and experimental lifecycle fixture |
| Engine/render/gamesys/graphics tests | Ownership, equivalence, queue bounds and failure/lifecycle coverage |
| Benchmark project tools and replay SDK | Named conditions, activation checks, expanded probes and unified reports |

Deliver a selectable new path and both inline controls, regression/sanitizer
evidence, an updated ownership/admission document and the comparison report with
raw data. Milestones 1 and 2 are the first implementation slice; milestones 3
through 6 complete the defined render-layer scope before the decision collection.

## Sources

- [Original revised design](</Users/jhonny/Downloads/Decoupled rendering - revised highlighted.pdf>), especially pages 3, 5, 7, 8 and 11-15.
- [Current implementation notes](SPRITE_SNAPSHOT_POC.md), including mixed preparation and gameplay admission.
- [Reusable replay runner](</Users/jhonny/dev/defold-projects/experiment-decoupled-rendering/REPLAY.md>).
- Current implementation inspected in `engine.cpp`, `render_thread.cpp`, `render_command.h`, `comp_sprite.cpp`, `comp_gui.cpp` and `comp_particlefx.cpp`.
