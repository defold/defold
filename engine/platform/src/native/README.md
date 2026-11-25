# Mobile and web platform implementation

Defold implemented the Android, iOS and web backends in this directory. They
build into `platform` and, on Android, the `platform_vulkan` variant. Android
builds install `share/java/platform_android.jar`. Web builds install the browser
event library at `lib/<platform>/js/library_platform.js`.

Android and iOS share the window lifecycle, keyboard and pointer state, and
touch polling. The Android backend uses the NDK and the iOS backend uses UIKit.
On the web, the JavaScript library handles DOM input, canvas events, and the
Gamepad API. All three build from Defold's private platform sources and use
`WindowCreateParams` to open a window. Desktop implementations live in the
sibling `macos`, `win32`, and `linux` directories.
