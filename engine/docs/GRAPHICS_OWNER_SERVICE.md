# Graphics work outside render frames

The graphics service lets game code and workers request work on the render owner
without putting it in a frame packet. Examples are uploading a buffer during
loading or creating a render target before the first frame. It is an internal
engine API, not the extension API or a replacement for `dmGraphics`.

Production still runs inline. Native tests exercise a separate render owner; the
service also has a nonblocking pump for an external owner such as browser main.
It does not create a thread, transfer a graphics context or enable threaded engine
startup. Those require platform/backend integration.

## Request flow

`enqueue and copy → render-owner pump → game-owner completion → release payload`

1. `EnqueueGraphicsTask` copies the request into service-owned memory. Upload
   helpers copy the bytes as well; copying a pointer would not be sufficient.
2. `PumpGraphicsService` executes a bounded number of accepted requests on the
   render owner, outside queue locks. That owner must already be allowed to use
   the backend context. A busy queue lock returns `BUSY` rather than blocking it.
3. `DispatchGraphicsCompletions` delivers results at an explicit game-owner
   boundary. It releases each request after its callback returns. Work generated
   by a callback is completed at a later boundary, not recursively.

Inline uses exactly this contract on one thread, without service mutexes or a
service thread. Enqueue does not execute work or call its completion immediately.
An accepted request completes once, including execution failure or explicit
shutdown cancellation; its callback runs once if supplied. A rejected request has
no callback or side effects.

A successful execution result means the adapter call returned, **not that the GPU finished**.
Some adapters defer submission itself until a later graphics frame; for example,
DX12 buffers created before `BeginFrame` first copy into backend-owned pending data.
Backend work that keeps upload bytes after returning must own a separate copy or
transfer; it cannot borrow the request payload or a reusable frame arena.

## Bounds and backpressure

The engine currently allows 256 outstanding requests and 8 MiB of request payloads.
The service preallocates request slots and allocates aligned payloads only after
admission. Both limits include executed requests whose completions have not yet
been delivered. This bounds the queue even when the game owner falls behind.

`WOULD_BLOCK` means retry after progress; an individual payload exceeding the total
byte budget is invalid. Rejection leaves ownership with the caller. Do not fall
back to graphics calls on the submitting worker. The payload limits are internal
starting values, not a promise that every resource can be uploaded in one request.

Native executors can sleep on service progress. Browser main must instead return
to its event loop and schedule another pump. The optional wake callback schedules
that future pump; it must not execute it synchronously.

## Where the engine services work

- **Startup:** the service establishes execution ownership after graphics-context
  creation, before extensions, renderer initialization and bootstrap resources.
  The render context borrows its execution context.
- **Loading:** a generic resource-factory service callback runs outside the loader
  lock. This lets synchronous `Get`/`CreateResource` deliver graphics completions
  even before there is an engine frame loop. The hook checks the calling owner;
  completions must not recursively perform synchronous resource loads.
- **Frames:** the component frame executor pumps graphics work alongside deferred
  renderer-state cleanup. Requests are independent of frame slots.
- **No frames:** the engine also services requests and completions after updates,
  including updates with zero simulation/render steps and paused rendering.
  OS suspension is different: backend execution waits until graphics is usable.
- **Shutdown:** accepted work is drained while callback owners still exist and
  around bootstrap/resource teardown. The service outlives the render context and
  frame queue, but is deleted before the graphics context.

`StopGraphicsService` closes admission without discarding accepted work. Continue
pumping and delivering completions, then join/quiesce the executor before deletion.
Cleanup operations must be submitted before closing admission. If shutdown cannot
use graphics, `CancelGraphicsService` cancels queued work and delivers cancellation
on the game owner. It never interrupts a task already executing; that task still
needs to finish and be acknowledged. Cancellation may overtake that active task.

Callers must keep completion contexts and callback code alive through delivery.
In the future threaded engine, the graphics executor must remain alive after the
frame executor stops, through final resource cleanup. A frame-queue drain alone
is not a graphics-service shutdown.

## Resource helpers and immediate-return APIs

`render_graphics_resource.h` provides owned requests for:

- Ordinary 2D texture creation, upload, metadata query and deletion.
- Vertex/index buffer creation, replacement and deletion.
- Render-target creation, resize, attachment-size query and deletion.

Created handles and queried values arrive in completion callbacks. Queries report
the value when the request executed, not necessarily the value when the callback
runs. No synchronous return value or new cache is silently substituted for them.
Existing immediate-return `dmGraphics` callers and resource loaders are unchanged;
they need explicit migration to pending/completion state before using this path.

Initial texture uploads support byte LUMINANCE, LUMINANCE_ALPHA, RGB and RGBA data
for a single 2D mip/subregion. Arrays, cube maps, volumes and compressed uploads
need explicit complete-payload sizing. The helper uses `SetTexture`, which consumes
or stages its input before returning, not `SetTextureAsync`, which can borrow data
for a worker. Render-target creation does not accept attachment data pointers.

Shader creation, readback and mapped buffers are not included in these helpers.
They need backend and data-lifetime contracts. In particular, wrapping the existing
blocking WebGPU readback call in a task would still block browser main. Surface
queries, resize/context-loss routing and real native/web context handoff remain
backend-specific follow-ups. No graphics API is made generally thread-safe here.

## Relationship to frame-resource ownership

This service answers **where** an operation runs, not **which frames may observe
it**. Requests are ordered within the service but not against outstanding frames.
Running an N+1 upload before frame N could otherwise change N's output even though
every graphics call used the correct thread.

Gamesys now provides `EnqueueRenderResourceMutation` for replacements of already
owned handles. It pauses frame admission, drains captured readers, then submits
an owned task through this service. Admission stays paused until the game-owner
completion returns. Only one such asynchronous mutation may be pending; direct
factory reloads are rejected during that interval. Rejected requests roll back
the pause and have no completion. Callers still own handle and callback lifetime.

Ordinary `EnqueueGraphicsTask` and the resource helpers do **not** implicitly enter
this scope. Use them for independent creation or when the caller already orders
resource use. See [Resource lifetime](RENDER_RESOURCE_LIFETIME.md) for capture
retention, synchronous reload guards and the remaining backend integration.

## Validation

The service tests cover copied inputs, request/byte saturation, failed execution,
deferred callbacks, re-entry, no-frame pumping, worker submission, stop/cancel and
native-owner shutdown. Synchronous resource tests request graphics work from
`Create` and complete `PostCreate` through the factory hook without rendering.

Resource helpers are exercised with the null adapter. An engine test verifies
progress with rendering paused and zero frame steps, then completion during
shutdown. This is CPU ownership/routing coverage, not real-GPU, browser or backend
handoff qualification.

Local arm64 macOS validation (2026-11-12): 333 focused tests passed in each build
configuration, 420 resource/loader tests passed with the capability disabled, and
the enabled service suite passed 100 repeated runs. Seven inline engine smoke
tests and the enabled-build startup rejection/cleanup test also passed. Tests
requiring threaded execution are conditional in the disabled build.
