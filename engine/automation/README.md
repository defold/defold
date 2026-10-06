# Debug engine automation

Automation is an internal extension registered as `EngineAutomation` (native
namespace `dmAutomation`). Stock debug engines expose `/automation-bridge/v3`
through their existing engine HTTP service. Projects need no native extension.
Loading the legacy `automation_bridge` extension fails before app initialization
with instructions to remove it and rebuild. Release engines omit the implementation,
registration, Lua namespace, routes, and capture dependencies.

## Integration and ownership

`automation` owns HTTP, scene snapshots, input, application synchronization, and
asynchronous operation state. `automation_capture` supplies platform capture;
`automation_capture_null` supplies headless capture stubs without desktop
recording libraries. `automation_null` supplies the engine hooks for release and
unsupported platforms. All four archives are installed into the SDK. Selection
happens at link time in CMake and the Extender manifests, without `DM_RELEASE`
wrappers in the module. No code from the `dap` branch is required.

Rendering follows the same convention: `render_inspection` owns projection
history in a private context, while `render_inspection_null` supplies no-op hooks
and allocates no inspection state. The shared renderer retains only an opaque
pointer. CMake defaults renderer consumers to the null library; debug/headless
engines and inspection tests select the real library with the
`DEFOLD_RENDER_INSPECTION` target property. Extender selects the equivalent
archives through its platform library lists and release manifest.

The HTTP route belongs to the engine service. Its attached runtime changes on
reboot and receives a new identity. `X-Automation-Runtime` guards requests against
accidentally addressing a replacement runtime. Snapshot IDs, artifacts, and
element IDs include this identity. Numeric receipts use a new monotonic seed.
Clients must discard receipts and event cursors when the identity changes.

During a native Lua debugger stop, health reports `debugger_paused: true`.
Health, lifecycle, frame, input status/pending, and input cancel/flush remain
responsive. Other automation requests return `409 debugger_paused` without
traversing the suspended scene or executing application Lua. Input leases still
expire in wall-clock time and release native holds; delivery is acknowledged only
after simulation resumes. Pending frame-dependent work remains pending until
resume, subject to its existing deadline. DAP continue, stepping, and disconnect
clear the paused state. This does not add automation pause/step commands.

Synthetic input runs after HID sampling and before input binding and dispatch.
Delivery is recorded after dispatch; Lua acknowledgement is separate. Gestures
advance with engine delta time, while leases and deadlines use monotonic time.
Pending input counts as activity. Service and lease cleanup continue through
throttled or skipped rendering. The early engine-delete event releases input and
finishes capture workers before the window, rendering, HID, and Lua contexts die.
Script finalization and reload remove only that owner's callbacks and annotations.

Elements combine existing collection, game object, and GUI node handles with
resource reload revisions tracked inside automation. Existing resource reload
notifications cover prototype, component, and GUI script replacement without
adding inspection fields to game objects or GUI scenes. Unsupported custom
subelements advertise snapshot-scoped identity. Bounds use actual component
transforms and recorded render projections; unavailable or conflicting mappings
are reported explicitly instead of guessed.

The integration retains these engine hooks:

- Input and update boundaries in `engine.cpp` establish delivery and observation
  ordering. Skipped frames continue service polling and lease cleanup.
- Script reload callbacks identify the current owner before `on_reload`, so only
  that owner's Lua callbacks and annotations are removed.
- Component bounds accessors expose actual sprite, label, and GUI transforms.
  Render dispatch records the view, projection, viewport, and render target used
  by each pass. Projection history is kept in private inspection tables populated
  only when inspection is enabled, with no added fields in component instances.
- Extension and profiler accessors supply legacy-extension detection and the
  actual profiler connection port. HTTP status text covers the statuses emitted
  by the automation routes.

The route is registered once per engine service and reused across reboots, as
the profiler routes are. It needs no engine-service deletion hook. Null graphics
engines, including engine tests, link the capture stubs.

## HTTP v3

The engine's `/openapi.json` includes automation routes, typed mutation bodies,
query selectors, the runtime identity header, and artifact range downloads.
Metadata is generated from the same route and mutation-field tables used by
validation. It remains valid across runtime reboots and describes optional
backend features; consult `/health` capabilities before using those features.

Success responses have `{"ok":true,"data":...}`; errors have
`{"ok":false,"error":{"code":...,"message":...,"status":...}}` with the
matching HTTP status. There is no v2 route. GET uses query filters; mutations use
typed JSON objects with `Content-Type: application/json`. Mutation query strings,
duplicate JSON keys, unknown mutation fields, incorrect types, out-of-range
numbers, and malformed JSON are rejected. Each mutation route declares its field
schema; optional defaults apply only when a field is omitted. Command and marker
`data` remain arbitrary valid JSON.

