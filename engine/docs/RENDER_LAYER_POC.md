# RenderFrame layer PoC

Implementation notes for the native macOS/Metal experiment. The design and
proposed evaluation protocol remain in [RENDER_LAYER_POC_PLAN.md](RENDER_LAYER_POC_PLAN.md).
The completed comparison is generated as `RENDERING_POC_SUMMARY.md` after the
collection passes its integrity audit.

## Selection

Select one pipeline at startup:

```ini
[render]
poc_pipeline = renderframe
poc_threaded = 1
```

`poc_threaded=0` consumes the same frame inline. Pipelines `legacy`, `component`
and `graphics` select the earlier paths; `legacy` requires threading off. Missing
selectors keep legacy behavior. Explicit old `sprite_snapshot` / `graphics_packets`
aliases cannot be combined with named selectors, even when the old value is zero.
Timer policy and QoS are independent, default-off experiments.

The reusable benchmark runner has condition IDs 0–7: modified direct, component
inline/threaded, graphics inline/threaded, render-layer inline/threaded, and a
separately supplied unmodified vanilla binary. These IDs are not config values.

## Ownership and extraction

`dmRender::RenderFrameBuilder` builds two reusable frame slots. A slot owns an
aligned data arena, global entries, captured pass commands/constants, adopted
sprite arrays, and deduplicated resource dependencies/generations. Published
entries address frame data; registered consumers receive renderer scratch and a
const frame, not a component-world callback context.

Capture occurs after simulation/transforms and before PostUpdate. Main records
render Lua into producer-owned storage while the owner consumes the previous
frame. At publication it drains the previous CPU reader, retires that frame's
resource references, prepares the platform surface on main, and submits the new
frame. At most one published frame is incomplete; frames are neither replaced
nor dropped. GPU lifetime is separate from CPU slot retirement.

- Sprites use the existing evaluated-input/geometry helpers, with frame-owned
  resolved resource adapters instead of live wrappers in consumer data.
- GUI boxes copy geometry inputs. Ordinary text copies strings, layout parameters,
  constants and stencil/order state; the consumer owns text-layout/render caches,
  geometry, buffers and render objects. Game-side text-metric queries drain the
  consumer before accessing shared font data; layout-cache misses also drain.
  These conservative barriers can reduce overlap in changing-text workloads.
- Standalone and GUI particles copy evaluated per-particle inputs. Simulation,
  transforms and random advancement stay on main; geometry expansion and packing
  run on the consumer. GUI particle node transforms are applied before capture.
- Render Lua stays on main. Captured views, projections, viewports, predicates,
  frusta and per-draw constants survive later mutations and Lua collection.

Frame capacity and conservative allocation-growth coexistence are admitted
against 32 MiB per slot / 64 MiB total. This includes adopted sprite storage and
owned commands, not renderer scratch, GPU storage or all transitive resources.
Overflow is explicit failure, not truncated rendering.

## Persistent graphics owner

The new path reuses the graphics owner service, but normal frame drawing executes
backend calls immediately on that owner. It does not build a second graphics
frame packet behind each RenderFrame.

Owner servicing starts after main-thread window/context setup and before bootstrap
resource realization. Supported create/upload/delete/query operations use explicit
requests, with conservative CPU/GPU drains for mutation. Synchronous requests may
stall main. Initial clears, normal rendering, resource mutations and teardown keep
graphics execution on the owner; required platform/window work stays on main.

`QueueGraphicsOwnerRequest` is a private bounded interface for copied task data
from resource-job producers. The callback executes on the owner, and its completion
runs on main at a service boundary. Embedded references remain the caller's
lifetime responsibility; callers must coordinate admission/mutation and must not
embed temporary data or live component pointers for consumption. This is not a
public general-purpose extension API. Accepted work drains before shutdown.
Auxiliary request payloads share an 8 MiB retained budget and a 256-request limit;
saturation is reported to the caller.

The focused extension-style fixture exercises initialize, frameless upload, frame
work and drained finalize with a real graphics buffer on both policies. General
extension frame-hook registration/migration is not implemented. Legacy pre/post
render hooks are explicitly rejected rather than silently skipped.

## Admitted content and limitations

The native experiment admits sprites, ordinary GUI boxes/text, standalone and GUI
particles, and the existing opt-in producer-owned physics/sound/preloaded-factory
subset used by the offline Underwatermelon replay.

Labels, tilemaps, models/meshes, general streaming, GUI pie/custom nodes, rich text
and custom text styles, arbitrary extension graphics, custom render targets,
compute, general camera integration, other backends and device-loss recovery are
not included. Resource replacement uses drains, not non-stalling old/new native
resource versions. Existing sprite-only scratch counters do not measure all new
GUI/particle renderer memory; use process RSS/footprint plus separately reported
frame/request capacity, without summing overlapping scopes.

Correctness fixtures cover captured sprite/particle geometry after mutation and
deletion, owned per-pass constants, arena overflow/partial failure, owner requests
and completion, paused service, ordinary resize, new glyphs, mixed GUI ordering,
and the unchanged gameplay replay. Particle randomness prevents an exact gameplay
pixel-equality claim. The synthetic deterministic pixel fixtures compare direct
and both new policies byte for byte. ASan/TSan validation precedes optimized
performance collection.
