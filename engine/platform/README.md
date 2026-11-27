# Platform module

The platform module owns the engine window and translates operating system events into
window callbacks and input state. The graphics adapter uses it to obtain a drawable,
a graphics context, or a native window for creating a rendering surface. The HID
system polls it for events and reads the resulting input state.

## Backends

| Implementation guide | Native window or surface |
| --- | --- |
| [Android](src/android/README.md) | NativeActivity, the Android looper, and replaceable native window surfaces |
| [iOS](src/ios/README.md) | A UIKit window scene, rendering view, and display link |
| [Linux](src/linux/README.md) | X11 or Wayland, selected when the window handle is created |
| [macOS](src/macos/README.md) | An AppKit window and content view on the main thread |
| [Web](src/web/README.md) | An HTML canvas and DOM event handlers connected to WebAssembly |
| [Windows](src/win32/README.md) | An HWND, its message queue, and the selected graphics API |

## From the API to the window

The SDK exposes an opaque `HWindow` and C functions in `<dmsdk/platform/window.h>`.
The C entry points forward to the engine's C++ `dmPlatform` interface. Each backend
defines the state behind the handle. Android and iOS share their window lifecycle
and input handling. Web implements the same engine interface for the browser canvas.
[SDK header](src/dmsdk/platform/window.h) · [C entry points](src/window.cpp) ·
[Engine interface](src/window.hpp) · [Shared mobile implementation](src/mobile_shared/README.md)

1. Call `WindowNew` and check that it returned a handle. Initialize
   `WindowCreateParams` with `WindowCreateParamsInitialize`, select the graphics
   API, and set the callbacks and their user data. Call `WindowOpen` and check
   its `WindowResult` before using the window.
2. During a frame, native HID calls `dmPlatform::PollEvents` before reading keys,
   pointer buttons, and other input. Backends also deliver callbacks for events such
   as resize or focus changes. Some callbacks can run during a native window
   operation, including a programmatic resize. [Native HID update](../hid/src/native/hid_native.cpp)
3. Call `WindowClose` to close the window. The handle can then be opened again.
   Call `WindowDelete` when the handle itself is no longer needed.

For rendering, `GetWindowWidth` and `GetWindowHeight` report drawable dimensions.
For a resize request, `GetWindowClientSize` reports the actual client dimensions in
the units accepted by `SetWindowSize`. These can differ on a scaled display. Code
that needs to observe a resize should read the new client size after the backend
processes it. Desktop tools can also query a borrowed native window ID on the
owner thread while a Win32, Cocoa, or X11 window is open. [Window queries](src/window.hpp)

## Build variants and shared services

- `platform` builds the backend selected by `TARGET_PLATFORM_OS`. Vendor builds can
  supply their own implementation through `DEFOLD_PLATFORM_WINDOW_SOURCES`.
  On macOS, Windows, and Linux, `platform_vulkan`
  adds native Vulkan surface helpers. On Android it selects the implementation
  that leaves surface creation to the renderer. [CMake target selection](CMakeLists.txt)
- `platform_null` supplies the window API for headless engines and unit tests. It
  keeps the requested dimensions and open state in memory and calls the resize
  callback when its size changes. Event polling and buffer swapping are empty.
  [Null backend](src/platform_window_null.cpp)
- On desktop, `DM_WINDOW_STATE` can supply a path prefix for saving and restoring
  window placement. The shared code stores normal geometry in a per-backend
  `<prefix>-<backend>.ini` file. Each backend translates between its window system
  and the stored geometry. [Placement](src/window_placement.cpp) · [State file](src/window_state.cpp)
- `platform_service` attaches the engine's debug HTTP server to the active window.
  It exposes observed geometry and tracks resize requests across engine updates.
  [Window HTTP service](src/service/README.md)

## Tests

From the repository root in a Defold build shell (`./scripts/build.py shell`),
run the common window API tests against a test-enabled CMake build. For example,
an Apple Silicon macOS build uses this command.

```sh
cmake --build engine/build/arm64-macos --target run_test_platform
```

This target uses the null backend. Each platform guide links to the tests that
exercise its native behavior. [CMake setup](../../scripts/cmake/README.md) ·
[Platform test targets](src/test/CMakeLists.txt)
