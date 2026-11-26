# Resource lifetime across render frames

A [frame packet](RENDERER_FRAME.md) may be rendered after game code moves on.
Its resources must still exist **and** match the captured inputs. We use:

- **Retained references:** keep a resource alive while a frame needs it.
- **A mutation barrier:** temporarily stop new frames and finish existing readers
  before changing a resource. A mutation is a change to existing resource data.

There are no per-frame resource versions yet. Production still runs inline: the
game owner (simulation) and render owner (graphics execution) are the same thread.

## Keeping resources alive

`RetainComponentRenderResource(factory, frame, resource)` adds a loaded-resource
reference to the frame. The factory must outlive it. Failed capture, cancellation
and normal retirement release references on the game owner. **CPU retirement**
allows packet reuse after the renderer finishes reading; it does not mean GPU completion.

Retain the resource that owns the required arrays and dependencies, not a pointer
to an element inside it. Repeated calls are allowed, but each uses another slot in
the frame's reference list.

A resource-backed render script retains its whole **render prototype** before Lua
recording, not once per draw. That owns the script, declared materials, compute
programs and targets, including their dependencies and bindings inherited from
earlier frames. The script's temporary command-storage path skips this reference
to avoid keeping itself alive.

Recording a handle alone does not retain it. Migrated components must retain the
resources their captured inputs need.

For render-target attachments, retain the owning target too. Keeping its texture
wrapper alive does not keep the target's native attachment alive.

## Replacing resources and refreshing bindings

A successful dependency reload updates the script's named resources and matching
material, compute or attachment-texture bindings left over from earlier frames.
Replacing the whole prototype clears those old bindings before installing the new
resource list. New material allocation and prototype dependency loading happen
before discarding the previous state. Failure at those stages leaves the previous
resource usable.

Releasing a prototype preserves inherited bindings to dependencies still owned by
another prototype or caller. Their material, compute or attachment-texture bindings
are cleared when the underlying resource is actually destroyed.

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

The caller must keep handles and callback code/data alive through completion;
copying request bytes does not retain embedded handles. See
[Graphics work outside render frames](GRAPHICS_OWNER_SERVICE.md) for queue rules.

Before allowing another frame, the engine services a pending mutation. If a completion
queues another change, the engine defers that simulation step instead of exiting or
busy-waiting. A mutation accepted during update skips rendering for that step and
completes when the service runs.

