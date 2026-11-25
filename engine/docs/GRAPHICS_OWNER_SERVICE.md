# Graphics-owner service

The renderer now owns a queue for graphics work that does not belong to a draw
frame: creating a texture, uploading a buffer, querying a render target, or
cleaning up a graphics resource. It can make progress during loading and when
rendering is paused.

Production execution is still inline. The service supports a separate render
owner in tests, but does not create a thread or move a graphics context itself.

Drawing pause does not stop the service. Context loss cancels queued requests
with `GRAPHICS_SERVICE_CONTEXT_LOST` and rejects new requests with that result.
Callbacks are still delivered on the game owner, so a synchronous resource load
can return an error instead of blocking the browser's restoration event. Resource
loaders must handle both admission and completion errors, not retry them as pending.
Restoration accepts fresh requests while drawing remains paused. Recreate lost
resources before submitting work against them; old queued operations are not replayed.
Both the loading hook and frame-queue pump use the same availability check.

## Request flow

```text
Game owner (or a worker in threaded mode)
    EnqueueGraphicsTask: copy the request into service-owned memory
        |
        v
Render owner
    PumpGraphicsService: execute a bounded number of requests
        |
        v
Game owner
    DispatchGraphicsCompletions: call back, then release request memory
```

Enqueue never executes the request, even inline. Completion means the graphics
API call returned, not that the GPU finished using the resource. An execute
callback must copy or stage data before returning; it cannot leave asynchronous
work pointing into the request's payload.

Request slots and bytes remain charged until completion delivery. A full queue
returns `GRAPHICS_SERVICE_WOULD_BLOCK`; retry after progress rather than bypassing
the queue with a direct graphics call. The engine currently reserves 256 slots
and allows 8 MiB of outstanding payloads. Individual payloads are allocated only
after admission. These are internal service limits, not project settings.

The render-owner pump never waits for work or a queue lock. A native executor can
use the wait functions between pumps. A browser-main executor must schedule a
later pump through the wake callback instead of blocking.

## Engine integration

1. `NewRenderContext` creates core renderer state and the service before resource
   loading. The context is registered alongside the other engine contexts, so
   native extension initialization can find it.
2. Gamesys connects the resource factory's generic loading-progress hook to
   `UpdateGraphicsService`. This lets synchronous loading finish graphics requests
   before the engine frame loop exists. A hook called on a loader worker does
   not execute graphics calls or game callbacks.
3. `InitializeRenderer` adds Lua bindings, text and debug rendering once the
   script context exists. Earlier native registrations and queued messages stay
   intact. Drawing and render-script APIs require this initialization.
4. Frame-queue service points pump render-owner requests. The end of an engine
   step services requests and delivers callbacks even if no frame was drawn.
   Completion callbacks must not start synchronous resource loads or wait for
   another completion.
5. Shutdown notifies producers with `STATE_DELETE_BEGIN` before draining requests.
   Producers stop recurring work but keep callback data alive until it completes.
   Shutdown calls `DrainGraphicsService` while callback data is alive.
   `FinalizeRenderer` releases drawing/Lua state, but keeps the service alive for
   later resource cleanup. `DeleteRenderContext` finally stops admission and
   deletes the service before the graphics context is destroyed. Final deletion
   closes admission before draining, so callbacks cannot extend that drain forever.

`StopGraphicsService` preserves accepted work. Explicit cancellation is separate:
it cancels queued requests and still delivers one completion per request. Active
execution finishes normally. If graphics execution is suspended during shutdown,
the engine cancels queued work instead of issuing graphics calls.

The renderer does not depend on the resource loader or gamesys. Gamesys connects
them; the engine controls their startup and shutdown order. Standalone render
contexts can borrow a service, whose caller then owns draining and deletion.

## Initial operations and follow-up work

`render_graphics_resource.h` provides internal helpers for ordinary 2D textures,
vertex/index buffers and render targets. Upload helpers copy input bytes. Texture
uploads currently cover byte L/LA/RGB/RGBA data, not compressed, array, cube or
volume textures. Existing Lua and dmSDK resource APIs are unchanged.

This service routes work to the correct owner; it does **not** keep resource
handles alive or order mutations against outstanding frames. The resource-lifetime
follow-up will add that protection before existing resource paths migrate.

The same queue can later underpin native-extension graphics tasks. An extension
API will also need shutdown rules that keep extension code and callback state
alive until requests finish. Per-frame drawing hooks remain a separate API.

A production threaded executor must establish graphics-context ownership and
keep pumping through cleanup, then join or quiesce before service deletion.
Browser and real-GPU execution still need platform validation.

The Linux x86-64 nightly also runs the focused foundation tests with
`WITH_THREADED_RENDERING=ON` under ThreadSanitizer. Graphics-service tests that
require threading report a skip when the build does not support it.
