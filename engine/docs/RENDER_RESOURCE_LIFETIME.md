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

Render-target reload follows the same rule. It creates the replacement target and
acquires all attachment wrappers before changing any published handles. Existing
wrappers keep their identity, so materials holding a wrapper see its new texture.
Failed staging releases only the replacement. Removed attachments, or attachments
whose target is destroyed, leave externally retained wrappers empty (`m_Texture = 0`),
not pointing at deleted textures. Re-adding the same attachment reuses its wrapper.
The target owns its original path so hash-based updates can recreate attachment names
without depending on debug reverse-hash data.

## Replacement order

`close admission → finish readers → retire CPU references → replace → reopen`

This barrier applies to captured rendering: consumer-owned list preparation,
threaded execution, or any registered component capture callback. The current
legacy inline engine keeps synchronous Lua resource setters working in render
scripts, extension render hooks and `PostUpdate` callbacks. This is a temporary
migration policy, not a second resource implementation or an asynchronous setter.
Registering even one captured component enables the barrier in a mixed setup.

- The resource factory has optional begin/end mutation callbacks around a complete
  recreate transaction, including reload notifications and old-resource destruction.
  They run outside factory/loader locks. Existing factories without callbacks are
  unchanged; the engine connects them to gamesys's synchronous mutation policy.
- The queue's mutation barrier is reusable, unlike permanent shutdown. Accepted
  frames finish normally; none are discarded. A native game owner can wait on
  progress signals, while the render owner continues consuming and submitting.
- An empty reservation made before simulation is preserved. If capture has already
  added data or references, mutation is rejected without cancelling that frame.
  The caller must retry at a safe boundary; rejection is not a queued reload.
- Synchronous nested reloads share the scope. Failed recreation still closes its
  scope and reopens admission. Game-owner callbacks cannot capture another frame
  while the scope is active.

Inline CPU retirement occurs before presentation. For captured rendering and queued
graphics mutations, the engine also guards the interval from `PreRender` through
`Flip`. A captured frame's CPU retirement alone cannot authorize replacement while
older draws remain in the backend's unsubmitted command buffer. Legacy synchronous
setters preserve their existing timing; this does not make arbitrary handle changes
safe or provide GPU completion. Native-object retirement remains the backend's job.

For an asynchronous graphics replacement, `EnqueueRenderResourceMutation` holds
the scope through task execution and game-owner completion. The task owns a copy
of its payload. The callback publishes the successful replacement before frames
resume; on failure it must keep the previous usable resource or an explicit fallback.
No new handle is ready for capture simply because enqueue succeeded. Pending
mutations reject other mutations, including requests from their completion callback.
This holds even in legacy inline mode: the synchronous compatibility policy does not
bypass protection for queued graphics work. Queue rejection and shutdown cancellation
both unwind the scope.

Direct CPU replacements can enter the same factory scope with
`dmResource::BeginResourceMutation` / `EndResourceMutation`. `resource.set_buffer`
uses this around copying, transferring or replacing its buffer, following the same
legacy-versus-captured policy. Rejection leaves
the previous buffer unchanged; errors leave the scope before raising a Lua error.
This does not intercept writes through `resource.get_buffer` / `buffer.get_stream`:
captured mesh data must still copy or version mutable bytes.

`resource.create_texture_async` now owns a pixel snapshot and retains the placeholder
resource until publication. Upload completion marks the request ready; the next script
service turn publishes through factory recreation, including the mutation guard and
reload notifications. A frame conflict defers publication to a later service turn,
without busy-waiting. Permanent publication failure keeps the old texture, logs the
error and cancels the request without invoking the success callback. The Lua callback
result still contains only `path`. Finalizing the script module cancels
unpublished requests and releases their Lua/factory state; a later upload callback
only frees the request's pixels. Upload execution still uses the existing adapter/job
path, not the owner-targeted graphics service.

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
memory. Progress while remaining idle needs an owner-only, non-presenting backend
pump that submits retirement markers and observes completion. Calling `Flip` from a
resource service is not a substitute: adapters may require an active frame/drawable.

Metal drains accepted upload callbacks at context shutdown and releases pending native
retirements. Other adapters still need the same completion-drain guarantee. In
particular, dropping an upload callback during teardown leaves a cancelled request's
pixel storage unreclaimed; script cancellation alone cannot fix backend shutdown.

## Scope and remaining integration

This is the common retention/barrier portion of resource ownership, not completion
of the entire resource-migration deliverable. Production execution remains inline.

- Factory recreate callbacks still run on the game owner. Their graphics work must
  move to owner-targeted requests before a production threaded loader can use them.
- Before production component capture enables the barrier, synchronous setters must
  also work during frame recording. Older accepted frames can drain, but the current
  building frame cannot wait for itself. Resource versions or ordered operations must
  protect its inputs without silently deferring the setter's Lua-visible result.
- Dynamic render-script target resize/delete still need an ownership layer and
  ordered frame operations. A resize between draws must remain between those draws,
  while game-side queries need desired-size metadata even before consumption.
  Deletion also needs a persistent cleanup obligation: cancelling an unpublished
  frame cannot leak a target that Lua has already forgotten. Plain resize/delete
  commands without that ownership layer are not sufficient.
- Async texture publication and `resource.set_buffer` use the guard, but their GPU
  operations and component-owned uploads still need graphics-owner routing.
  Published factory references are not a substitute for copied/versioned CPU data.
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

Additional tests cover render-target attachment rollback and shared wrappers, direct
buffer mutation rejection/error unwinding, async texture pixel ownership, deferred
publication, resource release and script teardown with an upload outstanding.
`test_graphics_metal_lifetime` is an opt-in real-Metal target: it checks actual upload
readback, pending-upload callbacks and native-object retirement without `BeginFrame`
or `Flip`. It uses worker signals and native destruction probes, not timing sleeps.

Local arm64 macOS validation (2026-11-13):

- 333 focused resource/render tests and 19 gamesys integration tests passed in each
  build configuration. Native executor cases are conditional in the disabled build.
- 352 resource-loader tests passed with the capability disabled.
- Ten inline engine smoke/regression tests passed, including pending-mutation
  progress, render-hook exclusion and teardown. The enabled-build startup gate
  test also passed; production threaded startup remains deliberately unavailable.
- Three real-Metal lifetime tests passed with Metal API validation enabled, including
  actual texture readback and native deallocation after no-frame shutdown.
- The preceding common-barrier change also passed 100 repeated enabled
  queue/component suite runs.

The broader gamesys suite is not fully green: `ResourceFolderTest.TestCreateTextureFromScript`
and `ResourceFolderTest.TestResourceScriptBuffer` fail with empty resource paths.
Both reproduce in the saved 2026-11-11 test binary; these separate fixture failures
were not changed here. The coroutine upload regression and the new controlled-data
mutation tests pass.
