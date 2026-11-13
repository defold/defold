# Graphics work outside render frames

The graphics service is a queue for graphics work that does not belong to a
particular rendered frame. For example, the engine can request a buffer upload
during loading or create a render target before the first frame.

Normal rendering uses a **frame packet** containing captured component inputs and
render commands. Graphics-service requests are separate from those packets.

The caller submits a request, the service runs it on the thread allowed to use
graphics, and a completion callback reports the result to game code. This is an
internal engine API, not yet an extension API or a replacement for `dmGraphics`.

Two terms used below describe who may access the data:

- **Game owner:** the thread running game code and receiving completion callbacks.
- **Render owner:** the thread allowed to execute the queued graphics calls.

Production still runs **inline**, where both owners are the same thread. Native
tests use a separate render thread. The service can also be advanced without
waiting, so a future browser-main renderer can use it. The service does not itself
create a thread, transfer a graphics context or enable threaded engine startup.

## Request flow

`copy request → execute graphics work → report result → release request data`

1. `EnqueueGraphicsTask` copies the request data, also called its **payload**, into
   service-owned memory. Upload helpers copy the upload bytes too; copying only a
   pointer would leave the request dependent on the caller's memory.
2. `PumpGraphicsService` runs a limited number of accepted requests on the render
   owner. A **pump** is simply a call that advances queued work. It executes callbacks
   without holding queue locks and returns `BUSY` if it cannot acquire the queue.
   The calling thread must already be allowed to use the graphics backend.
3. `DispatchGraphicsCompletions` reports results at a defined point in the game
   owner's update/loading loop. Each request is released after its callback returns.
   Work submitted by a completion callback is reported on a later dispatch call,
   not recursively inside that callback.

Inline follows the same steps on one thread, without service mutexes or a new
thread. Enqueue never executes the request or calls its completion immediately.
Each accepted request completes once, including failed execution or cancellation
during shutdown. Its completion callback runs once if supplied. A rejected request
does not execute and has no completion callback.

Threaded mode accepts submissions from game code and workers. Inline currently
requires submission from the game owner; worker submission in both modes needs
to be resolved before exposing this as a general extension API.

A successful result means the graphics backend call returned, **not that the GPU
finished**. Some backends do not even submit the upload until a later frame. For
example, DX12 buffers created before `BeginFrame` first copy into backend-owned
pending data. If the backend needs bytes after the request returns, it must keep
its own copy or take ownership of separate storage. It cannot keep borrowing the
request payload or reusable frame memory.

## Queue limits and retrying

The engine currently allows 256 outstanding requests and 8 MiB of request data.
Request slots are allocated up front; aligned payload memory is allocated only
when a request is accepted. Both limits count requests that have executed but
have not yet delivered their completion. This keeps memory bounded even when
game code is slow to collect results.

`WOULD_BLOCK` means the caller should retry after the service has made progress.
A request larger than the entire byte budget is invalid, not something retrying
can fix. A rejected request leaves the caller's data untouched. Do not respond to
rejection by issuing graphics calls directly on the worker. These are initial
internal limits; not every resource will fit in a single upload request.

A native service thread can wait for a progress signal. Browser main must instead
return to its event loop and arrange another pump call. The optional wake callback
schedules that later call; it must not run the pump immediately.

## When the engine runs queued work

Gamesys owns the service; the queue implementation remains in `dmRender`. Gamesys
is created in two stages: `NewContext` creates the service after graphics startup,
then `ConfigureContext` attaches the factory, renderer and component render bridge
once those dependencies exist. `Initialize` registers the built-in types afterward.

- **Startup:** the early gamesys context establishes the game/render owners before
  the factory, extension initialization, renderer and startup resources. The renderer
  uses the service's execution context without owning it. Gamesys also installs
  the resource factory's service and mutation callbacks; they do not access `Engine`.
- **Loading:** the resource factory calls a service hook without holding the loader
  lock. This lets synchronous `Get`/`CreateResource` receive graphics results before
  the frame loop starts. The hook checks which thread called it. A completion
  callback must not start another synchronous resource load.
- **Frames:** the code consuming frame packets also pumps graphics requests and
  cleans up renderer state waiting for deletion. Requests do not need a frame slot.
- **No frames:** the engine services requests after updates, even with zero
  simulation/render steps or paused rendering. OS suspension is different:
  graphics execution waits until the backend can be used again.
