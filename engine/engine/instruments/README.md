# Apple native tracing

Native profiling is off by default in debug engines on macOS and iOS. Enable it
for a build through the existing feature option:

```sh
./scripts/build.py --platform=arm64-macos build_engine -- --enable-feature=profile_native
```

The equivalent CMake setting is `-DDEFOLD_ENABLE_FEATURES=profile_native`.
With this feature enabled, native signpost emission starts automatically on launch.

The environment variable `DM_PROFILE_NATIVE_ENABLE` overrides the build default.
Set it to `1` to enable profiling for one launch, including in an ordinary debug
build:

```sh
DM_PROFILE_NATIVE_ENABLE=1 /absolute/path/dmengine /absolute/path/game.projectc
```

Set it to `0` to disable profiling in a build with the feature enabled. When the
variable is present, only the exact value `1` enables profiling; an empty or other
value disables it. When absent, the build default applies.

`engine_service_xctrace_profiler_darwin.cpp` reads this choice during service initialization,
before configuration and resource loading. A recording request can also activate
the backend during the run. The backend runs alongside Remotery and needs no
`game.project` setting. Release engines and platforms without a native backend
remain unaffected.

Signpost emission uses APIs supplied by the operating system, so clients do not
need Xcode installed. The backend emits events; a collector saves them to disk.
It annotates `DM_PROFILE` scopes, frames, counters and profiler messages. CPU
sampling and GPU tracing are provided by the collector's instruments, independently
of these annotations.
Use Instruments for routine native captures. Its file format is a `.trace` bundle.
The engine service owns recording startup, the recorder worker, status, and shutdown.
Its early initialization hook preserves zero-delay startup capture before the web server
is created. The service registers its signpost listener through dlib's profiling callbacks.

