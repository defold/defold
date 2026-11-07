# Engine services and ports

It is possible to connect to and interact with a debug version of the engine through a number of different open TCP ports and services. The following services and ports are typically available in a debug build of the engine:

* mDNS/DNS-SD - UDP Port 5353.
* Engine service - Port 8001 or the port specified in `DM_SERVICE_PORT` environment variable. When running from the editor `DM_SERVICE_PORT` is set to “dynamic” which means that the engine will let the OS assign a random available port.
* Redirect service - Port 8002. 
* Log service - Port assigned by OS. 
* Remotery - Port 17815.


## mDNS / DNS-SD
mDNS/DNS-SD is used by the running engine to broadcast its existence on the network so that the editor can discover it and connect to issue commands.

The service type is `_defold._tcp.local` and discovery metadata is provided through TXT records. The editor uses this metadata directly to build targets.
The advertised instance name is a protocol identifier, not display text: it is emitted as `defold[-<sanitized-address>][-<port>][-<startup-suffix>]`, must fit within one 63-byte DNS label, and may omit the address segment when keeping the port and suffix makes the label more useful. The editor-facing display name and stable target identity come from the TXT `name` and `id` entries instead of the wire instance label.

<details><summary>Example of discovery using `dns-sd` (macOS)</summary><p>

```bash
dns-sd -B _defold._tcp local
```
 
</p></details>

## Engine service
The engine service is implemented as a small web server running within the engine. The server provides a number of endpoints which can be used to query for data or issue commands:

The observation URLs are unversioned. `/ping`, `/info`, `/state`, and
`/openapi.json` accept only `GET` at the exact path, without a request body.
Query strings are accepted and ignored by these handlers.
Unsupported methods return `405` with `Allow: GET`; unknown
paths return `404`. Unexpected bodies return `400` and close the connection
without waiting for the upload. Invalid or unsupported body framing, including
chunked uploads, is rejected by the HTTP server. Observation errors use
`{"error":{"code":"...","message":"..."}}`; routing and framing errors can be plain text.

Header values share a 1024-byte storage budget, including one terminating byte
per value. Requests that exceed this budget return `431 headers_too_large` and
close the connection without waiting for the body. Rejection happens before
instance validation or handler dispatch, so oversized headers cannot bypass an
instance precondition.

Handlers run on the engine thread and response bytes are copied into an
asynchronous sender. Slow readers do not retain engine contexts. Observation
requests continue to be polled when the application loop skips simulation,
including throttling, iconification, and an iteration with zero simulation
steps. They do not advance simulation or rendering. Responsiveness follows the
application loop's pacing: OS suspension, a debugger stop, or a blocked callback
can prevent polling. Other existing service handlers can perform synchronous
work; a responsive observation does not confirm that a queued command executed.

Graphical and headless debug engines expose these endpoints where socket-server
support is available. Release engines link the null service. The current web
engine has no HTTP engine service.

### /openapi.json

`GET http://<engine-address>:<engine-service-port>/openapi.json` returns an
OpenAPI 3.0.3 document with `Content-Type: application/json`. It describes the
currently registered engine and native-extension HTTP handlers that provide
OpenAPI metadata, including their
methods, parameters, request bodies and responses. The document uses a relative
server URL, so it also works with a dynamically assigned service port.
The engine logs the full OpenAPI URL when the service starts.

The endpoint is available wherever the debug engine service is available. Other
methods return `405` with `Allow: GET`. The document is assembled on request, so
adding or removing a handler changes the next response. The profiler UI remains
available at `/`.

#### Native-extension handlers

`dmWebServer::AddHandler` accepts an OpenAPI JSON string as its fourth argument.
The original three-argument overload remains available and registers a handler
without metadata, as does passing `0` as the fourth argument. These handlers
serve requests normally but do not contribute to `/openapi.json`.

An optional fifth argument combines routing constraints:
`dmWebServer::MATCH_EXACT_PATH | dmWebServer::GET_ONLY | dmWebServer::NO_REQUEST_BODY`.
Exact-path matching ignores the query string while preserving it in
`request->m_Resource`. Method and body constraints are checked before invoking
the selected handler. Omitting the flags or passing `0` keeps prefix matching
and leaves method and body handling to the callback. OpenAPI metadata describes
the handler; it does not impose these constraints.

A non-null string contains a non-empty OpenAPI **Paths Object**, including the endpoint
paths and their methods. One routing prefix can describe several endpoint paths.
For example, a handler registered at `/example` can describe both `/example` and
`/example/{id}`.

