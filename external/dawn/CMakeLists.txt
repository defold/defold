include(FetchContent)

# Keep the WebGPU API in sync with the revision used by the graphics backend.
set(DAWN_REVISION 6bab1bd9fd23c03ab2b0f26e9b489e0b6b1844f2)
FetchContent_Declare(dawn
  URL "https://codeload.github.com/google/dawn/tar.gz/${DAWN_REVISION}"
  URL_HASH SHA256=189bb28e49a3e59423a66d3418899032f7d5ab2fec721ebf3dfbcbc219af750b
  EXCLUDE_FROM_ALL)

set(BUILD_SHARED_LIBS OFF)
set(DAWN_FETCH_DEPENDENCIES ON)
set(DAWN_BUILD_MONOLITHIC_LIBRARY STATIC)
set(DAWN_ENABLE_INSTALL OFF)
set(DAWN_ENABLE_METAL ON)
set(DAWN_ENABLE_NULL OFF)
set(DAWN_ENABLE_VULKAN OFF)
set(DAWN_ENABLE_DESKTOP_GL OFF)
set(DAWN_ENABLE_OPENGLES OFF)
set(DAWN_BUILD_TESTS OFF)
set(DAWN_BUILD_SAMPLES OFF)
set(DAWN_BUILD_PROTOBUF OFF)
set(DAWN_USE_GLFW OFF)
set(TINT_BUILD_TESTS OFF)
set(TINT_BUILD_CMD_TOOLS OFF)
set(TINT_BUILD_GLSL_VALIDATOR OFF)
set(CMAKE_OSX_DEPLOYMENT_TARGET "${SDK_VERSION_MACOSX_MIN}")

FetchContent_MakeAvailable(dawn)
add_custom_target(dawn ALL DEPENDS webgpu_dawn)

install(FILES "$<TARGET_FILE:webgpu_dawn>" DESTINATION "lib/${TARGET_PLATFORM}")
install(DIRECTORY "${dawn_SOURCE_DIR}/include/webgpu" DESTINATION include)
install(FILES "${dawn_BINARY_DIR}/gen/include/webgpu/webgpu_cpp_chained_struct.h"
        DESTINATION include/webgpu)
install(FILES "${dawn_BINARY_DIR}/gen/include/dawn/webgpu.h"
              "${dawn_BINARY_DIR}/gen/include/dawn/webgpu_cpp.h"
              "${dawn_BINARY_DIR}/gen/include/dawn/webgpu_cpp_print.h"
        DESTINATION include/dawn)
install(FILES "${dawn_SOURCE_DIR}/LICENSE" DESTINATION share/licenses/dawn)
