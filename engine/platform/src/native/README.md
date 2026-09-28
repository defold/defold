# Mobile and web platform implementation

These sources implement Defold's mobile and web platform backend and are built
into `platform` (and Android's `platform_vulkan` variant). Android Java classes
are installed as `share/java/platform_android.jar`; the browser event library
is installed as `lib/<platform>/js/library_platform.js`.

The native implementation was extracted from Defold's modified GLFW 2.7.1 fork.
Original third-party copyright and license notices are retained in derived files.
The private runtime interface is being reduced to the operations used by Defold;
engine consumers use the platform headers rather than these private headers.
