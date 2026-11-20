
# The renderer frame

This document describes how game data becomes draw calls, including the internal
snapshot path being built for decoupled rendering. Production still renders inline;
built-in components have not yet migrated to snapshots.

See [Consumer preparation validation](RENDER_CONSUMER_VALIDATION.md) for regression
coverage, CPU benchmark results and the limits of those measurements.

## Recording and consuming a frame

Game work and render work have different owners. In inline mode they run on the
same thread; the internal threaded tests run the same consumer on another thread.

1. **Reserve storage before simulation.** Gamesys acquires a reusable frame slot.
   Inline uses one slot; threaded execution uses two. The game may build N+1 while
   N is rendering, but must wait for N to finish before publishing N+1.
2. **Capture component inputs.** After final transforms and before destructive
   `PostUpdate`, opted-in components copy their render inputs into the packet.
   These are values and read-safe references, not pointers into mutable game worlds.
3. **Record passes.** Lua stays on the game owner. Render-script commands and their
   matrices, predicates, constants and per-draw camera values are copied into the
   same packet. Publication happens only when component capture and command
   recording both succeed. Failure cancels the whole unpublished packet.
4. **Prepare on the consumer.** `BeginRenderFrameConsumption` applies captured
   timing, resets submitted lights and begins the render list. Component consumers
   update their renderer-owned items from the captured inputs, then submit them.
5. **Execute every pass.** Finalize the list, then replay commands in order.
   Each draw selects, culls, sorts and batches the items for that pass.
6. **Finish CPU readers.** After all passes, each component's `EndFrame` clears its
   borrowed input bindings. `EndRenderFrameConsumption` then clears render objects,
   debug and text data once for the complete frame.
7. **Retire on the game owner.** Release packet references and reuse the CPU slot.
   GPU work can still be running: CPU retirement does not authorize deleting GPU
   resources or reusing upload bytes that the backend still needs.

Accepted packets are never dropped or replaced. Deleting a game world does not
delete its renderer state while an accepted packet still refers to it.

## Temporary inline compatibility

Unmigrated components still submit render data directly. The production gamesys
context selects legacy preparation so their existing order remains unchanged:

`BeginRender → PreRender → Render (list reset and components) → render messages →
SubmitRender → PostUpdate → EndRender → profiler/PostRender/presentation`

Here, `BeginRender` sets both recording/execution timing and resets lights;
`EndRender` clears render objects, debug and text after `PostUpdate`, including
failed or skipped drawing. Consumption preserves these inline submissions.

Snapshot contexts instead use `BeginRender` only to set game-owned recording time,
and `EndRender` does nothing. Their consumer owns renderer setup and cleanup.
Legacy preparation cannot be combined with threaded execution; snapshot consumers
reject component render callbacks that have not migrated.

## Render script

When the user calls `render.draw(pred, {frustum = frustum, constants = constants})`
in the render script, it records a draw pass that may later generate several draw
calls. Changing a camera or constant buffer after recording does not change an
already captured pass.

Also note that each frame, the render script will most often call `render.draw()` multiple times in order to produce multiple effects.

## Render items and list entries

An entry represents one renderable thing, such as a sprite. It holds bounds,
position, material tags, ordering/batch information and its dispatch association.

The existing persistent render-item API remains in use. A component renderer owns
its item handles and dispatch; these can survive many frames. During consumption
it refreshes them from the snapshot, disables unused items and submits its dispatch.
Item user data can identify a renderer slot, but must not point into a game pool.
Transient render-list entries remain supported as well.

These lifetimes are separate:

- **Captured inputs:** immutable packet data, borrowed through component `EndFrame`.
- **Persistent items and caches:** renderer-owned state reused between frames.
- **Draw scratch and visibility:** consumer-owned working data rebuilt or refreshed
  for the current frame/view, never written back into the captured inputs.

## Pruning

### Material tags

The predicate in `render.draw(pred)` selects material tags. Entries that do not
match are skipped.

### Frustum culling

When a pass supplies a frustum, central culling tests typed sphere, AABB or OBB
bounds stored in the renderer's arrays. Components must supply conservative bounds:
too-small bounds can hide visible geometry.

Items without typed bounds use their dispatch's custom visibility callback. If
that callback is null, they are always visible. Custom culling runs on the render
owner and follows the same input-lifetime rules as drawing. Different views can
produce different visible sets from the same snapshot; a pass without a frustum
resets visibility rather than inheriting the previous pass's result.

## Batching

After selection and culling, the renderer calculates view-dependent sort values
and builds the pass's draw order. Stable sorting preserves the existing ordering
rules, including ordering between different component dispatches.

Adjacent entries with compatible dispatch, batch key and minor order form batches.
The renderer does not group all entries of one component together at the expense
of transparency or stencil ordering.

Each batch is sent to the corresponding component type's render dispatch function.
The batching call has 3 stages:

* `RENDER_LIST_OPERATION_BEGIN` - Called once per selected dispatch in this draw pass.
* `RENDER_LIST_OPERATION_BATCH` - Called once per batch.
* `RENDER_LIST_OPERATION_END` - Called once per selected dispatch in this draw pass.

These are **pass callbacks**, not complete-frame callbacks. A dispatch can run in
several passes, or never run if nothing is selected. Captured inputs remain valid
until the separate component `EndFrame`, which runs once per consumed capture after
all passes, even when nothing was drawn or component preparation reported an error.


## Render Object

For each render batch call, the component type takes the list of render list entries and converts them into a single `RenderObject`.
It holds info about render constants, material, vertex/index buffer etc.

The render object is then registered with the renderer. The render object represents an actual draw call.

## Render constants

Each `RenderObject` may have a set of render constants assigned to it.
Usually they come from the material that is used.

The render script has the ability to override the constants in the `render.draw(pred, {constants = constants })` call.

## Current limits

The snapshot tests prove the CPU ownership contract, not general thread safety of
the render context or graphics API. Production threading remains gated off.

- Recorded resource handles do not yet retain/version materials, textures,
  programs or render targets. Material tag tables and other shared resource data
  still require safe lifetime and mutation rules.
- Frame metadata and camera preparation still query window/display state on the
  producer. Owner-routed surface queries and context-loss handling remain later work.
- Built-in component, light, debug/text, profiler and extension submissions remain
  on the serialized production path until migrated.
- Culling and geometry preparation are serial on the render owner. Jobification,
  GPU culling and production native/web executors are separate steps.