```cpp
static const char EXAMPLE_OPENAPI[] = R"json(
{
  "/example": {
    "get": {
      "summary": "Get extension status",
      "responses": {
        "200": {
          "description": "OK",
          "content": {
            "text/plain": {
              "schema": { "type": "string" }
            }
          }
        }
      }
    }
  }
}
)json";

dmWebServer::HandlerParams handler_params;
handler_params.m_Handler = MyHandler;
handler_params.m_Userdata = context;
dmWebServer::Result result = dmWebServer::AddHandler(
    server, "/example", &handler_params, EXAMPLE_OPENAPI);
```

The server retains the string pointer without copying it. Keep the string
unchanged and valid until `RemoveHandler` or server destruction; a
`static const char[]` definition is recommended. Removal also removes the
handler's OpenAPI entries.

Registration checks JSON syntax and basic OpenAPI structure. Documented paths
must start with the registered prefix. Each path belongs to one handler, which
describes all of its methods. Duplicate paths, including templates that differ
only in parameter names, are rejected. Invalid or conflicting metadata returns
`RESULT_ERROR_INVAL` without registering the handler. A duplicate routing prefix
returns `RESULT_HANDLER_ALREADY_REGISTRED`.
Invalid metadata is logged with the handler prefix, a byte offset, and a reason.
Path conflicts identify both handlers and their original documented paths.

Keep schemas inline or reference schemas under paths in the assembled document;
the registration argument does not add top-level `components`. Metadata describes
the handler's contract; the callback remains responsible for handling methods
and validating requests. Validate the complete document with an OpenAPI validator
when adding or changing a service, for example:

```sh
curl -fsS http://127.0.0.1:8001/openapi.json | python3 -m openapi_spec_validator -
```

This command requires the `openapi-spec-validator` Python package.

### /post
This endpoint will accept a POST request containing a protobuf command. One such example is the `Reload` command from `resource_ddf.proto` to reload a resource when hot-reloading content. Examples (also check `engine.clj` where some of these are used):

* `/post/@system/reboot` - Reboot the engine. `com.dynamo.system.proto.System$Reboot`
* `/post/@system/exit` - Exit the engine. `com.dynamo.system.proto.System$Exit`
* `/post/@system/run_script` - Run a Lua script. `com.dynamo.engine.proto.Engine$RunScript`
* `/post/@resource/reload` - Reload a resource (ie hot-reload). `com.dynamo.resource.proto.Resource$Reload`

Requests contain binary protobuf data and are limited to 1,024 bytes. Delivery
is asynchronous; the HTTP response does not confirm execution. OpenAPI documents
the binary payloads and links to their protobuf definitions. It does not add JSON
request support.

### /ping
Returns `PONG\n` as `text/plain` to check that the engine is responding.

### /info
This endpoint will accept a GET request and reply with a JSON formatted string containing information about the engine:

```json
{"version": "1.14.2", "platform": "arm64-macos", "sha1": "<build-sha>", "log_port": "7001", "service_api_version": 2}
```

The build values are illustrative. The existing four fields remain strings.
`service_api_version` is an integer identifying the common observation contract;
version 1 introduced the state fields below. Version 2 adds `/window/resize`
receipts. Compatible additions retain the version. Its absence means the engine does not expose this contract. Engine
release versions and OpenAPI's `info.version` identify the build separately.
Both `/info` and `/state` return `application/json` with `Cache-Control: no-store`.

### /state

Returns one consistent observation, with HTTP `200` even when not ready:

```json
{"connection_mode": false, "engine_instance_id": "<opaque-token>", "ready": true, "frame": 42}
```

* `connection_mode` retains its boolean meaning: the engine is running the built-in connection app.
* `engine_instance_id` identifies the current initialization attempt. It changes on restart or reboot, including rebooting the same project, and is retained by hot reload. It is `null` without an attached engine. Treat it as an opaque correlation token, not authentication or a project identifier.
* `ready` is true only after engine initialization and bootstrap collection initialization succeed for a real project. It is false during initialization, in connection mode, after failure, and after teardown. It does not promise completion of application loading, collection proxy loading, or frame presentation. Bootstrap failure retains the existing engine startup policy.
* `frame` is the authoritative unsigned 32-bit engine frame counter, sampled immediately before service polling. It starts at zero and is `null` without an attached engine. The normal poll excludes the frame still being processed: the counter increments at the end of the frame, after `Flip()` when rendering is enabled. Complete render-disabled iterations count; skipped simulation iterations do not. This is not a GPU-completion or scene-snapshot sequence.

Discover the existing service port, check `/info` and `/openapi.json`, then poll
`/state` with a client-side monotonic timeout. Reads do not wait for a condition
and accept no wait/timeout parameter. On an instance change, discard old
observations and reacquire subsystem state. Detect frame progress by a changed
value or unsigned modular comparison: the counter wraps at 32 bits without
changing the instance identity. Already queued responses keep the identity and
state sampled before teardown. Intermediate startup/teardown states need not be
observable; the service provides no event history.