| Methods and paths (relative to `/automation-bridge/v3`) | Purpose |
| --- | --- |
| `GET /health`, `/lifecycle`, `/frame`, `/screen` | Identity, capabilities, lifecycle, frames, geometry |
| `GET /scene`, `/elements`, `/element` | Inspection and selectors |
| `POST /observations`; `GET /observations/{id}` | Coherent immutable observation |
| `POST /coordinates/convert`; `PUT /screen` | Coordinate conversion and supported window resizing |
| `PUT /input/configure`; `GET /input/pending`, `/input/status` | Controller/device settings and receipts |
| `POST /input/click`, `/input/drag`, `/input/drag_path`, `/input/key` | FIFO synthetic input |
| `POST /input/pointer/open`, `/move`, `/hold`, `/up` | Pointer session operations (all under `/input/pointer`) |
| `POST /input/cancel`, `/input/flush` | Cancellation and release |
| `GET /events/cursor`, `/events`, `/events/wait`, `/state`, `/state/wait`, `/application/catalog` | Application synchronization; waits return immediately |
| `POST /commands`; `GET /commands?id=...`; `DELETE /commands` | Named Lua commands and receipts |
| `POST /markers` | Application/trace markers |
| `POST /screenshot`; `GET /screenshot/status` | PNG capture receipts |
| `GET /recording/capabilities`, `/recording/status`; `POST /recording/start`, `/recording/stop` | Asynchronous video operations |
| `GET`, `POST`, `DELETE /metal` | Metal trace lifecycle |
| `GET`, `DELETE /artifacts/{id}` | Bounded download and cleanup |

`health` does not traverse the scene. It reports protocol `3`, engine version and
commit, runtime identity, backend support, profiler connection metadata, and a
capability-to-version map. Check capabilities before requesting optional features.

An observation selects elements with ordinary filters or an `ids` array, and
includes published state, event cursor, screen metadata, simulation frame, and
render frame. `screenshot: true` requests an image from that exact frame; `frame`
can request a future frame. Failed or unavailable rendering produces an explicit
failure. Snapshots are built on demand and reused within a completed frame. Input
target validation cannot supply an observation's pre-update snapshot.

Pagination returns a snapshot-qualified `next_cursor`. Subsequent pages read the
same immutable data even while the scene changes. The latest 16 completed
observations/snapshots are retained; an evicted cursor returns `410 stale_snapshot`.
There can be 16 pending observations, each with a 30-second deadline. Request
bodies are limited to 64 KiB and JSON nesting to 16 levels; application JSON is
limited to 32 KiB. Existing input queue, lease, and gesture limits remain in force.

Capture files live in platform application storage. Completed capture status
returns an artifact ID, byte size, SHA-256, and preparation state. GET supports a
single explicit byte range of at most 1 MiB; clients concatenate and verify ranges
before publishing a local file. Metal directories transfer as tar archives. There
are at most 128 retained captures; DELETE frees storage. Reboot/shutdown cleans up
retained captures. Engine filesystem paths are not a transfer protocol.

Video start and stop return pending operation receipts. Poll recording status and
match `operation_id`; stop requires that ID. Platform callbacks/workers publish
completion records and never touch scene objects or Lua. Screenshot encoding uses
the bundled stb writer. PNGs and input use the same top-left coordinates, and
screenshots retain the input overlay.

## Lua and platforms

Application Lua functions are opt-in:

```ini
[automation_bridge]
application_api = 1
event_capacity = 256
```

The `automation_bridge` namespace retains `emit`, `publish`, `command`,
`acknowledge_input`, `annotate`, and `describe`. These provide events, published
state, named commands, acknowledgements, annotations, and application contracts. Event capacity is
clamped to 16–4096. Core inspection/input require no application opt-in.

macOS, Windows, Linux, Android, and iOS have the core module. Desktop headless
engines use null HID for input and omit framebuffer/window/video/Metal capabilities.
PNG capture supports OpenGL, OpenGL ES, Vulkan, and Metal adapters. Video retains
the existing macOS 15+ ScreenCaptureKit and Windows 10 1903+ Windows Graphics
Capture backends. Only macOS supports application audio. Platform requirements
stay in the capture implementation; ScreenCaptureKit is weak-linked and optional
Windows capture entry points are loaded dynamically. Metal traces retain their
macOS Metal adapter and `METAL_CAPTURE_ENABLED=1` requirements. HTML5 automation,
pause/stepping, arbitrary property mutation, and additional video backends are out
of scope.

## Validation

See [the validation record](VALIDATION.md) for completed checks and outstanding
platform acceptance gates.

Automation tests participate in the normal `build_tests` and `run_tests` targets
when `AUTOMATION_BUILD_TESTS` is enabled (it defaults to `BUILD_TESTS`). Each check
also has a focused `run_automation_*` target. To run the checks through CTest:

```sh
cmake --build engine/build/arm64-macos --target dmengine automation_tests
ctest --test-dir engine/build/arm64-macos -R '^automation_' --output-on-failure
```

Native input checks use a graphical engine and require a display. They default
to enabled on macOS; use `-DAUTOMATION_TEST_NATIVE_INPUT=ON` on other desktop
hosts with a display, or `OFF` for a headless build host. These checks verify
mouse holds and touch lifecycle through the game's input callbacks. The
headless runtime suite always runs on supported host desktop builds.

The fixtures use stock and unrelated-extension headless engines, real HTTP,
completion polling, owner deletion/recreation, reboot, malformed input, FIFO
delivery, immutable pagination, and a legacy-extension rejection binary. Release
inspection allows the null API symbols but rejects automation implementation,
projection-table implementation, route strings, and optional capture imports.
The separate Python repository supplies graphical/application/capture integration
tests and the pip package. Device, Windows/Linux runtime, graphics-backend, and
actual Extender service checks remain required platform acceptance gates; a
successful cross-compilation alone does not satisfy them.