`xctrace` is the recording tool's name. See Apple's
[command-line tool reference](https://developer.apple.com/documentation/xcode/xcode-command-line-tool-reference).

## Editor and engine recording on macOS

In **Preferences → Profile → macOS → XcTrace**, leave **Path to xctrace** empty to discover
the executable with `/usr/bin/xcrun --find xctrace`, or enter an explicit executable
path. Discovery follows the selected Xcode, including `DEVELOPER_DIR` when set.
Xcode must be installed; the standalone Command Line Tools do not contain
`xctrace`. The path is validated again when launching the game.

Set **Output folder** to an absolute filesystem path, or select it with the folder
picker. Startup and Debug-menu recordings save the trace, instrument package and
recorder log there, with a unique name for each recording. Leave the field empty
to use `~/Library/Logs/<bundle-id>/Profiling/` on the engine's Mac.

When an executable is found, the panel shows **Enable**, **Delay (ms)** (default
`0`) and **Duration (ms)** (default `5000`). Times are nonnegative integer
milliseconds; duration `0` records until stopped. Enabled launches pass these
settings through the engine environment:

| Variable | Meaning / default |
| --- | --- |
| `DM_PROFILE_NATIVE_TRACE` | `1` starts a recording; otherwise off |
| `DM_PROFILE_NATIVE_XCTRACE` | Executable override; otherwise use `xcrun --find xctrace` |
| `DM_PROFILE_NATIVE_TRACE_DELAY_MS` | Delay before starting the recorder; `0` |
| `DM_PROFILE_NATIVE_TRACE_DURATION_MS` | Recording time after recorder readiness; `5000`, or `0` until stopped |
| `DM_PROFILE_NATIVE_TRACE_ENERGY` | Include macOS energy measurements; `1` by default, `0` to disable |
| `DM_PROFILE_NATIVE_TRACE_PATH` | Absolute `.trace` output path or folder; optional |

**Energy consumption** is checked by default. It applies to startup and
Debug-menu recordings. macOS asks for administrator authorization to run Apple's
`powermetrics` sampler; the engine and recorder remain unprivileged. Recording
starts after authorization and the first valid sample. Cancelling authorization
reports a recording error. Uncheck the option when administrator access is
unavailable or when measuring performance without the sampler's overhead.

With zero startup delay, engine initialization waits for the recorder to become
ready so initialization runs with the recorder attached. A positive delay lets the engine run
while waiting and attaching. Recording stops and saves without stopping the game.
Normal engine shutdown also stops and saves a running recording.
After a successful recording, the Console prints an `open -a Instruments` command
with the saved trace path. The Python runner prints the same command after saving.
Check **Open in Instruments when finished** to have the editor open completed
local traces automatically. This defaults to off and applies to both startup and
Debug-menu recordings, independently of the startup **Enable** checkbox.
Very short recordings may contain few or no events from Instruments; prefer the
five-second default when inspecting startup work.

To record during a run, select the running macOS engine under **Project → Target**
and choose **Debug → Profiler → XcTrace**. This uses the current delay, duration and
energy setting from **Preferences → Profile → macOS → XcTrace**, independently of the startup
**Enable** checkbox. The editor shows the requested output path, or a busy message
if a capture is already pending, recording, or saving. Recording errors are also
reported in the engine Console. With duration `0`, use the DELETE request below
or close the engine to stop and save the recording.

The debug engine service exposes the same recorder at `/profiler/trace`:

```sh
# Start now; path is a file or folder on the engine's Mac.
curl 'http://localhost:8001/profiler/trace' \
    --data-urlencode 'path=/tmp/game.trace' -d 'delay_ms=0' -d 'duration_ms=5000' -d 'energy=1'
# Check recording state, resolved output path, and failure message.
curl 'http://localhost:8001/profiler/trace'
# Cancel a pending recording, or stop and save a running recording.
curl -X DELETE 'http://localhost:8001/profiler/trace'
```

Use the engine's actual service port. POST accepts an
`application/x-www-form-urlencoded` body, or an empty body for the defaults,
and returns `202 Accepted`. Parameters go in the body so long paths are not
limited by the server's URL length limit. `energy` accepts `0` or `1` and defaults
to `1`; authorization happens on the engine's Mac. GET returns `state`, `path`,
`error`, and `message`. States are `idle`, `scheduled`, `starting`, `recording`,
`stopping`, `complete`, `cancelled`, `error`, or `unsupported`. A second POST returns
`409 Conflict` with `error: "busy"` throughout the delay, recorder startup,
recording, and saving; it leaves the first capture intact. Invalid requests return
`400`; unsupported platforms return `501`. Tool or recording failures after an
accepted request appear in GET status. DELETE returns `202` and is safe to repeat.
Separate `/start` and `/end` routes are unnecessary.

Without an output override, captures go to
`~/Library/Logs/<bundle-id>/Profiling/`, using `com.defold.dmengine` for an unbundled
engine. Existing captures are not overwritten. Keep the companion `.instrdst`
package and `.xctrace.log` recorder log with the `.trace` bundle. Captures use
**Game Performance**, **os_signpost**, **Defold Resource Loading**, and, by default,
**Defold Energy**. Resource
timing rows require `resource.profile=1`; this setting can be supplied in the
editor's Engine Arguments.

## Instruments capture

Record the application's launch with Instruments using the **Game Performance**
template and add the **os_signpost** instrument. This combines CPU call-stack
sampling, thread scheduling, system calls, Metal command activity, GPU execution
and display timing with the engine's annotations. See Apple's
[Metal performance capture guide](https://developer.apple.com/documentation/xcode/analyzing-the-performance-of-your-metal-app).

In **Time Profiler**, select the application and inspect its call tree for CPU
work, including functions without `DM_PROFILE` scopes. Inspect the Metal and GPU
tracks for graphics work. In **os_signpost**, filter by subsystem
`com.defold.profiler` and category `PointsOfInterest` for engine annotations.
The **Logging** template alone does not collect CPU samples or GPU execution.
CPU sampling is statistical, rather than a record of every function invocation;
detailed shader profiling requires a separate Metal GPU capture.

For a macOS launch capture, run the checked-in capture script from the repository:

```sh
python3 scripts/profile_native.py --bundle-id com.example.game --duration 5 -- \
    /absolute/path/dmengine --config=resource.profile=1 /absolute/path/game.projectc
```

The script compiles the resource instrument, creates the application suspended,
attaches `xctrace`, and waits for its recording-ready notification before resuming
the application. It enables native profiling through the environment and stops
the application at the runtime limit, including if it is still initializing.
Both the recording limit and application runtime default to five seconds.

Captures and companion stdout, recorder log, metadata, and compiled instrument
package are saved under `~/Library/Logs/<bundle-id>/Profiling/`. The capture,
stdout, and package paths are printed to stdout. Keep the `.instrdst` package
alongside a shared trace: the recipient needs it installed in Instruments to
display the custom table.

`xctrace` requires Xcode; Apple's standalone Command Line Tools package does not
include it. See
[Installing the command-line tools](https://developer.apple.com/documentation/xcode/installing-the-command-line-tools/).
If the desired Xcode is not selected, set `DEVELOPER_DIR` to its
`Contents/Developer` directory when running the script. The runner is macOS-only;
iOS signposts can be recorded using Instruments' device profiling workflow.

Scopes and frames are intervals. Dynamic scope names, including resource paths,
appear in the interval metadata because Apple's signpost names must be string
literals. Counters are emitted as named values at frame end; profiler log messages
and thread names are events.

The backend emits immediately and does not buffer events for a recorder that
attaches later. The custom instrument explicitly subscribes to
`com.defold.profiler` / `PointsOfInterest`, so startup recording does not depend on
discovering the subsystem after launch.

Signpost collection can lose events under heavy load. Check the annotation timeline
against the CPU/GPU tracks before interpreting gaps as idle engine time.

## Resource loading table

Select **Defold Resource Loading**, then **Resource Loads**, and sort **Total**
descending to find the slowest resource. The table contains the resource path,
total, preload, read, create, post-create, queue and wait durations, post-create
call count, result, and process. Durations are numeric Instruments duration columns.
Instruments displays zero durations as a dash.

The existing `DM_PROFILE` and `DM_PROFILE_DYN` scopes provide the timeline.
Resource completion logs already pass through `ProfileLogText`, the same callback
as `DM_PROFILE_TEXT`; the XcTrace profiler forwards them as `Log` signposts.
`instruments/DefoldResources.instrpkg` interprets those messages inside Instruments.
The native backend has no resource-specific parser or additional profiling API.
Keep the package's message format in sync with `LogLoadProfile` in
`engine/resource/src/resource_private.h`.
The package's `ResourceLoads.clp` parser separates the timing suffix at the last
` took ` delimiter, preserving paths that contain timing delimiters themselves.
It omits incomplete records truncated by the 1,024-byte profiler text buffer or
native logging; those messages remain available as raw `Log` signposts.

**Total** is elapsed load time, including dependencies and waits, rather than CPU
time. Parent and dependency totals overlap and should not be summed. **Reported**
is the time the logging thread delivered the message, not the resource's start or
completion time. Cache hits have no load record.

For manual recordings, build the package, open it in Instruments to install it,
and add **Defold Resource Loading** to the **Game Performance** template before
recording:

```sh
xcrun instrumentbuilder engine/engine/instruments/DefoldResources.instrpkg \
    -o /tmp/DefoldResources.instrdst
```

The capture script loads this package for its recording without installing it.
For the Instruments UI, install the compiled `.instrdst` once on each developer
machine. It can also be copied into
`~/Library/Application Support/Instruments/Packages/`; reopen Instruments afterward
to load the package. The `.trace` contains the recorded data, while the package
provides the custom table UI.

## Power and energy

Energy collection is enabled by default for editor and engine-service recordings
on macOS. CPU/GPU
durations alone are not energy measurements; adding an instrument to an existing
trace cannot recover energy data that was never collected.

Open the trace and select **Defold Energy**. Its **System Power Samples** table
shows CPU/GPU/ANE estimates in watts; **Application Energy Samples** shows the
game's energy impact per second. Expand the instrument's timeline to correlate
the measurements with other recorded work. Sampling requests a 500 ms interval;
the **Sample Window** column shows the actual interval, which can be longer.
Unsupported hardware or missing measurements produce an error, rather than a
successful recording without energy data.

The standalone Python runner requires `--energy` to include these measurements:

```sh
sudo -v
python3 scripts/profile_native.py --bundle-id com.example.game --duration 5 --energy -- \
    /absolute/path/dmengine --config=resource.profile=1 /absolute/path/game.projectc
```

Run both commands in the same terminal. Apple's `powermetrics` sampler and its
small Python supervisor need administrator access; the engine, recorder and
signpost helper run as your user. The supervisor uses the system Python in
isolated mode, with user packages disabled. It stops and reaps its sampler when
the capture closes or fails, without needing to renew sudo authorization.
The sampler has a finite sample count and requests a 500 ms sampling interval;
the supervisor also enforces a deadline if the sampler stalls.
Missing administrator authorization is reported before the application launches.
This collector uses the Apple silicon CPU/GPU/ANE power counters; unsupported
hardware or output formats are reported as errors.

The **Defold Energy** instrument in the same `.trace` shows estimated system CPU,
GPU and ANE power in watts, where the hardware reports them, plus the selected
application's energy impact per second. System power includes work from other
processes and excludes components such as the display; it is not the game's power
consumption or the Mac's total power. Application energy impact is a platform-specific
score, not watts or joules. Do not interpret either measurement as per-resource
energy use, or compare scores across different hardware.

Power rows include the actual **Sample Window**. **Reported** is when the engine
or helper published the sample, which summarizes the preceding window. The Python
runner's companion
`.energy.jsonl` stores the normalized samples and original sample timestamps;
it contains only system power and the selected process's impact. A final sample
can arrive after recording has stopped. Use the trace timeline when correlating
samples with resource loading and CPU/GPU work. The `.energy.log` contains sampler
diagnostics. Unknown or absent measurements are not filled in with zeros.
The runner waits for the signpost helper to initialize before starting the game.
Before reporting success, it exports the saved energy tables and requires both
system power rows and impact rows for the selected PID. The `.json` metadata
records these retained row counts separately from the sidecar's sample counts.

For iPhone/iPad, use Apple's **Power Profiler** on a physical device running
iOS/iPadOS 26 or later. Pair and unlock the device in Xcode and enable Developer
Mode. Use wireless debugging with the device unplugged: Apple reports overall
battery consumption as zero while charging. Launch a debug app with native
profiling enabled and `resource.profile=1`, then obtain its PID:

```sh
xcrun devicectl device info processes --device <device-id>
python3 scripts/profile_native.py --bundle-id com.example.game --duration 5 \
    --power --device <device-id> --attach <pid>
```

This mode attaches to the selected process, records **Game Performance**,
**os_signpost**, **Defold Resource Loading** and **Power Profiler** in one `.trace`,
and leaves the application running. It checks tool and device eligibility before
recording. Native profiling must already be enabled; attaching cannot change the
running app's environment or capture earlier startup events. For device startup
profiling, configure the same instruments in Instruments before launching the app.

Expand **Power Profiler** to inspect overall battery consumption and per-app CPU,
GPU, display and networking impact on the same timeline as the resource and CPU
tracks. Confirm that those lanes contain samples before treating the recording as
an energy capture. Impact values are scores, not per-resource watts or joules.
See Apple's [Power Profiler guide](https://developer.apple.com/documentation/xcode/measuring-your-app-s-power-use-with-power-profiler)
and [combined CPU and power recording example](https://developer.apple.com/videos/play/wwdc2025/226/).

## Tests

On macOS, run the native listener tests with both build defaults and the profiler
core regression tests from a configured CMake build:

```sh
cmake --build engine/build/arm64-macos --target \
    run_test_engine_service_xctrace_profiler run_test_engine_service_xctrace_profiler_enabled run_test_engine_service_xctrace run_test_profile
```

The listener tests replace the OS event sink so recording transitions are
deterministic. To exercise the editor launch environment, run
`lein test editor.profiler-xctrace-test` from `editor/`.

Validate real CPU/GPU/energy collection with the five-second macOS capture
above and a Power Profiler capture on a supported physical iPhone/iPad.
