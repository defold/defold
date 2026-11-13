# Resource lifetime across render frames

A **frame packet** stores captured component inputs and render commands. It may be
rendered after game code has moved on. Its textures, materials and other resources
must still exist, and their contents must still match what the frame captured.

Keeping a resource alive only solves the first problem. A reload could change the
same resource before the renderer reads it. The initial solution therefore uses:

- **Retained references:** keep a resource alive while a frame needs it.
- **A mutation barrier:** temporarily stop new frames and finish existing readers
  before changing a resource. A mutation is a change to existing resource data.

It does not yet keep separate resource versions for different frames. Production
still runs inline, with game and rendering work on the same thread. Below, **game
owner** means the thread running game code, and **render owner** means the thread
consuming captured frames and issuing graphics calls.

## Keeping resources alive

`RetainComponentRenderResource(factory, frame, resource)` gives a frame being built
an extra reference to a loaded resource. The resource factory, which manages loaded
resources, must outlive that frame.

If capture fails, the extra reference is released. Cancellation and normal frame
retirement also release it on the game owner. **CPU retirement** means the renderer
has finished reading the packet, its references can be released and its CPU storage
can be reused. It does not mean the GPU has finished.

Retain the resource that owns the required arrays and dependencies, not a pointer
to an element inside it. Repeated calls are allowed, but each uses another slot in
the frame's reference list.

A resource-backed render script keeps its whole **render prototype** alive before
recording Lua commands into a frame packet. The prototype owns the script and its
declared materials, compute programs and render targets, which in turn keep their
dependencies alive. For example, keeping a target alive also keeps its attachment
textures alive.

This covers declared resources still bound from previous frames, even when the
script does not bind them again. The script's own temporary command-storage path
does not take this extra reference: that would make the prototype keep itself alive.

The frame keeps the whole prototype, rather than tracking a separate reference for
each draw. A resource handle listed in a recorded command does not automatically
keep that resource alive. Migrated components must retain the resources their own
captured inputs need.

## Replacing resources and refreshing bindings

A successful dependency reload updates the script's named resources and matching
material, compute or attachment-texture bindings left over from earlier frames.
Replacing the whole prototype clears those old bindings before installing the new
resource list. New material allocation and prototype dependency loading happen
before discarding the previous state. Failure at those stages leaves the previous
resource usable.

Render-target reload first creates the replacement target and obtains all its
attachment wrappers before changing any handles visible to callers. A **wrapper**
is a resource object that refers to an attachment texture. Existing wrappers keep
their identity, so a material referencing one sees the replacement texture.

If preparing the replacement fails, only the replacement is released. Removing an
attachment or destroying its target clears any wrapper still held elsewhere
(`m_Texture = 0`), rather than leaving it pointing to a deleted texture. Re-adding
the same attachment reuses its wrapper. The target stores its original path so
updates identified by a hash can rebuild attachment names without debug reverse-hash
data.

## Waiting for frames before changing a resource

`pause new frames → finish existing reads → release frame references → replace → resume`

The barrier applies when the renderer prepares lists from captured inputs, when
execution is threaded, or when any component has a registered capture callback.
Even one captured component enables it in a setup that still has legacy components.

The current legacy inline engine does not apply that barrier to ordinary synchronous
Lua resource setters. They still work immediately in render scripts, extension
render hooks and `PostUpdate` callbacks. This temporary compatibility rule preserves
existing behavior while components are migrated; it does not make setters asynchronous
or introduce a second resource implementation.

- The resource factory can call optional begin/end callbacks around replacement,
  including reload notifications and deletion of the old resource. These callbacks
  run without factory/loader locks held. Factories without callbacks are unchanged.
  Gamesys installs these callbacks and applies the rule for the active mode.
- Pausing the frame queue is temporary, unlike shutdown. Accepted frames finish
  normally; none are discarded. A native game thread can wait for progress signals
  while the render thread continues processing frames and submitting graphics work.
- A frame slot reserved before simulation can stay reserved if it is still empty.
  Once capture adds data or references, a conflicting change is rejected without
  cancelling the frame. The caller must retry later; rejection does not queue a reload.
