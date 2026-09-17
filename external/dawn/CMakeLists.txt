cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
set(BUILD_TESTS OFF)
include(defold)

project(dawn_external LANGUAGES ${DEFOLD_LANGUAGE_LIST})

if(NOT TARGET_PLATFORM MATCHES "^(arm64-macos|arm64-linux|x86_64-linux|x86_64-win32)$")
  message(FATAL_ERROR "Unsupported Dawn platform: ${TARGET_PLATFORM}")
endif()

if(DEFOLD_EXTERNAL_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${DEFOLD_EXTERNAL_INSTALL_PREFIX}" CACHE PATH "External package install prefix" FORCE)
endif()

include(FetchContent)

# Keep the WebGPU API in sync with the revision used by the graphics backend.
set(DAWN_REVISION 6bab1bd9fd23c03ab2b0f26e9b489e0b6b1844f2)
set(_dawn_source_dir "${CMAKE_CURRENT_BINARY_DIR}/sources/${DAWN_REVISION}")
# Reuse sources restored by CI without restoring a compiler-specific build tree.
# The revision in the directory name prevents reuse after a Dawn revision change.
if(NOT FETCHCONTENT_SOURCE_DIR_DAWN AND EXISTS "${_dawn_source_dir}/CMakeLists.txt")
  set(FETCHCONTENT_SOURCE_DIR_DAWN "${_dawn_source_dir}")
endif()
FetchContent_Declare(dawn
  URL "https://codeload.github.com/google/dawn/tar.gz/${DAWN_REVISION}"
  URL_HASH SHA256=189bb28e49a3e59423a66d3418899032f7d5ab2fec721ebf3dfbcbc219af750b
  SOURCE_DIR "${_dawn_source_dir}"
  EXCLUDE_FROM_ALL)

set(BUILD_SHARED_LIBS OFF)
set(DAWN_FETCH_DEPENDENCIES ON)
set(DAWN_BUILD_MONOLITHIC_LIBRARY STATIC)
set(DAWN_ENABLE_INSTALL OFF)
set(DAWN_ENABLE_METAL OFF)
set(DAWN_ENABLE_D3D11 OFF)
set(DAWN_ENABLE_D3D12 OFF)
set(DAWN_ENABLE_NULL OFF)
set(DAWN_ENABLE_VULKAN OFF)
set(DAWN_ENABLE_SPIRV_VALIDATION OFF)
set(DAWN_ENABLE_DESKTOP_GL OFF)
set(DAWN_ENABLE_OPENGLES OFF)
set(DAWN_USE_X11 OFF)
set(DAWN_USE_WAYLAND OFF)
# Defold uses Win32 HWND surfaces, not UWP/WinUI controls.
set(DAWN_USE_WINDOWS_UI OFF)
set(DAWN_BUILD_TESTS OFF)
set(DAWN_BUILD_SAMPLES OFF)
set(DAWN_BUILD_PROTOBUF OFF)
set(DAWN_USE_GLFW OFF)
set(TINT_BUILD_TESTS OFF)
set(TINT_BUILD_CMD_TOOLS OFF)
set(TINT_BUILD_GLSL_VALIDATOR OFF)
# Defold supplies WGSL. Only build the output writer required by each backend.
set(TINT_BUILD_WGSL_READER ON)
set(TINT_BUILD_WGSL_WRITER OFF)
set(TINT_BUILD_SPV_READER OFF)
set(TINT_BUILD_GLSL_WRITER OFF)
set(TINT_BUILD_MSL_WRITER OFF)
set(TINT_BUILD_SPV_WRITER OFF)
set(TINT_BUILD_HLSL_WRITER OFF)

if(TARGET_PLATFORM MATCHES "macos$")
  set(DAWN_ENABLE_METAL ON)
  set(TINT_BUILD_MSL_WRITER ON)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "${SDK_VERSION_MACOSX_MIN}")
elseif(TARGET_PLATFORM MATCHES "linux$")
  set(DAWN_ENABLE_VULKAN ON)
  set(TINT_BUILD_SPV_WRITER ON)
  set(DAWN_USE_X11 ON)
  set(DAWN_USE_WAYLAND ON)
elseif(TARGET_PLATFORM STREQUAL "x86_64-win32")
  set(DAWN_ENABLE_D3D11 ON)
  set(DAWN_ENABLE_D3D12 ON)
  set(TINT_BUILD_HLSL_WRITER ON)
endif()

FetchContent_MakeAvailable(dawn)
if(TARGET_PLATFORM STREQUAL "x86_64-win32")
  set_target_properties(webgpu_dawn PROPERTIES PREFIX "lib")
endif()
add_custom_target(dawn ALL DEPENDS webgpu_dawn)

install(FILES "$<TARGET_FILE:webgpu_dawn>" DESTINATION "lib/${TARGET_PLATFORM}")
if(TARGET_PLATFORM STREQUAL "arm64-macos")
  # Strip debug information from the packaged copy, preserving linker symbols
  # and the original build library for local debugging.
  install(CODE "
    execute_process(
      COMMAND \"${CMAKE_STRIP}\" -S \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/lib/${TARGET_PLATFORM}/$<TARGET_FILE_NAME:webgpu_dawn>\"
      COMMAND_ERROR_IS_FATAL ANY)
  ")
endif()
install(DIRECTORY "${dawn_SOURCE_DIR}/include/webgpu" DESTINATION include)
install(FILES "${dawn_BINARY_DIR}/gen/include/webgpu/webgpu_cpp_chained_struct.h"
        DESTINATION include/webgpu)
install(FILES "${dawn_BINARY_DIR}/gen/include/dawn/webgpu.h"
              "${dawn_BINARY_DIR}/gen/include/dawn/webgpu_cpp.h"
              "${dawn_BINARY_DIR}/gen/include/dawn/webgpu_cpp_print.h"
        DESTINATION include/dawn)
install(FILES "${dawn_SOURCE_DIR}/LICENSE" DESTINATION share/licenses/dawn)
