# Automation migration validation

Validation performed on 2026-10-03 on Apple Silicon macOS 27.0 using Xcode 27.1,
CMake 4, and the extension-free sample in the companion Python repository.
The implementation is present in both working trees. The remaining platform
acceptance gates below have not passed and are required before release.

## Completed checks

| Area | Result |
| --- | --- |
| Stock macOS engines | Debug, headless, and release build and link successfully. |
| Mobile cross-compilation | arm64 Android and iOS debug/release engines build and link. Android ELF alignment check passes at 16 KiB. |
| SDK installation | CMake installs all four automation archives plus `render`, `render_inspection`, and `render_inspection_null`; verified with an isolated macOS install. |
| Release exclusion | macOS, Android, iOS, and web release images pass `test_release.py`: only null automation entry points remain, with no registration, protocol/Lua strings, projection-table implementation, or checked capture imports. |
| Optional macOS dependencies | Debug uses a weak ScreenCaptureKit load command; headless and release have no ScreenCaptureKit dependency. |
| Focused CTest | All four targets pass: stock headless runtime, release exclusion, unrelated native extension coexistence, and early legacy extension rejection. Each runtime fixture runs 13 cases. |
| Existing subsystem tests | 95 game object reload, spawn/delete, component, game object script, and GUI script tests pass. |
| Python unit/tooling suite | 210 tests pass, including remote URL handling, borrowed process ownership, v3 contracts, artifact transfer, and asynchronous capture helpers. |
| Python packaging | Wheel version 4.0.0 installs with `--no-deps` in a clean virtual environment. Isolated imports outside the checkout resolve to the installed package; runtime dependency metadata is empty. |
| Extension-free sample | Bob builds the project without a native extension. All 16 graphical Metal runtime tests pass with the modified editor and stock debug engine, including repeated build, compile/Bob, hot-reload command, reboot, and explicit target reattachment. |
| Additional v3 runtime tests | Four pass: coherent screenshot observation and artifact cleanup, stale queued element rejection, asynchronous video finalization, and two-frame Metal capture. |
| Capture output | PNG orientation and input overlays pass. H.264 recording and Metal trace downloads pass checksum verification; video output was inspected with ffprobe. |
| Capture shutdown | Normal engine exit succeeds during recording startup and during active recording with application audio. |
| Editor | Eight existing target tests (19 assertions) and the new HTTP/OpenAPI discovery test (three assertions) pass. |
| Clojure lint | No new diagnostics in changed code. The invocation reports three arity errors and four warnings already reproduced from the unchanged baseline; this is not a clean repository-wide lint result. |
| Repository checks | Diff whitespace checks, required first-party license headers, new test comments, Python syntax, and Extender manifest YAML parsing pass. |

The runtime fixtures cover real HTTP statuses, malformed/oversized requests,
application polling while frames advance, FIFO delivery, controller ownership,
null-HID touch dispatch, duplicate object names across collections, collection and
GUI-node recreation, owner annotation/callback cleanup, reboot identity, immutable
pagination, and retention by observation completion order. The graphical suite
also exercises modifiers, held input, cancellation/lease expiry, resize/bounds,
screen-coordinate orientation, overlay capture, and profiler metadata.

### Engine integration cleanup review

