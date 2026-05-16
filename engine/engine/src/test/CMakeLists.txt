cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

set(ENGINE_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(ENGINE_TEST_ENGINE_ROOT "${ENGINE_TEST_ROOT}/../.." ABSOLUTE)

set(ENGINE_PROFILEREXT_LUA_EXPORTS
  ProfilerExt
  ProfilerBasic
  GraphicsAdapterNull)
set(ENGINE_PROFILEREXT_LUA_EXPORTS_CPP "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols_test_profilerext_lua.cpp")
defold_create_exported_symbols_file("${ENGINE_PROFILEREXT_LUA_EXPORTS_CPP}" "${ENGINE_PROFILEREXT_LUA_EXPORTS}")

defold_add_executable(test_profilerext_lua
  "${ENGINE_TEST_ROOT}/test_profilerext_lua.cpp"
  "${ENGINE_PROFILEREXT_LUA_EXPORTS_CPP}")
target_include_directories(test_profilerext_lua PRIVATE
  "${ENGINE_TEST_ROOT}"
  "${ENGINE_TEST_ENGINE_ROOT}/src"
  "${ENGINE_TEST_ENGINE_ROOT}/proto")
defold_target_link_libraries(test_profilerext_lua "${TARGET_PLATFORM}"
  testmain
  resource
  ddf
  font
  graphics_null
  image
  graphics_transcoder_null
  platform_null
  render
  render_font_default
  script
  extension
  dlib
  zip
  mbedtls
  profile
  profilerext
  lua)
defold_target_link_lua(test_profilerext_lua "${TARGET_PLATFORM}")
defold_target_link_socket(test_profilerext_lua "${TARGET_PLATFORM}")
defold_target_link_app(test_profilerext_lua "${TARGET_PLATFORM}")
if(TARGET_PLATFORM MATCHES "win32")
  target_link_libraries(test_profilerext_lua PRIVATE Psapi.lib)
endif()

defold_register_test_target(test_profilerext_lua ON "${ENGINE_TEST_ENGINE_ROOT}")