An unfinished mutation scope prevents finalization; new asynchronous mutations are
rejected afterward. See [service shutdown](GRAPHICS_OWNER_SERVICE.md#engine-integration)
for draining deletion callbacks before renderer teardown. Standalone callers must
drain borrowed services themselves.

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

The native test renderer submits graphics work before marking a packet finished,
so later uploads cannot overtake unsubmitted draws. It does not wait for GPU idle.
CPU retirement releases factory references; the backend separately keeps GPU
objects and upload bytes alive until the GPU finishes.

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

Null-backend tests use explicit signals, not sleeps, to cover:

- Reader completion, frame pause/resume, empty reservations and capture conflicts.
- Game-owner reference release, dependency lifetime, failed reloads and preserved bindings.
- Copied requests, queue limits, cancellation and completion order.
- Capture before Lua, script errors and temporary command-storage lifetime.
- Failed target/buffer replacement preserving old data and releasing the barrier.
- Texture pixel copies, delayed publication, resource release and pending-upload teardown.
- Context-loss cancellation and standalone renderers without a graphics service.

`test_graphics_metal_lifetime` is a separate, opt-in real-Metal test target. It checks
upload readback, pending-upload callbacks and native-object deletion without
`BeginFrame` or `Flip`. It uses worker signals and observes native-object destruction,
not timing sleeps.

## Relationship to the original branch

This is the resource-lifetime part of
`issue-13082-decoupled-rendering-foundation-3` (reference tip `6672023893`).
It keeps the same retention helper, queue barrier, queued-mutation wrapper,
transactional reloads and async-texture publication flow.

The newer merged code stays in place:

- Gamesys installs loading and mutation hooks during context creation, before
  extension initialization. No separate renderer-to-resource setup API is needed.
- Graphics servicing uses `UpdateGraphicsService` / `DrainGraphicsService` and
  the current context-loss gate and texture-upload validation.
- Frame bookkeeping uses `RenderFrameUsage`; tests use narrow test helpers instead
  of exposing graphics or gameobject internals.
- Metal's upload-callback shutdown fix is already on dev; this change adds its
  dedicated lifetime regression tests without replacing newer backend code.

Component migration and production threaded execution are separate follow-ups.

## Validation for this split

Local arm64 macOS validation (2026-11-26):

| Coverage | Thread support disabled | Thread support enabled |
| --- | --- | --- |
| Resource, loader, render and graphics-service suites | 695 passed; 11 native-thread cases skipped | 706 passed |
| Focused gamesys integration tests | 21 passed | 21 passed |
| Engine startup, mutation and shutdown tests | 12 passed | 2 startup/gate tests passed |

The enabled build still rejects production threaded startup; that gate is expected
until component migration is complete. The tests exercise native-thread execution
separately. The frame-queue, component-render and component-resource suites also
passed 50 repeated runs with thread support enabled.

Three real-Metal lifetime tests passed with Metal API validation enabled. Browser,
other GPU backends and ThreadSanitizer were not run locally. The nightly threaded
sanitizer target now includes the new component-resource suite.

The full gamesys suite is not green: `ResourceFolderTest.TestCreateTextureFromScript`
fails with an empty resource path, then leaves fixture state that causes later tests
to fail. The original branch documents this same fixture failure. It was not changed
here; the focused texture, target, prototype and mutation tests pass.

## Benchmark against dev

Local CPU comparison on 2026-11-26: `dev` at `a7887793e3` versus the uncommitted
resource-lifetime changes. Both builds used Apple Clang 21, `RelWithDebInfo` (`-O2`),
the null graphics backend and threaded rendering disabled. The laptop was an
Apple M1 Pro with 16 GiB RAM, running macOS 26.5.2 on AC power. No thermal warning
was reported. Builds were stopped during measurement.

The full engine test uses an empty collection and the normal `StepFrames` path,
without frame pacing. Unlike the renderer-only benchmarks, its render script is
factory-owned, so this measures the new per-frame prototype retain/release work
and the checks for pending resource changes. The same temporary standalone harness
was compiled against both revisions, without test-specific extension callbacks.

Each case has 500 warmup frames and 30 batches of 100 measured frames. Results below
are the median of ten process-level medians, from ten alternating dev/PR3 pairs.
Startup and sample printing are outside the measured interval. Lower is better.

| Engine frame workload | dev (µs/frame) | PR3 (µs/frame) | Change |
| --- | ---: | ---: | ---: |
| Default commands, no render script | 7.280 | 7.115 | -2.27% |
| Clear-only render script | 8.045 | 7.860 | -2.30% |
| 128 draws | 41.885 | 42.205 | +0.76% |
| 128 draws, 256 constant vectors per draw | 56.935 | 57.225 | +0.51% |

There is no material steady-state slowdown evident in these cases. The two
draw-heavy cases add about 0.3 µs/frame. Small gains should not be treated as an
optimization result: background applications were still present and individual
runs varied. For example, the constant-heavy process medians ranged from
55.64–62.04 µs on dev and 55.81–109.88 µs on PR3. No outliers were removed. These
measurements do not establish individual-frame tail latency.

Additional controls used five alternating pairs per benchmark:

- Renderer preparation: 1,000/10,000 items, static/moving, one/four views, 5,000
  measured frames per case. Snapshot-inline median changes range from -2.79% to
  +0.77%. Direct and captured paths produced matching draw-order checksums, visible
  counts and batch counts on both revisions.
- Render-script recording: nine clear/draw/camera/constant workloads. Median
  changes range from -4.88% to -1.28%; this harness does not retain a factory resource
  and is only a regression control.
- Frame storage: direct-arena capture changed by -0.65%, copied capture by +0.01%.
  This small synthetic test includes phase-timer overhead.

The script and renderer controls allocated no new dynamic frame-storage blocks
after warmup. Copy counts and data checks matched. Fixed engine frame storage grew
by **24 bytes per CPU slot**, from 1,062,090 to 1,062,114 bytes, for one additional
retained reference. This is not a measurement of total engine memory.

The engine harness verified all 3,500 frame/BeginFrame/Flip calls per case. During
script recording, the prototype reference count changed from 1 to 2 on PR3 and
returned to 1 after retirement; dev stayed at 1. Thus the comparison exercises the
new lifetime work rather than accidentally bypassing it.

The existing renderer benchmark needed the same automatic-adapter-selection fix
on both revisions. Two unused engine test calls on dev also needed the current
`Init` signature to compile; baseline production code was unchanged. Preliminary
engine runs that printed samples between batches were superseded by the ten-pair
results above, which print only after measurement.

This is an inline CPU overhead check, not a threaded speedup or real-game FPS
claim. It does not cover GPU work, upload-copy costs, reload throughput, waits for
outstanding readers, or many captured component resources.
