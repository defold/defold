# Linux window backend

`NewWindow` first tries to connect to X11, then tries Wayland if the X11 connection
fails. An accessible XWayland display therefore takes the X11 path even when
`WAYLAND_DISPLAY` is set. Each handle records the chosen connection and a table
of window operations. `PollEvents` dispatches pending events for windows on the
calling thread. On Wayland, `ShowWindow` also dispatches events while waiting
for the compositor to configure a hidden window. If a callback deletes that
window, the backend keeps its display connected until the outermost poll or
show call returns. [Backend selection and polling](window_linux.cpp) ·
[Window operations](window_linux.h)

## Window systems

- **X11.** Xlib delivers changes to window geometry through `ConfigureNotify`.
  The backend measures the client area in pixels and uses RandR for monitor
  geometry. XKB maps physical keys, while X input methods supply text. Tools in
  the same desktop session can identify the window by its XID.
  [X11 window and events](window_x11.cpp) · [Key mapping](window_x11_keys.cpp)
- **Wayland.** The compositor configures the logical size of an `xdg_toplevel`.
  The backend converts that size to drawable pixels using the output scale or
  the fractional scale and viewport protocols when available. The compositor
  owns window positioning. Where supported, the session management protocol
  lets the compositor restore a toplevel. Seat events supply pointer and
  keyboard input, with xkbcommon handling keys and text composition.
  [Wayland window](window_wayland.cpp) · [Input](window_wayland_input.cpp) ·
  [Session restoration](window_wayland_session.cpp)

## Graphics and controllers

Desktop OpenGL on X11 uses GLX. OpenGL ES on X11 and both GL APIs on Wayland use
EGL. On Wayland the backend resizes the native EGL window to the drawable's
pixel dimensions. Vulkan uses a Wayland surface on a Wayland connection and
supports XCB and Xlib surfaces on X11. [GL contexts](window_opengl.cpp) ·
[Vulkan surface selection](window_vulkan.cpp)

Both window systems share an evdev joystick implementation. It scans
`/dev/input` for controllers, monitors device changes with inotify, and
translates Linux input events into joystick state. [Joystick implementation](window_joystick.cpp)

## Tests

On a configured Linux build, `run_linux_window_tests` runs the window tests
against Xvfb and Openbox for X11 and Weston for Wayland. The runner selects each
display explicitly, so both backend paths are exercised on the same host.
[Test runner](../test/run_linux_window_tests.py) · [Linux window tests](../test/test_window_linux.cpp) ·
[Platform module](../../README.md)
