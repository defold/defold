# Native video recording

`engine/record` owns native window recording, its Lua bindings and HTTP service. It has no
dependency on automation. The implementation is extracted from the recording
work on `automation-bridge-in` at `dc343a46f1`.

## Libraries and lifetime

- `record`: asynchronous capture and H.264/MP4 backends.
- `record_script`: the Lua `recorder` module and `ScriptRecord` built-in extension,
  built from `record_script_extension.cpp`.
- `record_service`: the `EngineRecord` built-in extension, HTTP handlers,
  OpenAPI metadata, recording files and downloads.

Debug desktop engines register the service through the existing extension and
web-server registries. The handler registers its own Paths Object with
`dmWebServer::AddHandler`, so `/openapi.json` discovers it automatically.
Release, headless, mobile and web engines omit all recording libraries and both
extension symbols, including the Lua API, HTTP service and native capture
dependencies. The same defaults apply to custom engines built through Extender.
Excluding recording through an app manifest or the CMake feature option also
omits all three libraries and both extensions.

`record.h` exposes capabilities, `Start`, `Stop`, `Update`, `GetStatus`, and `Finalize`.
There is one recording per process. Public calls run on the main thread; startup,
stop and native teardown run on a single persistent worker. `Start` copies its
options and snapshots the supplied native window on the main thread. A successful
start/stop return means accepted; read `m_Pending` and then inspect the result.
The host calls `Update` once per frame to poll native state, schedule automatic
stops, restore the window and notify observers. `GetStatus` only copies the latest
snapshot; HTTP requests and Lua callback dispatch do not advance capture.
Explicit start/stop commands preserve completed receipts before replacing them.
`ScriptRecord` initializes the Lua module and obtains the recording window through
the SDK. Its update callback calls `Update` before dispatching Lua callbacks, and
its `ENGINE_DELETE` event handler finalizes capture before script, sound, render
and window teardown. Linux reads the backbuffer in an SDK post-render callback,
before `Flip`, independently of HTTP. CMake and Extender register `ScriptRecord`
first: extension registration prepends callbacks, so capture runs after the other
extensions' post-render drawing. Custom hosts must preserve that registration order.
The service also handles `ENGINE_DELETE`; repeated finalization is safe.
The C++ caller owns its output path; the HTTP service owns and cleans up its
generated files.

## Lua interface

The module exposes only `recorder.start(options, callback)` and `recorder.stop()`.
Lua and HTTP share one recording: a second start fails while capture or
finalization is pending. No recording ID or capability check is required.
Desktop debug engines return an error when the operating system or capture
environment is unsupported. The `recorder` module is absent from release,
headless, mobile and web builds, and when Exclude Record is enabled in the
app manifest.

```lua
local ok, err = recorder.start({
    path = sys.get_save_file("my_game", "capture.mp4"),
    fps = 30,
    audio = false,
}, function(self, event, result)
    if event == recorder.EVENT_STARTED then
        -- Capture is active; recorder.stop() can now be called.
    elseif event == recorder.EVENT_STOPPED then
        print("MP4 ready:", result.path)
    elseif event == recorder.EVENT_ERROR then
        print("Recording failed:", result.error)
    end
end)
if not ok then print(err) end
```

`path` is required and must name a writable MP4 file in an existing directory.
It uses the host path encoding, like `sys.get_save_file` and Lua file I/O
(the Windows ANSI code page on Windows, UTF-8 elsewhere). Callback results
preserve the supplied path. Native C++ recording paths remain UTF-8.
An existing file may be overwritten. FPS defaults to 30 and application audio
defaults to false. Unsupported audio returns an error. Unknown options and
invalid argument types or values raise Lua errors.

Both functions return `true` when accepted, or `nil, error` when rejected.
Callbacks run during engine update, never inside start/stop. A successful start
produces `EVENT_STARTED`, followed by one `EVENT_STOPPED` or `EVENT_ERROR`;
startup failure produces only `EVENT_ERROR`. Results contain `path`, `width`,
`height`, `fps` and `audio`, plus `error` on failure. STOPPED means the MP4 is
finalized and ready to read, including after an automatic stop on resize.

The callback belongs to the initiating script. Script deletion suppresses the
callback without stopping capture; engine shutdown finalizes without invoking
Lua callbacks. The caller owns its output file, which survives service cleanup
and engine shutdown. Completion observers retain the previous session's result
before another Lua/HTTP caller can replace it.

## HTTP interface

All routes use the engine service port and are described in `/openapi.json`.

