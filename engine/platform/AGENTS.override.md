# Mobile and web backend provenance

Defold developed its Android and iOS ports in its GLFW fork. The original
Android port (`454940f3da`) included `initThreads` and `terminateThreads`
from GLFW's X11 backend. Both functions were later removed.

The historical browser bridge credited Éloi Rivard and Thomas Borsos for the
Emscripten GLFW 2 implementation (`f46f0c5d4d`). Defold replaced its
`DOMToNativeKeyCode` switch with the `keyNumber` lookup in `92d3a152c7`.
`src/web/library_platform.js` retains Defold's touch coordinate conversion
and `onTouchMove` handling from the fork (`669472cf31`, `7384144d4b`).

Use the repository's license-header guidance for first-party code and preserve
third-party notices when carrying code forward. [License headers](../../AGENTS.md#license-headers)
