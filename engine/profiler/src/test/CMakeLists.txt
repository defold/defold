cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

set(PROFILER_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(PROFILER_ROOT "${PROFILER_TEST_ROOT}/../.." ABSOLUTE)
set(PROFILER_SRC_DIR "${PROFILER_ROOT}/src")

defold_add_executable(test_profilerext_null "${PROFILER_TEST_ROOT}/test_profilerext_null.cpp")
target_include_directories(test_profilerext_null PRIVATE "${PROFILER_SRC_DIR}")
defold_target_link_libraries(test_profilerext_null "${TARGET_PLATFORM}"
  testmain
  dlib
  profilerext_null)
defold_register_test_target(test_profilerext_null OFF "${PROFILER_ROOT}")

set(PROFILER_RENDER_EXPORTS GraphicsAdapterNull)
set(PROFILER_RENDER_EXPORT_FILE "${CMAKE_CURRENT_BINARY_DIR}/profiler_render_exported_symbols.cpp")
defold_create_exported_symbols_file("${PROFILER_RENDER_EXPORT_FILE}" "${PROFILER_RENDER_EXPORTS}")

defold_add_executable(test_profilerext_render
  "${PROFILER_TEST_ROOT}/test_profilerext_render.cpp"
  "${PROFILER_RENDER_EXPORT_FILE}")
target_include_directories(test_profilerext_render PRIVATE "${PROFILER_SRC_DIR}")
defold_target_link_libraries(test_profilerext_render "${TARGET_PLATFORM}"
  testmain
  dlib
  zip
  mbedtls
  ddf
  script
  extension
  resource
  profile_null
  platform_null
  lua
  graphics_null
  image
  graphics_proto
  font
  render
  render_font_default
  profilerext)
defold_target_link_socket(test_profilerext_render "${TARGET_PLATFORM}" PRIVATE)
defold_target_link_app(test_profilerext_render "${TARGET_PLATFORM}" SCOPE PRIVATE)
defold_target_link_libraries_web(test_profilerext_render "${TARGET_PLATFORM}"
  library_sys.js
  library_script.js
  library_render.js)
if(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
  find_package(Threads REQUIRED)
  target_link_libraries(test_profilerext_render PRIVATE Threads::Threads dl)
endif()
defold_register_test_target(test_profilerext_render ON "${PROFILER_ROOT}")
