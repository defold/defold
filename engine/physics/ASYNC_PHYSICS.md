# Asynchronous (double-buffered) 2D physics

This document describes the optional double-buffered Box2D (v3) 2D physics path: how it
works and what it does **not** support. It is opt-in and off by
default; the synchronous single-world path is unchanged when it is off.

## Why

`b2World_Step` runs on the main thread and can be a large part of the frame. The
double-buffered path lets the step run on a worker thread, concurrently with the rest of
the main-thread frame (scripts, rendering, other components).

## How it works

Two Box2D worlds are kept:

- the **game world** (`m_WorldId`): the world the rest of the engine reads (body
  positions, queries). It is never stepped in the async path.
- the **physics world** (`m_PhysicsWorldId`): a twin that the worker steps.

Each game body has a twin body in the physics world. A Box2D body id embeds its owning
world index, so the twin id is stored separately on the body (`m_PhysicsBodyId`), not
reused across worlds.

### Operation queue

While the worker is simulating, the main thread must not mutate the physics world
directly. Structural changes (create/destroy body, enable/disable, uniform scale, world
gravity) are recorded as typed operations and drained at a safe point when the worker is
idle. Enable/disable applied before a twin exists is folded into the pending create.

### Per-step pipeline (one frame of latency, "N-1")

Each `StepWorld2D` call:

1. waits for the previous step's worker job;
2. delivers that step's results on the main thread: copies the twin transforms/velocities
   onto the game bodies and replays the collected collision, contact-point and trigger
   callbacks (callbacks call into script, so they stay on the main thread);
3. does the main-thread work that needs the physics world while the worker is idle: drains
   the operation queue, runs queued ray casts, debug draw;
4. injects the current game state into the twins (transform, velocity, and shape geometry);
5. snapshots the twin ids and kicks the worker (`b2World_Step` + event collection), then
   returns.

Because game objects are updated from the *previous* step, they lag the simulation by one
step (N-1 latency). Collect (on the worker) only gathers events into POD buffers; deliver
(on the main thread) replays them. Events collected for an object that is deleted before
delivery are filtered out so a callback is never handed a freed object.

The twin's collision geometry is mirrored from the game shape each step (only when it
differs, so resting bodies sleep), because a body can be resized/recreated at
runtime.

### Threading modes

- **Threaded** (default when async is on): the worker runs on a background thread.
- **Inline** (`m_AsyncWorkerInline`, and the automatic fallback on an Emscripten build
  without pthreads): the step runs inline on the calling thread. Same N-1 semantics and
  results as the threaded mode, with no same-frame shortcut.

### Configuration

- `physics.async` (game.project, default 0) turns the double-buffered path on.
- Emscripten threaded builds require SharedArrayBuffer (COOP/COEP headers); without
  pthreads the build falls back to the inline mode.

## Not yet implemented

This path is intended for scenes of freely-simulating dynamic bodies. The following are
**not** supported or not mirrored to the twin, and should be considered before enabling it:

- **Most, but not all, runtime body state is carried game -> twin.** Carried each step: the
  body transform, linear/angular velocity, collision-shape geometry, the collision group/mask
  (filter), and applied force/torque (accumulated on the game body and injected onto the twin
  before it steps). Carried via the operation queue: create/destroy, enable/disable, uniform
  scale, and world gravity. **Not** carried, so they do not affect the simulation under async:
  - linear/angular damping, gravity scale, mass overrides,
  - group hull flips,
  - explicit wake/sleep requests.

- **Joints are not mirrored.** Joint create/destroy/params are not deferred to the twin, so
  joints do not function under the async path.

- **Grid / tilemap shapes are not yet implemented.** The operation queue skips grid shapes and
  the shape mirror ignores them, so tilegrid collision objects are not simulated by the
  twin.

- **Runtime shape changes are only partially handled.** A uniform-scale change and a body
  recreated at a new size are mirrored; changing a shape's *type* at runtime is not.

- **Runtime size changes on multi-shape bodies are not mirrored.** Geometry mirroring is index-based
  and applied only to single-shape bodies, because the physics-world shape enumeration order is not
  guaranteed to match the game body's shape array for multi-shape bodies. Filter mirroring is
  order-independent and does work for multi-shape bodies.

- **Script-driven dynamic-body transforms do not drive the simulation.** With
  `allow_dynamic_transforms`, a dynamic body is physics-driven under async; its game-object
  transform is not pulled back into the simulation (that would re-inject the twin's own transform
  onto the twin, which does not re-step it). Kinematic bodies remain script-driven.

- **N-1 latency is observable.** Game objects reflect the previous step. Ray casts and debug
  draw see that one-step-old geometry. Callback ordering across a frame differs from the
  synchronous path (ray-cast callbacks are separated from the collision stream).

## Tests

`engine/physics/src/physics/test/test_box2d_async.cpp` covers the double-buffered path,
including sync-vs-async parity, sleep/stability, cadence, high velocity, runtime resize,
kinematic follow, spin, deleted-object filtering, create/destroy churn, and three-way
parity across synchronous / threaded / inline modes.
`test_box2d_async_nothreads.cpp` covers the no-threads worker fallback.
