# Android window backend

The Android window belongs to the activity. At startup, the engine installs NDK
app-command and input callbacks on the native app glue and waits for
`APP_CMD_INIT_WINDOW` before starting the engine thread. The looper thread
continues to receive Android events while the engine runs. The callbacks copy
commands and input into queues under a lock. During engine polling,
`dmNativeAndroidFlushEvents` takes the queued events, handles window lifecycle
changes, then delivers input. [Engine startup](../../../engine/src/engine_main.cpp) ·
[Callback registration](app_android.cpp) · [NDK event handlers](android_init.c) ·
[Engine-side dispatch](window_android.c)

## Native window and rendering

Android can destroy and replace the `ANativeWindow` while the engine is still
running. The common window code dispatches `APP_CMD_TERM_WINDOW` and
`APP_CMD_INIT_WINDOW` through a small set of backend hooks. In the GLES build,
the hooks manage the EGL window surface and its relationship to the context.
The `platform_vulkan` build uses the no-API implementation to track window
availability and dimensions while the renderer manages its surface.
[Backend hooks](window_backend.h) · [GLES lifecycle](window_opengl.c) ·
[EGL implementation](android_util.c) · [No-API lifecycle](window_no_api.c)

Native clients that need to hold a window reference call `AcquireAndroidWindow`
and pair a successful acquisition with `ReleaseAndroidWindow`. Holding that
reference keeps the object alive. `IsAndroidWindowCurrent` checks whether the
activity still uses it. [Native window interface](../platform_window_android.h)

## Input, insets, and tests

`DefoldActivity` provides the Android UI for text entry, reads window insets and
cutouts, and notifies native code when they change. The native backend converts
reserved rectangles from Android view coordinates into engine window coordinates.
An inset change can trigger the resize callback even when the drawable keeps
the same dimensions. Controller motion and key events feed the native joystick
implementation. CMake packages the activity in `share/java/platform_android.jar`.
[Activity](java/com/dynamo/android/DefoldActivity.java) ·
[Native geometry](platform_window_android.cpp) · [Controllers](window_joystick.c)

The same inset tests run against the GLES and no-API variants. They cover
surface replacement, notification without a size change, and changes queued
inside a callback. The joystick tests exercise button and axis mappings.
[Inset tests](../test/test_platform_android_insets.cpp) ·
[Joystick tests](../test/test_platform_android_joystick.cpp) ·
[Shared mobile implementation](../mobile_shared/README.md) · [Platform module](../../README.md)
