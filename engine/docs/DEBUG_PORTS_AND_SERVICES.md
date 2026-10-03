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
currently registered engine and native-extension HTTP handlers, including their
methods, parameters, request bodies and responses. The document uses a relative
server URL, so it also works with a dynamically assigned service port.

The endpoint is available wherever the debug engine service is available. Other
methods return `405` with `Allow: GET`. The document is assembled on request, so
adding or removing a handler changes the next response. The profiler UI remains
available at `/`.

#### Native-extension handlers

`dmWebServer::AddHandler` requires an OpenAPI JSON string as its fourth argument.
The string contains a non-empty OpenAPI **Paths Object**, including the endpoint
paths and their methods. One routing prefix can describe several endpoint paths.
For example, a handler registered at `/example` can describe both `/example` and
`/example/{id}`.

```cpp
static const char EXAMPLE_OPENAPI[] =
    "{\"/example\":{\"get\":{"
    "\"summary\":\"Get extension status\","
    "\"responses\":{\"200\":{\"description\":\"Extension status\","
    "\"content\":{\"text/plain\":{\"schema\":{\"type\":\"string\"}}}}}}}}";

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

## Redirect service
The redirect service will redirect any request to the engine service on its actual port (see Engine Service)


## Log service
The log service can be used to read logs from the engine using a TCP socket.


## Remotery
Remotery, the high performance profiler used in Defold, serves data on port 17815 using a Web Socket connection.