### Window metrics and resizing

The debug-only `platform_service` library registers the `EnginePlatform` native
extension. It borrows the engine service's HTTP server, registers routes in
`AppInitialize`, and attaches the window in `Initialize`. It owns only window
metrics and resize operations. Its internal typed observation API exposes the
same geometry revision as HTTP. Engine identity, readiness, and frame count
remain in `/state`.

The extension advances resizing through its ordinary `Update` callback, before a
graphics frame begins. `Finalize` detaches the window and retires receipts
before native teardown; `AppFinalize` unregisters the routes and frees the service. Reboot
registers a new service on the same HTTP server. Release engines omit the library
and extension; headless debug engines retain the observation routes.

`GET /window` reads the surface metrics on demand. All window routes return JSON with
`Cache-Control: no-store` and accept only their documented exact paths/methods.

```json
{
  "revision": "w3",
  "window": {"width": 960, "height": 640},
  "drawable": {"width": 1920, "height": 1280},
  "display_scale": 2,
  "resize": {"supported": true, "unavailable_reason": null, "max_operations": 64, "retention_ms": 60000}
}
```

The values above are illustrative. `window` is the client area in native setter
units, excluding decorations; `drawable` is the presentable image in pixels.
Unavailable dimensions are `null`, including window/drawable on headless engines.
Native mobile backends currently report drawable dimensions but no client metrics or resize support.
Desktop windowed modes support resize; fullscreen modes do not.

The service advances the geometry revision when it observes changed window/drawable
dimensions or availability, or when a window is attached or detached. Changes away
and back between observations are not tracked.
Camera, viewport, render target and image-content changes do not change it.
Coordinates have a top-left origin, X right and Y down. Pixel `(i,j)` has center
`(i+0.5,j+0.5)`. Convert client coordinates with
`(x * drawable.width / window.width, y * drawable.height / window.height)` using
one observation. Reject missing/zero dimensions; display scale is diagnostic
metadata, not the transform. These metrics do not establish a world/GUI projection
or prove that a frame has rendered at the observed dimensions.

`POST /window/resize` requires `Content-Type: application/json`. Clients that
need to pin the request to an engine instance can supply `X-Engine-Instance`
from live `/state`. The engine service checks supplied headers before routing
any request and rejects a mismatched instance with `410 stale_instance`.
Receipt polling requires the header, so clients that need completion should
acquire and retain it before submission. The platform service does not store
engine identity. The resize body accepts at most 1024 bytes:

```json
{"width": 640, "height": 360, "expected_window_revision": "w3", "timeout_ms": 30000}
```

Width and height are positive signed 32-bit integers in client units. The native
window system can constrain the requested size; receipts report the observed
result. Resizing preserves the graphics reference resolution used by
`render.get_width()` and `render.get_height()`. Optional `timeout_ms` is 1–60000, default
30000. Optional `expected_window_revision` is checked at acceptance and again
before the setter runs. Unknown/duplicate fields are rejected.

Optional `supersede` defaults to `false`. With `true`, an accepted request replaces
any pending resize, including one awaiting an OS size change. The replaced receipt
becomes `failed` with `resize_superseded` and retains its observed dimensions.
Invalid, stale-instance, or revision-conflicting requests never replace pending work.

A `202` response contains a queued receipt and a `Location` URL. To confirm
completion, poll `GET /window/resize/{id}` with the same `X-Engine-Instance` to observe
`queued -> awaiting_window -> complete | failed`. IDs are local to the service;
keep the original instance header even if the engine reboots during polling.
The setter runs once during the extension update, before a graphics frame begins.
Resizes wait while simulation is skipped, including throttled, paused, or
zero-step iterations. Deadlines continue to use elapsed monotonic time and are
checked when updates resume or a new resize is admitted, so expired work cannot
block a new selection while paused. Reads never advance a frame or dispatch a resize.

```json
{
  "operation_id": "r1",
  "state": "complete", "expires_in_ms": 59980,
  "requested": {"width": 640, "height": 360},
  "observed": {"window": {"width": 640, "height": 360}, "drawable": {"width": 1280, "height": 720}},
  "window_revision": "w4", "already_matches": false, "error": null
}
```

Completion means an actual client-size observation matched the request. It does
not promise a rendered frame. `already_matches` is true when no setter was needed.
OS clamping/refusal or a missed deadline produces `resize_not_observed` with the
last observed dimensions. Post-acceptance conflicts fail the receipt. Terminal
observations are immutable, and retained for up to 60 seconds using monotonic time;
later manual/application resizes do not rewrite them. Pending expiry is `null`.
HTTP disconnect does not cancel accepted work; retries create new operations.