- A synchronous reload started inside another reload shares the protected period.
  Even failed replacement must end that period and allow frames again. Callbacks
  cannot capture another frame while the barrier is active.

Inline CPU retirement happens before presentation. For captured rendering and queued
graphics changes, the engine also blocks mutation from `PreRender` through `Flip`.
Finishing CPU reads is not enough if older draw commands are still waiting to be
submitted by the graphics backend. Legacy synchronous setters keep their existing
timing; this does not make arbitrary handle changes safe or wait for the GPU.
The backend remains responsible for keeping native GPU objects alive long enough.

## Queued resource changes

`EnqueueRenderResourceMutation` keeps new frames paused while a graphics task runs
and until its game-owner completion returns. The task owns a copy of its request
data. The completion callback installs the successful replacement before frames
resume. On failure, it must keep the previous usable resource or provide a fallback.

Accepting a request does not make its new handle ready for capture. While a change
is pending, other mutations are rejected, including those requested from its own
completion callback. This protection also applies in legacy inline mode. Rejection
by the queue and cancellation during shutdown both release the barrier.

This API is for handles the caller already owns. It does not keep arbitrary handles
inside custom request data alive. Keep them, the callback code and callback data
alive until completion. See [Graphics work outside render frames](GRAPHICS_OWNER_SERVICE.md)
for the request/completion flow.

Before allowing another frame, the engine services a pending mutation. If a completion
queues another change, the engine defers that simulation step instead of exiting or
busy-waiting. A mutation accepted during update skips rendering for that step and
completes when the service runs.

Shutdown also finishes work submitted by deletion callbacks. Gamesys `Finalize`
drains its owned graphics service before closing component rendering, but keeps the
service open for later resource cleanup. Standalone callers must drain borrowed
services themselves. An unfinished mutation scope still prevents finalization,
rather than letting it delete callback data in use. New asynchronous mutation
requests are rejected after gamesys has been finalized.

## Lua resource updates

Code that changes CPU data directly can use `dmResource::BeginResourceMutation` /
`EndResourceMutation` to enter and leave the same protected period.
`resource.set_buffer` does this when copying, transferring or replacing its buffer,
using the legacy-versus-captured rule above. Rejection leaves the old buffer unchanged.
On failure, the code leaves the protected period before raising a Lua error.

This does not catch direct writes through `resource.get_buffer` / `buffer.get_stream`.
Captured mesh data must still copy those bytes or keep a separate version of them.

`resource.create_texture_async` owns a copy of the upload pixels and keeps its
placeholder resource alive until the uploaded texture is installed:

1. The upload callback marks the request ready.
2. The next script-resource update tries to install it through normal factory
   recreation, including the mutation check and reload notifications.
3. A conflict with a frame leaves the request for a later update, without busy-waiting.
4. Successful replacement invokes the Lua callback, whose result still contains only
   `path`. Permanent failure keeps the old texture, logs the error and cancels the
   request without invoking that success callback.

Finalizing the script module cancels requests not yet installed and releases their
Lua/factory state. If an upload callback arrives afterward, it only frees the
remaining request and pixel storage. Upload execution still uses the existing
graphics-adapter/job path, not the new graphics service.

## CPU completion is not GPU completion

The native test renderer submits graphics work before marking the frame packet
finished. Otherwise a later upload could run ahead of draws still waiting to be
submitted. It does not wait for the GPU to become idle every frame. CPU retirement
releases factory references; the backend must separately keep GPU objects and
temporary upload bytes alive until the GPU is done with them.

Backends handle delayed deletion differently. Vulkan uses frames/fences, Metal uses
submitted-command-buffer completion, and DX12 keeps per-frame deferred resources
and pending uploads. OpenGL relies on driver object-lifetime rules; WebGPU has
native-reference and command-submission rules. These mechanisms are not interchangeable.
A task completion is not a GPU fence and does not even guarantee that a pending
backend upload has been submitted.

