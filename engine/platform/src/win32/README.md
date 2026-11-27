# Windows window backend

`OpenWindow` creates an HWND using a window class with its own device context.
During `WM_NCCREATE`, the window procedure stores the Defold window pointer in
`GWLP_USERDATA`. Later messages can then reach the callbacks and input state for
that particular window. `PollEvents` drains the calling thread's message queue,
so polling one handle also delivers messages for other windows on that thread.
[Window procedure and event loop](window_win32.cpp)

## Size and graphics

The backend measures the client rectangle for resize callbacks and adjusts the
outer frame when setting a client size. It handles `WM_DPICHANGED` when a window
moves between monitors and uses monitor bounds and DPI when restoring saved
placement. [Window metrics](window_win32.cpp) · [Display queries](window_display.cpp) ·
[Placement](window_placement.cpp)

| Requested API | What the window backend creates |
| --- | --- |
| Desktop OpenGL | WGL context on the window's device context. A helper window is used to load WGL extensions. An optional shared context supports worker uploads. [WGL implementation](window_opengl.cpp) |
| OpenGL ES | An EGL window surface and current ES context through ANGLE. The Windows loader must be able to find `libEGL.dll` and `libGLESv2.dll`. The optional shared context uses a pbuffer. [EGL implementation](../window_egl.cpp) |
| Vulkan | An HWND and, through the Vulkan variant, a `VkSurfaceKHR` for that HWND. [Vulkan surface](window_vulkan.cpp) |
| DirectX or WebGPU | An HWND that the graphics adapter uses to set up presentation. [Native handles](../platform_window_win32.h) |

## Input and tests

The input handler decodes scan codes, text, and mouse messages. A modal dialog can
take mouse capture before Windows delivers `WM_KILLFOCUS`. The backend defers the
button reset so the focus-loss callback can still read the buttons held when the
window lost focus. Gamepad discovery combines XInput and DirectInput and filters
devices exposed by both. [Message handling](window_input.cpp) ·
[Focus dispatch](window_win32.cpp) · [Joystick discovery](window_joystick.cpp)

The Win32 tests cover focus and capture message ordering and polling multiple
windows. The desktop graphics tests exercise ANGLE rendering, an HWND for
WebGPU, and shared WGL contexts. [Window and input tests](../test/test_platform_win32.cpp) ·
[ANGLE and WebGPU tests](../test/test_window_placement_win32.cpp) ·
[WGL tests](../test/test_window_opengl_win32.cpp) · [Platform module](../../README.md)