One resize can be active, with at most 64 receipts in a bounded ring. A new
request evicts the oldest completed receipt when the ring is full; completed
history never blocks a resize. Expired and evicted receipts return `410`. Errors use
`{"error":{"code":"...","message":"..."}}`: `400` for invalid requests,
`409` for busy/revision conflicts, `410` for stale instances or
expired receipts, `413` for oversized bodies, and `501` for unavailable resizing.
Unknown current-instance operation IDs return `404`. Receipt reads return `200`
even when the operation failed. Unsupported methods return `405` and `Allow`.
Unexpected/oversized bodies are rejected by closing before responding; partial
valid uploads yield without admitting an operation. Release builds expose no
window service. Screenshot capture and image endpoints are a separate delivery.

**Compatibility / release note:** The debug engine now uses service API version 2.
The editor sends one width/height JSON request with `supersede: true` to
`/window/resize` for each simulator change, without an instance preflight or receipt
polling. The latest selection replaces pending work, including while paused or
awaiting a refused resize. It retains the existing request timeout and HTTP error
behavior. Acceptance does not confirm completion. The project
design resolution remains unchanged (#11025).
Presets specify window client units, so high-DPI hosts may have more drawable
pixels. Generic DDF HTTP dispatch remains unchanged, including `/post/@render/resize`.
The updated editor requires an engine that supports `/window/resize`.

### Profiler endpoints

`/` serves the profiler HTML. `/scene_graph` returns a JSON scene graph with
component-specific properties and recursive `children` arrays. `/resources_data`
and `/gameobjects_data` return binary profiler streams. Their formats and error
responses are described in `/openapi.json`.

### /profiler/gputrace

Capture complete rendered frames to a Metal `.gputrace` document without a native
extension. This requires macOS, the Metal graphics adapter, and an engine launched
with `METAL_CAPTURE_ENABLED=1`. Use the engine service port printed in the log.
Use a writable absolute output path on the engine host, with no existing trace at
that location.

```sh
ENGINE=http://127.0.0.1:8001
curl -fsS -X POST -H 'Content-Type: application/json' \
  --data '{"path":"/tmp/frame.gputrace","frames":1}' \
  "$ENGINE/profiler/gputrace"
curl -fsS "$ENGINE/profiler/gputrace"
curl -fsS -X DELETE "$ENGINE/profiler/gputrace"
```

`POST` schedules a capture and returns `pending`. `frames` defaults to 1 and
accepts integers from 1 through 10000. Query parameters are also accepted and
override JSON body fields. Use JSON for long paths, since the engine limits request
URLs to 127 bytes. Request headers and body together must be smaller than 65536
bytes, and output paths are limited to 1023 UTF-8 bytes. Incomplete uploads yield
to rendering and other service requests until the rest of the body arrives or the
connection times out. A second capture is rejected while one is pending or
active.

Capture starts before rendering the next frame and finishes after `Flip()` submits
the requested number of frames. Paused or iconified frames do not advance the
capture. `DELETE` cancels a pending capture or requests an early stop after the
current rendered frame; it does not delete the output. Shutdown and reboot also
stop captures before destroying the graphics context.

Poll `GET` until the state is `complete`, `canceled`, or `failed`. Success responses
use `{"ok":true,"data":{...}}`, with `state`, `path`, `frames`, `frames_captured`,
`stop_requested`, and an `error` string when capture startup failed. The initial
state is `idle`. Errors use
`{"ok":false,"error":{"status":400,"code":"bad_request","message":"..."}}`.
The endpoint and its OpenAPI entry are registered only on macOS builds. On macOS,
unsupported adapters or launch environments return HTTP 501.

#### Inspecting a trace with gpudebug

Open the completed trace in Xcode, or use Apple's GPU command-line tools if your
Xcode/macOS installation provides them:

```sh
xcrun --find gpudebug
gpudebug -t /tmp/frame.gputrace
```

At the `gpudebug>` prompt, use `list` and `go commands` to find command buffers,
encoders, and draw calls. Follow the names reported by `list --all`; their paths
depend on the capture. `info pipeline` inspects a selected draw's pipeline, and
`fetch color0` exports its color attachment. Use `help` or `man gpudebug` for the
commands supported by the installed tool.

For scripted analysis, `gpudebug -t /tmp/frame.gputrace -c "list"` starts a session.
Reuse its printed session id with `gpudebug -s <id> -c "<command>"`, then finish
with `gpudebug --terminate <id>`.

## Redirect service
The redirect service will redirect any request to the engine service on its actual port (see Engine Service)


## Log service
The log service can be used to read logs from the engine using a TCP socket.


## Remotery
Remotery, the high performance profiler used in Defold, serves data on port 17815 using a Web Socket connection.
