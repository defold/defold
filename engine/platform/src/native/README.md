# Mobile and web platform implementation

These sources implement Defold's mobile and web platform backend and are built
into `platform` (and Android's `platform_vulkan` variant). Android Java classes
are installed as `share/java/platform_android.jar`; the browser event library
is installed as `lib/<platform>/js/library_platform.js`.

The native implementation was extracted from Defold's modified GLFW 2.7.1 fork.
Original third-party copyright and license notices are retained in derived files.
Window creation uses Defold's `WindowCreateParams` directly. These private C and
JavaScript helpers provide the native lifecycle and input implementation; they
are not a GLFW compatibility API and are not installed as SDK headers. Desktop
platforms continue to use the separate GLFW 3 backend.
