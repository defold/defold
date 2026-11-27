# Web window backend

The game window is the HTML canvas in `Module['canvas']`. The WebAssembly
backend implements `dmPlatform`, owns the canvas window handle and touch slots,
and calls JavaScript to access the DOM. Emscripten links these bindings with
`--js-library`. Web builds also install them at `lib/<platform>/js/library_platform.js`.
[Web window implementation](window_web.cpp) · [JavaScript library](library_platform.js) ·
[Build rules](../../CMakeLists.txt)

## Opening and resizing the canvas

For OpenGL, `dmNativeOpenWindowJS` asks Emscripten to create a WebGL context.
It uses the requested version hint and can retry with WebGL 1. For WebGPU it
leaves context creation to the graphics adapter. A browser canvas can bind only
one context type. If the engine closes the window and retries with a different
API family, the backend replaces the canvas, preserves its DOM attributes, and
rebinds its listeners. [Canvas preparation and context creation](library_platform.js)

The window dimensions come from the canvas backing buffer. During
`dmNativeSwapBuffers` the backend checks for size changes. In fullscreen it
computes the backing size from the viewport and pixel ratio, then notifies the
engine through the resize callback. Fullscreen and pointer-lock change events
update the actual state after the browser handles a request. [Display handling](library_platform.js)

## Input and tests

DOM listeners handle keyboard, focus, mouse, touch, and controller connection
events as they arrive, so the native poll function has no event queue to drain.
The backend maps CSS touch coordinates to canvas pixels and accumulates
movement until the engine reads it. A tap that begins and ends between reads is
delivered across two reads. Controller queries use the browser's Gamepad API.
[DOM input](library_platform.js) · [Touch accumulation](window_web.cpp) ·
[Touch reader](../window_touch.c)

`run_test_library_platform_js` exercises the JavaScript library with Bun or Node.
`test_native_web` covers the compiled touch and GL entry points. For end-to-end
coverage, `run_test_native_web_browser` runs the `wasm-web` build in Chromium
and exercises rendering, canvas recreation, fullscreen, and pointer lock.
[JavaScript tests](../test/test_library_platform.js) · [WebAssembly tests](../test/test_native_web.cpp) ·
[Browser test setup](../test/browser/README.md) · [Platform module](../../README.md)
