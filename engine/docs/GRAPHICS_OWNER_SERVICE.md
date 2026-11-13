# Graphics work outside render frames

The graphics service queues work outside [rendered frames](RENDERER_FRAME.md),
such as loading a texture or creating a render target before the first frame.
It is an internal API, not yet an extension API or a replacement for `dmGraphics`.

- **Game owner:** the thread running game code and receiving completion callbacks.
- **Render owner:** the thread allowed to execute the queued graphics calls.

Production still runs **inline**: both owners are the same thread. Native tests
use a separate render thread. The non-waiting pump also supports a future browser
main-thread renderer. The service does not create a thread, transfer a graphics
context or enable threaded engine startup.

## Request flow

`copy request → execute graphics work → report result → release request data`

1. `EnqueueGraphicsTask` copies request data (**payload**) into service-owned memory.
   Upload helpers copy the bytes too, not just a pointer to the caller's storage.
2. `PumpGraphicsService` advances a limited number of requests on the render owner,
   outside queue locks. It returns `BUSY` if it cannot acquire the queue. That thread
   must already be allowed to use the graphics backend.
3. `DispatchGraphicsCompletions` delivers results on the game owner, then releases
   each request. Callback-submitted work is reported on a later dispatch, not recursively.

Inline uses these same steps without service mutexes. Enqueue never executes work
or calls completion immediately. Accepted requests complete once, including failures
and shutdown cancellation; rejected requests never execute or call completion.

Threaded mode accepts submissions from game code and workers. Inline currently
requires submission from the game owner; worker submission in both modes needs
to be resolved before exposing this as a general extension API.

A successful result means the backend call returned, **not that the GPU finished**
or even that an upload was submitted. Backends needing bytes afterward must copy
them or own separate storage; they cannot borrow request payloads or frame memory.
See [GPU completion and cleanup](RENDER_RESOURCE_LIFETIME.md#cpu-completion-is-not-gpu-completion).

## Queue limits and retrying

The initial limits are 256 outstanding requests and 8 MiB of request data, including
executed requests awaiting completion delivery. Slots are allocated up front;
aligned payload storage is allocated on acceptance. Not every upload fits these limits.

Retry `WOULD_BLOCK` after progress. A request larger than the whole byte budget is
invalid, not retryable. Rejection leaves the caller's data untouched; it must not
fall back to issuing graphics calls directly on a worker.

A native service thread can wait for a progress signal. Browser main must instead
return to its event loop and arrange another pump call. The optional wake callback
schedules that later call; it must not run the pump immediately.

## When the engine runs queued work

Gamesys owns the service; the queue implementation remains in `dmRender`. Gamesys
is created in two stages: `NewContext` creates the service after graphics startup,
then `ConfigureContext` attaches the factory, renderer and component render bridge
once those dependencies exist. `Initialize` registers the built-in types afterward.

- **Startup:** gamesys establishes owners before the factory, extensions and renderer.
  The renderer borrows the execution context. Gamesys installs factory callbacks
  without passing them an `Engine` pointer.
- **Loading:** the factory service hook checks its calling thread and runs outside
  the loader lock. Synchronous `Get`/`CreateResource` can finish before the frame loop.
  Completion callbacks must not start another synchronous resource load.
- **Frames:** consumption also pumps requests and services renderer-state deletion.
  Requests do not need a frame slot.
- **No frames:** updates service requests even with zero simulation steps or paused
  rendering. OS suspension still prevents graphics execution.
- **Shutdown:** accepted work finishes while its callback code and data still exist.
  Gamesys `Finalize` drains requests and closes component rendering, but keeps the
  service open for resource cleanup. `DeleteContext` stops and deletes the service
  after renderer/factory teardown, before the graphics context is destroyed.

Engine code marks application/presentation boundaries and asks gamesys to drain
before callback owners disappear. `PrepareFrame` advances pending mutations and
can defer simulation. The graphics-frame guard ends after `Flip`, not CPU retirement.

`StopGraphicsService` rejects new requests but keeps accepted work. Submit cleanup
first, then finish execution and completion delivery. Before deletion, join the
native executor or ensure an external pump can no longer run.

If shutdown cannot use graphics, `CancelGraphicsService` cancels queued requests
and reports cancellation on the game owner. It does not interrupt a running task;
that task must still finish and report its result. Cancellation callbacks may run
before that active task's completion.

Keep callback code/data alive through completion. In the future threaded engine,
the service must keep running through resource cleanup, even after all frames finish.

## Supported resource operations

`render_graphics_resource.h` provides helpers that copy request data for:

- Ordinary 2D texture creation, upload, metadata query and deletion.
- Vertex/index buffer creation, replacement and deletion.
- Render-target creation, resize, attachment-size query and deletion.

Handles and query results arrive in callbacks; queries reflect execution time,
not delivery time. Existing `dmGraphics` calls/loaders are not automatically routed
here. Callers must handle pending work before adopting these helpers.

Texture uploads currently support byte LUMINANCE, LUMINANCE_ALPHA, RGB and RGBA
data for a single 2D mip level or subregion. Arrays, cube maps, volumes and compressed
uploads need additional rules for calculating and copying all required bytes.
The helper uses `SetTexture`, which consumes or copies its input into staging
storage before returning. It does not use `SetTextureAsync`, which can keep the
input for a worker. Render-target creation does not accept attachment data pointers.

Shaders, readback, mapped buffers, window operations, context loss and context
transfer still need backend-specific work. Queueing a blocking readback would
still block browser main. This service does not make arbitrary graphics calls thread-safe.

## Changing a resource used by a frame

The service answers **which thread runs an operation**, not **which frame sees
the change**. Its requests are ordered with each other, but not with outstanding
frames. An upload for frame N+1 could otherwise change frame N's output, even
though both used the correct graphics thread.

Ordinary tasks/helpers do **not** pause frames. Use them for independent resources
or when the caller controls when a resource is used. For shared-resource changes,
gamesys provides `EnqueueRenderResourceMutation`; see
[Queued resource changes](RENDER_RESOURCE_LIFETIME.md#queued-resource-changes)
for its frame barrier, completion and lifetime rules.

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
