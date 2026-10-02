# Graphics packet threading proof of concept

This experiment keeps component rendering, render Lua, culling, batching and
geometry generation on the game thread. It records graphics operations into owned
packets and executes the adapter calls inline or on a graphics worker. It is a
bounded native comparison of Solution 1, not the complete asynchronous graphics
frontend described in the design proposal.

## Select an execution mode

`render.graphics_packets` defaults to 0:

| Setting | Execution |
| --- | --- |
| 0 | Existing direct graphics calls |
| 1 | Record packets and consume them inline |
| 2 | Record packets and consume them on a worker |

The existing `render.sprite_snapshot=0/1/2` settings are unchanged. Nonzero
graphics packet mode rejects simultaneous sprite snapshots, mixed preparation
and sprite traces. Compare these paths independently; combining both threading
boundaries would introduce a different experiment. Timer and QoS treatments
remain separate opt-ins.

The benchmark project's reusable runner accepts modes 0 through 4: original,
component inline, component threaded, graphics inline and graphics threaded.
Its mode numbers are condition identifiers, not values for `graphics_packets`.
For example, `--modes 0,1,2,3,4 --mixed-preparation` selects all five conditions;
mixed preparation is automatically disabled for graphics modes.

Native execution currently requires macOS/Metal. The null adapter supports focused
tests. Startup resource loading completes before packet mode starts; shutdown
drains the owner and GPU callbacks before restoring direct execution and normal
world/context destruction.

For a direct launch, pass `--graphics-adapter=metal
--config=render.graphics_packets=2` to the PoC engine. Keep
`render.sprite_snapshot=0` and `render.mixed_preparation=0`; the explicit compiled
project path must be the final argument. Use packet mode 1 for the inline control.
`render.graphics_packet_delay_us=0..1000000` adds an artificial worker delay for
correctness stress only and defaults to zero. It is not a benchmark treatment.

## Recording and ownership

`graphics_packet.cpp` installs a frontend adapter table while retaining the real
backend table for execution. Packet commands contain copied scalar/structure
arguments and owned vertex/index upload and uniform payloads. Uploaded bytes are
copied before the caller returns. Published packets contain no pointer to the
caller's temporary upload or constant memory.

Two reusable packet slots allow recording N+1 while the owner executes N.
Publication waits for the previous CPU consumer; there is at most one outstanding
packet. Neither frame replacement nor unordered draw merging is used. The worker
preserves the recorded order of state changes, uploads and draws. It calls backend
functions directly, avoiding recursive recording.

Each packet has a 32 MiB retained capacity ceiling. Allocation replacement can
temporarily retain old and new allocations; a separate growth counter reports
that peak. The buffer-size metadata table is bounded at 65,536 records. Oversized
packets and unsupported graphics calls fail explicitly, rather than dropping
commands or using an unsafe direct fallback. This is a fail-fast PoC policy.

Pipeline state, viewport and changed buffer sizes have producer-side values, so
ordinary rendering queries see recorded changes without draining every draw.
Immutable shader reflection and capability queries use the stable preloaded
metadata. Mutation/destruction barriers finish CPU consumers before wrappers,
program metadata or vertex declarations can be changed or freed.

Native buffer/texture retirement remains the adapter's responsibility. CPU packet
completion permits reuse of copied command bytes; it does not imply GPU completion.
Metal copies upload data into backend-owned storage before returning. Texture
uploads issued during packet frame execution join the active Metal command buffer,
preserving draw/upload/draw ordering instead of submitting an upload ahead of
uncommitted draws.

## Resource requests and synchronous operations

Resource creation, texture upload, deletion, shader compilation/reload and readback
use a synchronous owner request. It first consumes preceding packet commands and
then executes the requested operation on the graphics owner. The producer blocks
until execution returns, keeping request structures, pointer operands and result
storage alive. Resource requests can split a frame into ordered packet segments.
They are accounted separately from ordinary recorded command payloads.

Creation returns a realized backend handle. This first implementation does not
reserve logical handles ahead of asynchronous creation. Shader compilation errors
and readbacks retain their immediate result behavior. Texture async calls become
synchronous owner requests with a producer-side callback; completion indicates CPU
execution and ordered upload submission, not a general GPU completion fence.

