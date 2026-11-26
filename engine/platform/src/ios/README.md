# iOS window backend

`AppBootstrap` passes the engine callbacks to the application delegate and
enters `UIApplicationMain`. The scene delegate accepts one application window
scene and creates its `UIWindow` and root view controller. As that scene becomes
active or resigns active, the backend updates the iconified state and calls the
engine's focus callback. [Bootstrap](app_ios.mm) · [Application delegate](app/AppDelegate.m) ·
[Scene delegate](app/SceneDelegate.m)

## Rendering and the frame loop

The view controller selects a rendering view for the requested API.
`EAGLView` owns the OpenGL ES context, framebuffer, and drawable renderbuffer,
and can create an auxiliary context in the same sharegroup. The no-API path
uses `MetalView`, backed by a `CAMetalLayer`, for renderers such as Metal and
Vulkan. Both views use the same `BaseView` implementation for frame scheduling
and input. [View selection](app/ViewController.m) · [GLES view](app/EAGLView.m) ·
[Metal layer](app/MetalView.m)

A `CADisplayLink` on the main run loop calls the view, which asks the application
delegate to advance the engine. When the controller replaces the rendering view
it invalidates the old display link before starting the new one. This prevents the
old view from continuing to drive frames. [Display link](app/BaseView.m) ·
[Engine update](app/AppDelegate.m)

## Input, safe areas, and tests

`BaseView` tracks UIKit touches by identity and passes committed characters and
marked text from the software keyboard into the shared input code. UIKit reports
safe-area insets in points. The native window queries convert them to drawable
pixels and expose the safe rectangle with a bottom-left origin. An inset change
can trigger a resize notification even if the view's dimensions stay the same.
[Input and inset notifications](app/BaseView.m) · [Window queries](platform_window_ios.mm)

The iOS tests exercise frame-rate settings across view recreation, safe-area and
reserved-area notifications, and home indicator behavior.
[Frame-rate tests](../test/test_platform_frame_rate.mm) ·
[Safe-area tests](../test/test_platform_safe_area.mm) ·
[Home indicator tests](../test/test_platform_home_indicator.mm) ·
[Shared mobile implementation](../mobile_shared/README.md) · [Platform module](../../README.md)
