# Shared mobile window implementation

Android and iOS use the same C++ window adapter. It owns a single `HWindow`,
passes creation parameters to the native backend, and installs engine callbacks
with their user data. Web implements the engine interface for the browser canvas.
[Mobile adapter](../window_mobile_web.cpp) · [Native interface](../window_mobile_web_backend.h) ·
[Web backend](../web/README.md)

## Window lifetime

`dmNativeInit` initializes the OS backend and shared process state. Opening a
window clears input, initializes the dimensions, and calls `dmNativeOSOpenWindow`
with either an OpenGL or a no-API request. If opening fails or the context lacks
the required GLES version, the shared code closes the native window and clears
its state. The OS hooks handle resources such as an Android surface or an iOS
rendering view. [Initialization](window_init.c) · [Open, close, and poll](window.c)

On Android and iOS, the first call to `GetReservedAreas` starts observing the
parts of the window reserved by the OS. The adapter caches the rectangles in a
stable order and refreshes them before delivering a resize callback. Android
signals inset changes from the activity. On iOS the adapter also compares the
rectangles during polling, so an occlusion change alone can trigger the callback.
Closing the window clears the observation. [Reserved-area handling](../window_mobile_web.cpp)

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
