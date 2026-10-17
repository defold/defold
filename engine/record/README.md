# Native video recording

`engine/record` owns native window recording and its HTTP service. It has no
dependency on automation. The implementation is extracted from the recording
work on `automation-bridge-in` at `dc343a46f1`.

## Libraries and lifetime

- `record`: asynchronous capture and native H.264/MP4 backends.
- `record_null`: the same C++ interface with recording reported as unsupported.
- `record_service`: the `EngineRecord` built-in extension, HTTP handlers,
  OpenAPI metadata, recording files and downloads. It works with either backend.

Debug desktop engines register the service through the existing extension and
web-server registries. The handler registers its own Paths Object with
`dmWebServer::AddHandler`, so `/openapi.json` discovers it automatically.
Headless debug engines register the service with `record_null`. Release engines
omit `record_service` and native capture dependencies.

`record.h` exposes capabilities, `Start`, `Stop`, `GetStatus`, and `Finalize`.
There is one recording per process. Public calls run on the main thread; startup,
stop and native teardown run on a single persistent worker. `Start` copies its
options and snapshots the supplied native window on the main thread. A successful
start/stop return means accepted; poll `m_Pending` and then inspect the result.
The built-in extension handles `ENGINE_DELETE` and calls `Finalize` before sound,
render and window teardown. Later extension/app finalizers are safe to repeat.
The C++ caller owns its output path; the HTTP service owns and cleans up its
generated files.

## HTTP interface

All routes use the engine service port and are described in `/openapi.json`.

| Method | Route | Purpose |
| --- | --- | --- |
| GET | `/recording/capabilities` | Backend, platform requirements, video/audio support and limits |
| GET | `/recording/status` | Current operation and finalized file receipt |
| POST | `/recording/start` | Start recording; returns HTTP 202 with an operation ID |
| POST | `/recording/stop` | Stop that operation; returns HTTP 202 |
| GET | `/recording/files/{id}` | Download a finalized MP4 using a closed byte range |
| DELETE | `/recording/files/{id}` | Delete a retained, completed file |

First read `/recording/capabilities` and retain its `instance_id`. Send it as the
`X-Recording-Instance` header on every POST and DELETE request. Status and accepted
start/stop receipts also include this identity. It changes on engine restart or
reboot, independently of automation. A missing header returns HTTP 400
(`missing_instance`); a stale identity returns HTTP 410 (`stale_instance`) before
changing recording state. GET status/download requests can send the same header
to reject results from a replacement service. Capabilities always allows discovery
of the current identity.

Retain the identity together with the operation/file receipt; do not replace it
with a newly discovered identity when retrying an old command. Operation counters
can be reused by a replacement process.

Start accepts an optional JSON object:

```json
{"width": 1280, "height": 720, "fps": 30, "audio": false}
```

Omit both dimensions to select native content size. Explicit dimensions must both
be present and between 1 and 16384. The backend may adjust dimensions for its
encoder; the status contains the actual result. Capture rate is 1–60 fps,
defaulting to 30. Audio defaults to backend support. The service accepts no output
path. Unknown or duplicate fields, invalid types and malformed JSON are rejected.

Poll status until `pending` is false and `active` is true. Stop with the returned ID:

```json
{"operation_id": 1}
```

Poll until `pending` is false, `finalized` is true, and `file.state` is `complete`.
The file receipt contains its URL, byte size and SHA-256. Download using
`Range: bytes=START-END`, with inclusive bounds and at most 1 MiB per request;
responses use HTTP 206 and `video/mp4`. Verify the assembled file against SHA-256.

Files remain available until DELETE or engine shutdown/reboot. The service retains
at most 128 files. Hashing runs on a worker after native finalization. Failed
starts remove partial files. Shutdown joins capture and file workers before
removing owned files and unregistering the handler. Operation IDs are not reused
within a process; stale stop IDs and deleted file IDs return HTTP 410.

## Platforms and compatibility

| Runtime | Backend | Audio |
| --- | --- | --- |
| macOS 15+ | ScreenCaptureKit | Application audio |
| Windows 10 version 1903+ | Windows Graphics Capture and Media Foundation | Application audio on Windows build 20348+ |
| Linux and headless debug | Null backend; capabilities and errors remain discoverable | Unsupported |
| Release, mobile and web | No recording HTTP service | Unsupported |

OS availability does not guarantee permission or device support. Capture failures
are reported asynchronously in the status receipt. ScreenCaptureKit is weakly
linked; Windows capture APIs are resolved at runtime. Existing native capture and
window-cropping behavior is preserved from the automation implementation.

Windows application audio uses WASAPI process loopback for the engine process and
its children, with 48 kHz stereo PCM encoded to 128 kbit/s AAC in the MP4. It does
not capture other applications or the microphone. Audio support is detected
separately from video; older Windows versions continue to support `audio=false`.
Both tracks use a common QPC timeline. Silent intervals are preserved, and stop
drains audio capture before finalizing the file. No external encoder is required.

This replaces the VP8/IVF framebuffer recorder. The old C++ `New`/`RecordFrame`/
`Delete` interface and the `start_record`/`stop_record` system messages are removed,
as is the VP8 build dependency. Consumers use the new C++ API or the independent
HTTP service. Automation clients can discover and call these routes directly.

## Validation

From the repository root with the usual Defold SDK configured:

```sh
cmake --build engine/build/arm64-macos --target run_test_record run_test_record_null run_test_record_timeline run_test_record_service
```

The lifecycle suite uses controlled completion signals to cover repeated capture,
failed startup, thread ownership and shutdown during startup/stop. The service
suite uses a real HTTP server and a controlled recorder, without automation,
graphics, Lua or capture permissions. It covers discovery, schema validation,
receipts, download/checksum/delete, unsupported capabilities and reboot cleanup.
It also checks audio defaults and explicit opt-in/opt-out, stale identities
across real process replacement, reused operation counters, and early shutdown
through the registered extension event callback. The timeline suite
covers leading silence, gaps, overlapping packets, clipping at stop and rounding
over long recordings.

On an interactive Windows desktop with an audio output device, run:

```sh
cmake --build engine/build/x86_64-win32 --target run_test_record_win32_native
```

This opt-in suite opens its own window and plays test tones. It decodes the actual
MP4 tracks to verify H.264/AAC, tone/flash synchronization, silent audio, recording
without a render stream, repeated recording, shutdown finalization, failed-start
cleanup and `audio=false`. The launcher plays a competing tone in another process
so the silence cases also check application isolation. Completion uses playback
and capture events; the native tests are excluded from the default CI test suite.

Validated on arm64 macOS: recording, null, timeline and HTTP service suites;
debug, headless and release engine builds; live debug/headless discovery; native
320×240 H.264 MP4 capture,
ranged download, SHA-256, `ffprobe`, and normal engine exit with application audio
recording active. Binary inspection confirmed that release omits the service/native
backend and all three variants omit automation and VP8.

Validated on x86_64 Windows 11 build 26200: recording, null, timeline and HTTP
service suites, and the native recording suite using 320×240 H.264 video and
48 kHz stereo AAC. Native decoding verifies synchronization and stream contents.
The Windows sources also build with MSVC and the Windows SDK 10.0.20348.0.