The follow-up review on 2026-10-03 removed inspection fields from existing game
object, GUI scene, sprite, GUI component, and GUI render-object structures. Reload
revisions now use the existing resource notification API inside automation;
projection history uses opt-in render-context tables. Eight existing engine files
no longer need changes. The retained hooks and their purpose are documented in
[the integration notes](README.md#integration-and-ownership).

Checks after this cleanup on Apple Silicon macOS:

- Debug, headless, release, and the native engine test binary build successfully.
  Null graphics tests now select `automation_capture_null`; the engine test binary
  has no ScreenCaptureKit import.
- All four automation CTest checks pass. Both runtime fixtures now run 13 tests,
  including repeated component/prototype/GUI reloads, owner cleanup, stable IDs
  for unaffected components, and HTTP routing after reboot.
- All 67 renderer tests and 81 game object reload, script, spawn/delete, and GUI
  script tests pass. The new renderer test checks that inspection allocates only
  when enabled and clears component projections between frames.
- The focused engine throttle and frame-pacing tests pass.
- The graphical sample suite passes 14 tests on Metal; its two editor-dependent
  tests are skipped with a directly launched engine. Coherent screenshot/artifact
  capture and queued deleted-target rejection both pass in a fresh engine. These
  suites use separate engines because the sample suite retains an input lease
  that would interfere with a subsequent client's input tests.
- Diff whitespace, first-party license headers, and Python syntax checks pass.

Other platform acceptance gates below were not rerun during this cleanup.

### Release library separation

The subsequent 2026-10-03 review moved automation hooks and render inspection to
real/null libraries. Release links `automation_null` and
`render_inspection_null`; the latter creates no inspection context. Projection
tables and their implementation are confined to `render_inspection`. The shared
renderer retains an opaque pointer and lifecycle/dispatch hooks.

Checks after this change:

- macOS debug, headless, release, and native engine test binaries build. Link
  commands select exactly one inspection implementation per executable.
- An isolated macOS SDK install includes all four automation archives and both
  inspection archives alongside the shared renderer.
- All four automation checks pass, along with 67 renderer tests and the new null
  inspection test. The real implementation clears both component and material
  projections between frames; the null implementation stays disabled when asked
  to enable and returns unavailable projections without allocating a context.
- The two graphical Metal observation/capture and deleted-target tests pass.
- iOS and Android debug/release engines cross-compile successfully. Web debug and
  release builds also succeed and select both null libraries.
- The strengthened exclusion check passes for macOS, iOS, Android, and web
  release images, allowing the null API symbols and the namespace in ELF debug
  metadata. It rejects the saved pre-cleanup release binary because that binary
  contains projection-table implementation, and rejects the real debug engine.
- Extender manifests parse successfully; library-list checks cover macOS, iOS,
  Android, web, Windows, and Linux. They select the same real/null implementations
  as CMake. No Extender service build or mobile/web runtime test was performed.

### Review regression fixes

The subsequent review corrections on 2026-10-03 validate raw UTF-8 and mutation
field types, preserve native pointer holds, give synthetic touches their own
per-frame HID sample, and allow touch injection before physical device input.
The documented `/events/wait` route now shares the immediate event polling
handler with `/events`.

- All five automation checks pass: 22 runtime cases each for stock headless and
  unrelated-extension engines, five graphical native input cases, release
  exclusion, and legacy-extension rejection. Input cases assert the game's
  actual press/release transitions, held positions, repeated taps, and cancellation.
- All 14 HID tests (1,737 assertions) and 18 input tests (286 assertions) pass.
  HID coverage includes fresh devices, disabled touch input, packet capacity,
  and preservation of physical touches.
- Automation commands are registered in both aggregate test runners and the
  `build_tests` dependency graph. All five checks passed through the normal
  resource-group scheduler; the focused native CMake target also passed.
- The fixture builds with Bob Light and repository built-ins. Its full runtime
  suite passed independently of the fixture built with full Bob.
- The automation and HID libraries cross-compile for arm64 Android and iOS.
  These are compilation checks; no physical mobile device was exercised.
- Diff whitespace checks pass. The platform acceptance gates below remain open.

## Dev merge follow-up (2026-10-06)

Merged `dev` at `bbb6666175`. The follow-up adds debugger pause servicing,
automation OpenAPI metadata, source-aware GO/GUI input regressions, graphics
likeness acceptance, and a reusable supplied-service runner. The companion
Python catalog refresh includes schema-helper expansion and 109 preferences.

Verified on Apple Silicon macOS 27.0.1:

- All nine automation CTest checks pass. The stock and unrelated-extension
  headless fixtures each run 25 cases; native input runs seven. Two harness
  regressions verify that borrowed services are checked and never terminated.
- Metal, OpenGL and Vulkan (MoltenVK) each match independently specified pixels
  at 100% across three successive screenshots, including stencil clipping,
  Retina scaling, scene bounds, coordinate conversion and actual GUI picking.
- The debugger engine suite reports 139 tests, 21 documented fixture skips,
  and no failures; the release exclusion check passes. Debugger stops retain
  responsive health, expire input leases and reject Lua/frame-dependent work
  until resumption.
- All 211 companion Python unit/tooling tests pass. The catalog generator runs
  successfully against the merged editor schema, including 2D/3D grid axes.

Verified on native `arm64-linux` in an Ubuntu 24.04 container, using Xvfb and
Mesa llvmpipe software Vulkan (Clang 18, Java 25):

- All seven automation CTest checks pass: harness ownership, stock headless
  runtime, release exclusion, unrelated-extension coexistence, legacy rejection,
  native input, and Vulkan capture. Both headless fixtures run 25 cases and
  native input runs seven; all three Vulkan captures match at 100%.
- The build caught and fixed a Linux-only collision between X11's `Font` typedef
  and Defold's text-layout header. Common automation code now includes the SDK
  headers it uses; native graphics headers stay in the platform implementation.
- The source checkout was mounted read-only; SDK/JNI dependencies and engines
  were built in a separate temporary root. Runtime checks used no network access
  beyond container-local services. This validates software graphics, not a
  physical Linux GPU or desktop recording permissions.

Verified on an iPhone 17 Pro simulator running iOS 26.4, using a locally built
`arm64_sim-ios` debug engine and Metal:

- All 23 supplied-service acceptance checks pass: 21 protocol/input/lifecycle
  cases, capture/artifact/coordinate acceptance, and reboot invalidation.
- All three simulator captures match at 100%. The native release binary passes
  the automation/inspection exclusion check. The simulator bundle helper stages
  the chosen local engine after Bob's engine-cache cleanup.
- This is simulator evidence. No physical iOS or Android behavior is claimed.

The runner saves `acceptance.json` and capture PNGs/likeness metadata locally.
Health records the active adapter, and the launcher explicitly supplies device
kind and OS/device name. Failed or skipped required checks fail acceptance.

## Outstanding acceptance gates

- Windows compilation and runtime, including Windows Graphics Capture and its
  optional API loading; no Windows SDK/toolchain was available here.
- Physical Android and iOS execution, including forwarded service URLs, touch,
  storage, lifecycle, and OpenGL ES/Vulkan/Metal capture. The paired iPhone had
  no matching provisioning profile, so iOS acceptance used the simulator as
  requested. No Android device was available; cross-compilation does not establish
  physical-device behavior.
- An actual Extender service build of debug, headless, and release variants,
  including a project with an unrelated extension. Local CMake coexistence and
  manifest parsing do not validate the Extender build service.
- Physical OpenGL ES capture and coordinate checks remain open. Metal, OpenGL
  and Vulkan captures on macOS, software Vulkan on Linux, and Metal captures in
  the iOS simulator now pass.
- OS-level recording permission denial, minimum-supported-OS execution, and
  shutdown/error behavior on the other supported platforms.

## Reproduction

Configure the normal CMake build with `-DAUTOMATION_BUILD_TESTS=ON`, then run from
the Defold repository root:

```sh
cmake --build engine/build/arm64-macos --target dmengine automation_tests
ctest --test-dir engine/build/arm64-macos -R '^automation_' --output-on-failure
cmake --build engine/build/arm64-ios --target dmengine dmengine_release
cmake --build engine/build/arm64-android --target dmengine dmengine_release
```

The companion repository's `DEVELOPMENT.md` describes package installation and
the Python unit, graphical, and capture suites. For capture tests use the engine
service URL, grant recording permission, and enable `METAL_CAPTURE_ENABLED=1`
when testing Metal traces. Runtime tests must be allowed to bind local service
ports and create native windows. Test fixtures launch and clean up their own
processes; they must never terminate a borrowed runtime.

For the editor-driven suite, launch the modified editor with a temporary
preferences file whose `:dev :custom-engine` points to the newly built stock
debug engine, then run the sample's `AutomationBridgeApiTest` without a supplied
engine URL. Repeated builds are verified by a new runtime identity, and reboot
reattachment uses the concrete target ID returned by the editor. The engine
service port can stay unchanged across these runs.