- **Shutdown:** accepted work finishes while its callback code and data still exist.
  Gamesys `Finalize` drains requests and closes component rendering, but keeps the
  service open for resource cleanup. `DeleteContext` stops and deletes the service
  after renderer/factory teardown, before the graphics context is destroyed.

Engine code still marks application and presentation boundaries. It asks gamesys
to service updates without frames and drain before callback owners are destroyed.
`PrepareFrame` advances pending resource changes and tells the engine to defer a
simulation step when necessary. The engine no longer manages request counters or
drain loops. The graphics-frame guard still ends after `Flip`, not at the earlier
component-render cleanup point.

`StopGraphicsService` rejects new requests but keeps accepted work. Submit cleanup
requests before stopping admission, then continue executing tasks and delivering
results. Before deleting the service, join its native thread or ensure an external
pump can no longer run.

If shutdown cannot use graphics, `CancelGraphicsService` cancels queued requests
and reports cancellation on the game owner. It does not interrupt a running task;
that task must still finish and report its result. Cancellation callbacks may run
before that active task's completion.

Callers must keep callback code and data alive until completion is delivered.
In the future threaded engine, graphics tasks must still be serviced after frame
processing stops, through final resource cleanup. Finishing every frame does not
necessarily finish every graphics request.

## Supported resource operations

`render_graphics_resource.h` provides helpers that copy request data for:

- Ordinary 2D texture creation, upload, metadata query and deletion.
- Vertex/index buffer creation, replacement and deletion.
- Render-target creation, resize, attachment-size query and deletion.

Created handles and query results arrive in completion callbacks, not as immediate
return values. A query reports the value when the request ran, which may differ
from its value when the callback is delivered. Existing synchronous `dmGraphics`
calls and resource loaders have not been converted automatically. Callers must
explicitly handle pending work and completion before using these helpers.

Texture uploads currently support byte LUMINANCE, LUMINANCE_ALPHA, RGB and RGBA
data for a single 2D mip level or subregion. Arrays, cube maps, volumes and compressed
uploads need additional rules for calculating and copying all required bytes.
The helper uses `SetTexture`, which consumes or copies its input into staging
storage before returning. It does not use `SetTextureAsync`, which can keep the
input for a worker. Render-target creation does not accept attachment data pointers.

Shader creation, readback and mapped buffers are not included. They need rules for
backend execution and how long their data remains valid. Putting the existing
blocking WebGPU readback in a task, for example, would still block browser main.
Window/surface queries, resize, context loss and transferring context ownership
also need backend-specific work. This service does not make arbitrary graphics
calls safe from any thread.

## Changing a resource used by a frame

The service answers **which thread runs an operation**, not **which frame sees
the change**. Its requests are ordered with each other, but not with outstanding
frames. An upload for frame N+1 could otherwise change frame N's output, even
though both used the correct graphics thread.

Gamesys provides `EnqueueRenderResourceMutation` for changing resources the caller
already owns. It stops new frames from starting, waits for captured frames to
finish reading, then submits a copied task. New frames stay paused until the
game-owner completion returns. Only one such change may be pending; direct factory
reloads are rejected during that time. If the request is rejected, the pause is
undone and no completion runs. The caller must still keep its handles and callback
data alive.

Ordinary `EnqueueGraphicsTask` and the resource helpers do **not** pause frames.
Use them to create independent resources, or when the caller already controls
when the resource is used. See [Resource lifetime](RENDER_RESOURCE_LIFETIME.md)
for keeping resources alive, protecting reloads and the remaining backend work.

## Validation

The service tests cover copied inputs, full queues, byte limits, failed execution,
deferred callbacks, calls made from callbacks, progress without frames, worker
submission, stop/cancel and native-thread shutdown. Resource tests request graphics
work from `Create` and complete `PostCreate` through the factory hook without rendering.

Resource helpers are tested with the null graphics backend, which does not use a
real GPU. An engine test checks progress with rendering paused and zero frame
steps, then completion during shutdown. These tests check CPU data lifetime and
thread routing, not real-GPU or browser behavior or graphics-context transfer.

Local arm64 macOS validation (2026-11-12): 333 focused tests passed in each build
configuration (threaded-rendering support enabled and disabled), 420 resource/loader
tests passed with support disabled, and the enabled service suite passed 100 repeated
runs. Seven inline engine smoke tests and the enabled-build startup rejection/cleanup
test also passed. Thread-specific cases are skipped when support is disabled.