| Method | Route | Purpose |
| --- | --- | --- |
| GET | `/recording/capabilities` | Backend, platform requirements, video/audio support and limits |
| GET | `/recording/status` | Current operation and finalized file receipt |
| POST | `/recording/start` | Start recording; returns HTTP 202 with an operation ID |
| POST | `/recording/stop` | Stop that operation; returns HTTP 202 |
| GET | `/recording/files/{id}` | Stream a finalized MP4 or a closed byte range |
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
{"fps": 30, "audio": false}
```

Recording uses the native window content size in physical pixels. There are no
width, height or scale options. At most one row and column are cropped to satisfy
the encoder's even-dimension requirement; status reports the actual video size.
Changing the window size or pixel density stops and finalizes the recording.
Capture rate is 1–60 fps, defaulting to 30. Audio defaults to backend support.
The service accepts no output path. Unknown or duplicate fields, invalid types
and malformed JSON are rejected.

Poll status until `pending` is false and `active` is true. Stop with the returned ID:

```json
{"operation_id": 1}
```

Poll until `pending` is false, `finalized` is true, and `file.state` is `complete`.
The file receipt contains its URL, byte size and SHA-256. GET that URL without
`Range` to stream the entire MP4 with HTTP 200. For a partial download, send
`Range: bytes=START-END`, with inclusive bounds within the file, to receive HTTP
206. There is no range-size cap. Both responses use `video/mp4` and `Content-Length`
for the number of bytes being downloaded.

The engine update accepts the request, then a worker reads and sends the body
through a 64 KiB buffer, independently of subsequent engine updates. Prefer one
whole-file request to avoid waiting for an update for every range. Clients should
read, hash and write the response incrementally, then verify its total size and
SHA-256 against the receipt before publishing the downloaded file.

Files remain available until DELETE or engine shutdown/reboot. The service retains
at most 128 files. Hashing runs on a worker after native finalization. Failed
starts remove partial files. Shutdown joins capture and file workers before
removing owned files and unregistering the handler. Operation IDs are not reused
within a process; stale stop IDs and deleted file IDs return HTTP 410.

## Platforms and compatibility

| Runtime | Backend | Audio |
| --- | --- | --- |
| macOS 15+ | ScreenCaptureKit | Application audio |
| Windows 10 version 1903+, OpenGL, DirectX 12 or Vulkan | Windows Graphics Capture and Media Foundation | Application audio on Windows build 20348+ |
| Linux debug, OpenGL/OpenGL ES or Vulkan | Framebuffer capture, OpenH264 and minimp4 | Video only |
| Headless debug | Null backend; capabilities and errors remain discoverable | Unsupported |
| Mobile and web debug | Null backend; no recording HTTP service | Unsupported |
| Release | Recording libraries, Lua API and HTTP service omitted | Unavailable |

OS availability does not guarantee permission or device support. Capture failures
are reported asynchronously in the status receipt. ScreenCaptureKit is weakly
linked; Windows capture APIs are resolved at runtime. Recordings contain the
window's client area, excluding its title bar and borders. Windows crop geometry
is measured in physical pixels regardless of the calling thread's DPI awareness.
Windows copies the native client pixels into the encoder without resampling.
A resized capture frame is discarded before the recording is finalized.

Windows capture targets the engine's `HWND` through `CreateForWindow`, using a
separate D3D11 device for the capture frame pool. It does not read the engine's
graphics buffers. OpenGL, DirectX 12 and Vulkan all expose that same native
window through the graphics SDK, so the capture path supports all three.
Vulkan must be included and selected in the engine build. The native Windows
tests below use a GDI window; live recording with each graphics adapter still
requires an interactive Windows smoke test.

Windows application audio uses WASAPI process loopback for the engine process and
its children, with 48 kHz stereo PCM encoded to 128 kbit/s AAC in the MP4. It does
not capture other applications or the microphone. Audio support is detected
separately from video; older Windows versions continue to support `audio=false`.
Both tracks use a common QPC timeline. Silent intervals and static video are
preserved through the stop boundary, including video-only recordings. Stop drains
audio capture and writes the final held video frame before finalizing the file.
No external encoder is required.

Linux records the engine backbuffer after post-render extension callbacks, before
`Flip`. Capture uses the graphics API without depending on X11 or Wayland
desktop capture portals or window-selection prompts. It captures rendered game
content, including the debug profiler overlay, rather than the desktop or other
windows. OpenGL, OpenGL ES and Vulkan are supported; the Linux WebGPU adapter is
not advertised because its surface readback support is optional.

Linux builds OpenH264 2.6.0 from the pinned sources in `external/openh264` and
compiles the pinned `external/minimp4` header into `record`. Neither dependency is
linked into release, headless, macOS or Windows engines. `audio` is reported as
false and explicit `audio=true` requests return `audio_unsupported`.
Extender uses the same backend for custom Linux debug engines and excludes the
codec from its release and headless variants.

Frame readback is synchronous on the render thread. A separate encoder worker
converts native-size top-down BGRA pixels to BT.601 I420 and writes H.264 into MP4.
The framebuffer must be at least 16 pixels per side. Odd dimensions crop one row
or column without scaling; invalid native sizes fail asynchronously.
The encoder supports at most 36,864 16x16 macroblocks per frame, and the framebuffer
may contain at most 4096x4096 pixels in total. Window resizing stops capture before
readback and drains accepted frames into the finalized file.

The queue holds at most three frames, including the frame being encoded. Frames
are dropped when it is full, keeping memory bounded and avoiding waits for the
encoder. MP4 sample durations use monotonic capture times, so dropped frames or
pauses in rendering preserve elapsed time. The first image covers startup
latency and the last image extends to stop. Stop/shutdown drains accepted frames
and writes the MP4 index. Stopping before any frame is captured reports failure.
CPU encoding and GPU readback can affect game performance, especially at high
resolutions or frame rates.

This replaces the VP8/IVF framebuffer recorder. The old C++ `New`/`RecordFrame`/
`Delete` interface and the `start_record`/`stop_record` system messages are removed,
as is the VP8 build dependency. Consumers use the new C++ API or the independent
HTTP service. Automation clients can discover and call these routes directly.

## Validation

From the repository root with the usual Defold SDK configured:

```sh
cmake --build engine/build/arm64-macos --target run_test_record run_test_record_script run_test_record_timeline run_test_record_service
```

The lifecycle suite uses controlled completion signals to cover repeated capture,
failed startup, thread ownership, read-only status queries and shutdown during
startup/stop. The service suite uses a real HTTP server and a controlled recorder, without automation,
graphics, Lua or capture permissions. It covers discovery, schema validation,
receipts, download/checksum/delete, unsupported capabilities and reboot cleanup.
It also checks audio defaults and explicit opt-in/opt-out, stale identities
across real process replacement, reused operation counters, and early shutdown
through the registered extension event callback. The timeline suite
covers leading silence, gaps, overlapping packets, clipping at stop and rounding
over long recordings.

The Lua suite uses the registered extension, real script contexts and the
asynchronous recorder with a controlled backend. It checks update and callback
ordering without HTTP, validation, busy/unsupported requests, start/stop/capture
failures, resize, deletion, early shutdown, reboot, initialization failure and
callbacks that stop or restart recording. On Linux it also verifies capture after
other post-render callbacks. The service suite also verifies that a direct
caller can replace an HTTP recording without losing its completed file or
transferring ownership of the caller's output to the service.

On Linux, the normal test suite also runs `test_record_linux`. It uses controlled
framebuffer pixels with the real OpenH264 encoder, MP4 muxer/demuxer and H.264
decoder. Tests verify colors, orientation, finalization on resize, video-only tracks,
duration, odd native sizes, shutdown/restart, unsupported audio, empty recordings,
readback limits and failed starts. The timeline suite also checks capture
deadline rounding and drift.
It does not require a display server. Run the focused targets with:

```sh
cmake --build engine/build/arm64-linux --target run_test_record_linux run_test_record run_test_record_service
```

Set `DEFOLD_RECORD_TEST_OUTPUT` to a writable path when running the Linux test
binary to retain a sample MP4 for independent player or FFmpeg validation.

On an interactive Windows desktop with an audio output device, run:

```sh
cmake --build engine/build/x86_64-win32 --target run_test_record_win32_native
```

This opt-in suite opens its own window and plays test tones. It decodes the actual
MP4 tracks to verify H.264/AAC, tone/flash synchronization, silent audio, recording
without a render stream, repeated recording, shutdown finalization, failed-start
cleanup, static/paused video duration and `audio=false`. Colored client corners
verify cropping for DPI-unaware, system-aware and per-monitor-aware windows,
and finalization after a window resize; run above 100% display scaling to exercise
DPI virtualization. The launcher plays a competing tone in another process
so the silence cases also check application isolation. Completion uses playback
and capture events; the native tests are excluded from the default CI test suite.

Validated on arm64 macOS: recording, Lua, timeline and HTTP service suites;
debug, headless and release engine builds; live debug discovery; native
320×240 H.264 MP4 capture,
ranged download, SHA-256, `ffprobe`, and normal engine exit with application audio
recording active. Binary inspection confirmed that release and headless omit
all recording libraries and extensions, and all three variants omit automation
and VP8.

Validated on x86_64 Windows 11 build 26200: recording, timeline and HTTP
service suites, and the native recording suite using 320×240 H.264 video and
48 kHz stereo AAC. Native decoding verifies synchronization, stream contents and
client-area cropping at 150% display scaling across all three DPI awareness modes.
The Windows sources also build with MSVC and the Windows SDK 10.0.20348.0.

Validated in Ubuntu 24.04 containers on arm64 and x86_64: recording,
timeline and HTTP service tests, with NEON and x86 assembly respectively.
FFmpeg independently decodes the generated MP4s; ffprobe confirms H.264 I420
video with no audio track. macOS recording, timeline and service regression
suites also pass. These Linux tests supply framebuffer pixels through a test
adapter; live OpenGL/OpenGL ES/Vulkan window capture and desktop performance
still require an interactive Linux smoke test.

A minimal Clang 18 `-O2` executable linking the encoder and minimp4 (including
its demuxer) is approximately 579 KiB stripped on arm64 and 687 KiB on x86_64.
This is a dependency footprint check, not a measurement of the full engine's
size increase. The source dependency adds approximately 6.4 MiB to the checkout.
