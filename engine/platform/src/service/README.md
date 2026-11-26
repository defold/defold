# Window HTTP service

The platform extension registers `/window` with the engine's debug HTTP server
when the server is available. After graphics initialization it attaches the
graphics window to the service. A headless engine has no native window to attach.
The extension updates the service during engine updates and detaches it when the
engine finalizes. This lets the HTTP server outlive an individual window.
[Extension lifecycle](platform_service_extension.cpp)

## How a resize reaches the window

1. `GET /window` returns the observed client and drawable dimensions, display
   scale, a geometry revision, and whether resizing is supported. A client can
   use that revision to make its next resize conditional on the observed geometry.
2. Before submitting a resize, a client that will poll the result reads
   `engine_instance_id` from `GET /state`. It posts the requested width and height
   in client units to `/window/resize` and receives the receipt URL in the
   `Location` header. The service update issues the resize and checks the actual
   client size immediately and on later updates while the operation is pending.
3. The client reads the receipt URL with the saved ID in the `X-Engine-Instance`
   header. The receipt reports whether the window reached the requested size or
   the operation ended with an error, such as a timeout. Engine updates advance
   the operation independently of how often a client reads the receipt.

The OpenAPI schema specifies request fields, receipt states, and error responses.
The service tests drive updates and HTTP requests separately so they can exercise
asynchronous resizes and window replacement. [Schema](platform_service_openapi.h) ·
[Implementation](platform_service.cpp) · [Service tests](../test/test_platform_service.cpp) ·
[Platform module](../../README.md)
