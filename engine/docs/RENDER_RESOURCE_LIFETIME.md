# Resource lifetime across render frames

A frame must keep the data it captured, not simply an address that still exists.
Holding a factory reference prevents destruction, but a reload can still change
the object behind that reference. The initial solution combines retained references
with a conservative mutation barrier. It does not keep multiple resource versions.

## Capturing and releasing

`RetainComponentRenderResource(factory, frame, resource)` adds a factory reference
to a building frame. Capture failure rolls back that reference; cancellation and
normal retirement release it on the game owner. The factory must outlive the frame.
Retain the resource owning borrowed arrays and dependencies, not an interior pointer.
Repeated calls are allowed but use separate reference slots.

A resource-backed render script retains its whole render prototype before recording
Lua commands into an external frame. This also retains its script, declared
materials, compute programs and render targets, and their dependent textures.
Retaining the target keeps its attachments alive. This covers declared bindings inherited from previous
frames, even when the script does not issue a new bind command. The ordinary scratch
update path does not retain the prototype: doing so would create an ownership cycle.

This is deliberately coarser than per-draw retention. Raw command dependency
descriptors still describe use; they do not independently own arbitrary GPU handles.
Migrated components must retain their own resource owners when capturing inputs.

Successful dependency reload refreshes both declared names and matching inherited
material/compute or attachment-texture bindings. Whole-prototype replacement clears
old inherited bindings before publishing the new registry. Material allocation and
prototype dependency acquisition happen before discarding their previous state, so
failure at those stages leaves the previous resource usable.

## Replacement order

`close admission → finish readers → retire CPU references → replace → reopen`

- The resource factory has optional begin/end mutation callbacks around a complete
  recreate transaction, including reload notifications and old-resource destruction.
  They run outside factory/loader locks. Existing factories without callbacks are
  unchanged; the engine connects them to gamesys.
- The queue's mutation barrier is reusable, unlike permanent shutdown. Accepted
  frames finish normally; none are discarded. A native game owner can wait on
  progress signals, while the render owner continues consuming and submitting.
- An empty reservation made before simulation is preserved. If capture has already
  added data or references, mutation is rejected without cancelling that frame.
  The caller must retry at a safe boundary; rejection is not a queued reload.
- Synchronous nested reloads share the scope. Failed recreation still closes its
  scope and reopens admission. Game-owner callbacks cannot capture another frame
  while the scope is active.

Inline CPU retirement occurs before presentation. The engine marks the rendering
interval, including `PreRender` through `Flip`, and rejects mutations until submission
has finished. A `PostUpdate` callback cannot use CPU retirement to replace a texture
while older draws remain in the backend's unsubmitted command buffer.

For an asynchronous graphics replacement, `EnqueueRenderResourceMutation` holds
the scope through task execution and game-owner completion. The task owns a copy
of its payload. The callback publishes the successful replacement before frames
resume; on failure it must keep the previous usable resource or an explicit fallback.
No new handle is ready for capture simply because enqueue succeeded. Pending
mutations reject other mutations, including requests from their completion callback.
Queue rejection and shutdown cancellation both unwind the scope.

This API is for already owned handles. It does not retain handles hidden in custom
payloads. Keep those handles, callback code and callback state alive until completion.
Drain the graphics service before finalizing gamesys; finalization refuses an active
mutation rather than deleting the context used by its callback.

The engine services a pending mutation before frame admission. If completion queues
another change, it defers that simulation step instead of exiting or busy-waiting.
A mutation accepted during update skips rendering for that step and completes at
the service boundary. Shutdown also drains work accepted by deletion callbacks;
the asynchronous API rejects requests after gamesys has been finalized.

## CPU completion is not GPU completion

The native frame consumer submits before acknowledging the packet. Otherwise an
upload on a later service turn could overtake draws still waiting for submission.
There is no per-frame GPU-idle wait. CPU retirement releases factory references;
the backend must separately keep native objects and staging bytes alive for the GPU.

The existing adapters use different retirement mechanisms: Vulkan uses frame/fence
deferred destruction, Metal uses submitted-command-buffer completion, and DX12 has
per-frame deferred resources and pending buffer uploads. OpenGL relies on driver
object lifetime rules; WebGPU has native reference and command submission rules.
These mechanisms are not interchangeable, and a task completion is neither a GPU
fence nor a promise that a backend's pending upload was already submitted.

No-frame resource traffic can also leave backend retirements waiting for the next
submission or context teardown. The bounded service queue does not bound that GPU
memory. Backend-specific retirement progress and shutdown need real-GPU validation.

## Scope and remaining integration

This is the common retention/barrier portion of resource ownership, not completion
of the entire resource-migration deliverable. Production execution remains inline.

- Factory recreate callbacks still run on the game owner. Their graphics work must
  move to owner-targeted requests before a production threaded loader can use them.
- Dynamic render-script target resize/delete, asynchronous texture replacement,
  direct buffer edits and component-owned uploads still need explicit integration.
  Queue serialization alone does not protect these unguarded mutation paths.
- Render-target attachment recreation still needs transactional failure handling:
  its existing implementation can discard old attachments before all new ones exist.
  The generic guard restores admission on failure, not the contents of a resource
  whose own recreate callback failed destructively.
- Atlas/texture and material/program dependencies are retained by their owning
  resources. Mesh/pose/component snapshots must adopt this contract during migration;
  a reference does not freeze mutable component or Lua buffer memory.
- Render-state binding refresh requires a quiescent game/render boundary. It is not
  a generally thread-safe API for modifying a live render context.
- Native/web context ownership, context loss, no-frame backend retirement and GPU
  shutdown remain platform work. Null-adapter CPU tests cannot qualify those paths.

## Tests

Event-driven tests cover reader draining, admission pause/resume, preserved empty
reservations, capture conflicts, game-owner releases, parent/child resource lifetime,
reload failure, copied task payloads, backpressure, cancellation and completion
ordering. Render-script tests check retention before Lua executes, inherited use,
script failure and scratch-update ownership. Tests use the null graphics adapter;
no sleeps are used to establish producer/consumer ordering.

Local arm64 macOS validation (2026-11-13):

- 330 focused tests and four real resource reload tests passed in each build
  configuration. Native executor cases are conditional in the disabled build.
- 352 resource-loader tests passed with the capability disabled.
- Ten inline engine smoke/regression tests passed, including pending-mutation
  progress, render-hook exclusion and teardown. The enabled-build startup gate
  test also passed; production threaded startup remains deliberately unavailable.
- Enabled queue/component suites passed 100 repeated runs.

The broader gamesys suite stopped at `ResourceFolderTest.TestCreateTextureFromScript`
with empty resource paths. The same isolated failure reproduces in the untouched
2026-11-11 disabled-build binary; this separate failure was not changed here.
