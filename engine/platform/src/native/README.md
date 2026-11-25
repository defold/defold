# Mobile and web platform implementation

The Android, iOS and web backends in this directory build into `platform`
and, on Android, the `platform_vulkan` variant. Android builds install
`share/java/platform_android.jar`. Web builds install the browser event library
at `lib/<platform>/js/library_platform.js`.

Android and iOS share the window lifecycle, keyboard and pointer state, and
touch polling. The iOS backend uses UIKit. The Android backend uses the NDK and
retains initialization and shutdown code adapted from [GLFW 2.7.1](android/android_init.c).

On the web, the JavaScript library handles DOM input, canvas events, and the
Gamepad API. All three build from Defold's private platform sources and use
`WindowCreateParams` to open a window. Desktop implementations live in the
sibling `macos`, `win32`, and `linux` directories.
