# VkQuality

Android Vulkan quality recommendation library imported from:

https://github.com/android/vkquality

Imported revision:

2642a0d5e4cbef4b064bbe6cd3967bce97f67883

This external package contains only the runtime pieces used by Defold:

- Native C++ runtime from `vkq_library/vkquality/src/main/cpp`, excluding upstream tests and the
  JNI entry points used by the Unity Java wrapper.
- Default `vkqualitydata.vkq` runtime data asset.
- Apache 2.0 license.

The upstream Gradle project, Java and Unity wrappers, Unity sample project, editor tool, and tests
are intentionally not vendored here. The package is built with CMake through `scripts/build.py
build_external --package=vkquality`.

VkQuality is packaged as an optional `libvkquality.so`. On Android builds containing both Vulkan
and OpenGL ES, Defold loads its C API at runtime and passes the physical-device properties gathered
by Defold's Vulkan support probe to `vkQuality_initializeFlagsInfo`. This avoids VkQuality creating
an additional Vulkan instance and lets builds without a fallback graphics adapter omit the library.
