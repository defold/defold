defold_log("features.cmake:")

option(BUILD_TESTS "Build unit tests" ON)
option(USE_VULKAN "Use Vulkan graphics adapter for app test" OFF)

defold_log("BUILD_TESTS: ${BUILD_TESTS}")
defold_log("USE_VULKAN: ${USE_VULKAN}")
