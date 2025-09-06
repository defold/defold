message("features.cmake:")

option(BUILD_TESTS "Build unit tests" ON)
option(USE_VULKAN "Use Vulkan graphics adapter for app test" OFF)

message(STATUS "BUILD_TESTS: ${BUILD_TESTS}")
message(STATUS "USE_VULKAN: ${USE_VULKAN}")
