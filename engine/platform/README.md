# Platform window backends

The window backends live under `src/android`, `src/ios`, `src/linux`,
`src/macos`, `src/web`, and `src/win32`. Each provides the engine's
`dmPlatform` interface for its target.

The window HTTP service and its OpenAPI schema live in `src/service`.
Its headers are private to the service and its tests.

Android and iOS share window lifecycle, keyboard, pointer, and joystick code
in `src/mobile_shared`. Android uses the NDK and iOS uses UIKit. Both share
the C++ window adapter in `src/window_mobile_web.cpp` with the web backend,
which handles DOM input, canvas events, and the Gamepad API in JavaScript.

Android builds install `share/java/platform_android.jar` and provide a
`platform_vulkan` variant that leaves surface creation to the renderer.
Web builds install the browser event library at
`lib/<platform>/js/library_platform.js`.
