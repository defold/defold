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
* `/post/@render/resize` - Resize the engine window. `com.dynamo.render.proto.Render$Resize`
* `/post/@resource/reload` - Reload a resource (ie hot-reload). `com.dynamo.resource.proto.Resource$Reload`

Requests contain binary protobuf data and are limited to 1,024 bytes. Delivery
is asynchronous; the HTTP response does not confirm execution. OpenAPI documents
the binary payloads and links to their protobuf definitions. It does not add JSON
request support.

### /ping
This endpoint will accept a GET request and reply with a "pong". This can be used to check that the server is running and responding to requests.

### /info
This endpoint will accept a GET request and reply with a JSON formatted string containing information about the engine:

```json
{"version": "1.4.1", "platform": "x86_64-macos", "sha1": "8f96e450ddfb006a99aa134fdd373cace3760571", "log_port": "7001"}
```

### /state

Returns JSON with a boolean `connection_mode` indicating whether the engine is
waiting for a project connection.

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