There is one game producer and one synchronous control request at a time.
Independent background recording contexts, concurrent resource producers and a
nonblocking readiness API remain future work. Calls from an unadmitted thread fail.

Platform/window operations remain on main after CPU drain. The main thread updates
the Metal surface before publishing a frame; the worker's BeginFrame avoids window
queries. The private `sprite._snapshot_pause()` benchmark hook also supports packet
modes. Pausing drains submitted work and GPU callbacks, suppresses presentation,
and leaves resource requests serviceable. The existing resource stress fixture
exercises requests during repeated pauses, including requests while already paused.

## Supported scope

The recorded subset includes frame begin/presentation, clear, ordinary render
state, viewport/scissor, vertex declarations, vertex/index buffer uploads and
bindings, programs/constants/samplers, texture bindings and backbuffer rendering.
Built-in component rendering continues through its original path.

Custom render targets, compute, explicit uniform/storage-buffer APIs, native texture
handle escape and graphics-handle invalidation are rejected. The experiment does
not claim general native extension, streaming, device-loss or other-backend support.
Main-thread graphics query/metadata paths are audited for this admitted subset.
Resource creation/mutation can stall the pipeline; measure those stalls separately
from steady-state rendering.

## Diagnostics and validation

`sprite._get_snapshot_stats()` reports `mode=graphics-inline` or
`mode=graphics-threaded`, plus `graphics_packet_*` counters for submissions,
completions, commands, copied bytes, synchronous calls, wait/execution time,
recording overlap, retained capacity, allocation growth, per-frame bytes, buffer
metadata and reserved worker stack. Sampling does not force a drain. A sampled
threaded completion may trail submission by one. Execution time includes owner
requests; it is CPU wall time, not GPU time.

Packet capacity includes service/slot storage and the buffer metadata allocation.
The worker stack reservation is separate. Neither includes component geometry
scratch, resource payloads retained by the engine or opaque driver allocations.
RSS and backend allocation measurements overlap these scopes and must not be
added together. Stable retained capacity after cleanup differs from a live leak.

The graphics test suite covers copied bytes, partial updates, producer state
queries, queue bounds, resource ordering, paused requests, immediate completion,
unsupported operations and capacity rejection. Build and run through the existing
CMake PoC trees; ASan and TSan checks are correctness evidence, not timing evidence.
The native `test_app_graphics metal graphics-packets` fixture verifies that an
in-frame texture upload changes a later draw without changing the earlier one.
Add `packet-inline` to exercise the same sequence through the inline recorder.

The benchmark tool `tools/verify_graphics_packets.py` runs exact sprite/GUI/text/
stencil pixel comparisons across all five modes. `--stress --modes 3,4` additionally
exercises particles, texture/atlas replacement, scratch resource creation/deletion
and paused servicing. Particle randomness is excluded from cross-process pixel
equality. Native windows stay visible without requesting focus.

The existing deterministic gameplay runner provides checkpoint equivalence and a
combined performance/memory/energy report. Short implementation-validation runs
must be labeled diagnostic. Establishing a speedup requires counterbalanced longer
runs with frozen content/engine, consistent display/power conditions and both
inline controls. Actual energy remains a separately captured measurement.

The report also pairs component-threaded/component-inline,
graphics-threaded/graphics-inline and graphics-threaded/component-threaded trials
within the same repetition, cap and timer policy. These comparisons separate
recording overhead from the effect of the worker and compare the two boundaries.

Implementation-validation evidence is saved in the benchmark project's
[`graphics-packet-validation-20261002-v1`](</Users/jhonny/dev/defold-projects/experiment-decoupled-rendering/results/graphics-packet-validation-20261002-v1/README.md>)
directory, including frozen engines, source snapshots, logs, pixel captures,
resource stress, memory samples and the five-mode gameplay report.

## Related documents

- [Component snapshot implementation](SPRITE_SNAPSHOT_POC.md).
- [Planned render-layer implementation](RENDER_LAYER_POC_PLAN.md).
- [Reusable benchmark runner](</Users/jhonny/dev/defold-projects/experiment-decoupled-rendering/REPLAY.md>).
