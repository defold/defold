# macOS window backend

`NewWindow` runs on the main thread. The first handle initializes `NSApplication`,
installs an application delegate if needed, and sets up the menu. `OpenWindow`
creates an `NSWindow`, its delegate, and a `DMInputView` for the content area.
During `PollEvents` the backend drains pending AppKit events. The window delegate
translates notifications such as focus changes, miniaturization, and close
requests into engine callbacks. [Window and event loop](window_macos.mm)

## Resizing and rendering

The content view's bounds give the client size in points. Converting those bounds
to backing coordinates gives the drawable size in pixels. A resize or a backing
scale change updates both measurements and the graphics drawable before notifying
the engine. `SetWindowSize` can call game code synchronously through AppKit. It
retains the original native window and delegate and checks their association
after each call that can deliver a resize. This lets a callback close, reopen,
or delete the window safely. [Window metrics](window_macos.mm) ·
[Desktop placement in AppKit coordinates](window_placement.mm)

OpenGL uses an `NSOpenGLContext` attached to the content view and can provide a
second shared context for worker uploads. OpenGL ES uses the content view's
backing layer with ANGLE when the application supplies `libEGL.dylib` and
`libGLESv2.dylib`. Metal and WebGPU can obtain the native
view from the platform interface. The Vulkan variant creates a surface using
`VK_EXT_metal_surface` when available and supports `VK_MVK_macos_surface` as a
fallback. [OpenGL context](window_opengl.mm) · [Native handles](../platform_window_osx.h) ·
[Vulkan surface](window_vulkan.mm)

## Input and tests

`DMInputView` maps AppKit key and pointer events to window state. It implements
`NSTextInputClient` so composed characters and marked text from an input method
reach the engine separately from physical key events. An `IOHIDManager` on the
main run loop tracks joystick connections, axes, buttons, and hats.
[Input view](window_input.mm) · [Joystick implementation](window_joystick.mm)

The Cocoa tests cover resize callbacks that delete the window, backing scale
changes, focus, pointer input, and context lifetime. Separate tests exercise
saved placement and joystick reconnection. [Window tests](../test/test_window_cocoa.mm) ·
[Placement tests](../test/test_window_placement_cocoa.mm) ·
[Joystick tests](../test/test_window_joystick_cocoa.mm) · [Platform module](../../README.md)
