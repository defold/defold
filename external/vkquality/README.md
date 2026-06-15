# VkQuality

Android Vulkan quality recommendation library imported from:

https://github.com/android/vkquality

Imported revision:

2642a0d5e4cbef4b064bbe6cd3967bce97f67883

This external package contains only the runtime pieces used by Defold:

- Native C++ runtime from `vkq_library/vkquality/src/main/cpp`, excluding upstream tests.
- Java startup mitigation and JNI wrapper classes from `vkq_library/vkquality/src/main/java`.
- Default `vkqualitydata.vkq` runtime data asset.
- Apache 2.0 license.

The upstream Gradle project, Unity sample project, editor tool, and tests are intentionally not
vendored here. The package is built with CMake through `scripts/build.py build_external
--package=vkquality`. The Java annotation dependency on AndroidX was removed from
`StartupMitigation.java` so the jar can be built with Defold's existing Android SDK classpath.
