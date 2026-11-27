# Shared mobile window implementation

Android and iOS compile the same `dmPlatform` implementation for their single
application window. It owns the window handle, cached dimensions, callbacks, and
reserved areas. Native handlers deliver focus and input callbacks with the
user data registered by the engine. [Window implementation](window.cpp) ·
[Native mobile state](window_mobile_private.h)

## Window lifetime

`dmNativeInit` initializes the OS backend and shared process state. Opening a
window clears input, initializes the dimensions, and calls `dmNativeOSOpenWindow`
with either an OpenGL or a no-API request. If opening fails or the context lacks
the required GLES version, the shared code closes the native window and clears
its state. The OS hooks handle resources such as an Android surface or an iOS
rendering view. [Initialization](window_init.c) · [Window operations](window.cpp)

On Android and iOS, the first call to `GetReservedAreas` starts observing the
parts of the window reserved by the OS. The window caches the rectangles in a
stable order and refreshes them before delivering a resize callback. Android
signals inset changes from the activity. On iOS the window also compares the
rectangles during polling, so an occlusion change alone can trigger the callback.
Closing the window clears the observation. [Reserved-area handling](window.cpp)

## Input between frames

Native handlers write key, pointer, and touch state as events arrive. The mouse
code remembers a press followed by a release between two reads, so HID can still
observe the click. Touch slots retain a short tap across two reads, delivering
a begin followed by an end. Reading a touch also consumes its accumulated movement.
The same touch reader serves the web backend. [Pointer and key state](window_input.c) ·
[Touch reader](../window_touch.c) · [Input tests](../test/test_mobile_input.cpp) ·
[Touch tests](../test/test_native_touch.cpp)

[Android](../android/README.md) · [iOS](../ios/README.md) · [Web](../web/README.md) ·
[Platform module](../../README.md)