If resources change while no frames are drawn, backend cleanup may wait for the
next submission or context destruction. The graphics queue's memory limits do not
limit this waiting GPU memory. Cleaning it up while idle needs a backend operation
that runs on the render owner, submits completion markers and checks them without
presenting a frame. Simply calling `Flip` from the service is not enough: a backend
may require an active frame or drawable.

Metal delivers accepted upload callbacks at context shutdown and releases native
objects waiting for deletion. Other adapters still need the same callback-delivery
guarantee. Dropping an upload callback during teardown leaves a cancelled request's
pixel storage allocated; cancelling the Lua request alone cannot fix that.

## What still needs to be done

This implements shared resource references and the initial replacement barrier,
not the complete resource migration. Production execution remains inline.

- **Move resource graphics calls to their owner.** Factory recreate callbacks still
  run on the game owner. Their graphics calls, async texture operations and component
  uploads need routing before production threaded loading can use them. Guarding
  async texture replacement and `resource.set_buffer` does not complete that move.
- **Support immediate setters while recording.** Before production components enable
  the barrier, synchronous setters must also work during frame recording. Older
  frames can finish, but a frame being built cannot wait for itself. Resource versions
  or correctly ordered operations must protect its inputs without making Lua setters
  silently take effect later.
- **Handle dynamic render targets.** A resize between draws must execute between
  those draws, while game-side queries still need the requested size before rendering
  catches up. Deletion must still happen if the frame carrying it is cancelled after
  Lua has forgotten the target. Recording resize/delete commands alone is not enough.
- **Adopt references and snapshots in components.** Atlas and material resources keep
  their texture/program dependencies alive. Mesh, pose and other component captures
  must retain what they need too. References do not freeze mutable component or Lua
  buffer memory; those bytes need copies or versions.
- **Refresh bindings only when readers have stopped.** Updating render-state bindings
  is not a generally thread-safe way to modify a render context in use.
- **Finish backend/platform integration.** Native/web context ownership, context loss,
  cleanup without frames and GPU shutdown need backend work and real-platform tests.
  Tests with the null graphics backend cannot prove those paths are safe.

## Tests

Tests use explicit thread signals to check that readers finish, new frames pause
and resume, empty frame reservations survive, and capture conflicts are handled.
They also check game-owner reference release, dependent-resource lifetime, failed
reloads, copied request data, full queues, cancellation and completion order.
Render-script tests check references before Lua runs, bindings kept from earlier
frames, script errors and temporary command-storage lifetime. These tests use the
null graphics backend; they do not use sleeps to arrange thread ordering.

Additional tests check that failed target replacement preserves shared attachment
wrappers, rejected buffer updates leave data unchanged, and error paths release
the mutation barrier. Texture tests cover copied pixels, delayed replacement,
resource release and script teardown while an upload is outstanding.

`test_graphics_metal_lifetime` is a separate, opt-in real-Metal test target. It checks
upload readback, pending-upload callbacks and native-object deletion without
`BeginFrame` or `Flip`. It uses worker signals and observes native-object destruction,
not timing sleeps.

Local arm64 macOS validation (2026-11-13):

- 333 focused resource/render tests and 19 gamesys integration tests passed with
  threaded-rendering support both enabled and disabled. Native-thread cases are
  skipped in the disabled build.
- 352 resource-loader tests passed with threaded-rendering support disabled.
- Ten inline engine smoke/regression tests passed, including pending-mutation
  progress, rejecting queued mutations during render hooks and teardown. The
  enabled-build startup-gate test also passed: starting production threaded execution
  is still deliberately rejected.
- Three real-Metal lifetime tests passed with Metal API validation enabled, including
  actual texture readback and native deallocation after no-frame shutdown.
- The earlier shared-barrier change also passed 100 repeated queue/component suite
  runs with threaded-rendering support enabled.

The broader gamesys suite is not fully green: `ResourceFolderTest.TestCreateTextureFromScript`
and `ResourceFolderTest.TestResourceScriptBuffer` fail with empty resource paths.
Both reproduce in the saved 2026-11-11 test binary; these separate fixture failures
were not changed here. The coroutine upload regression and the new controlled-data
mutation tests pass.
