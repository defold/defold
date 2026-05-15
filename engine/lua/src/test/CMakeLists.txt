cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

set(LUA_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(LUA_ROOT "${LUA_TEST_ROOT}/../..")

if(TARGET_PLATFORM MATCHES "^(x86_64-macos|arm64-macos)$")
  defold_csharp_nativeaot_runtime_available(_lua_csharp_runtime_available "${TARGET_PLATFORM}")
  if(_lua_csharp_runtime_available)
    defold_add_csharp_static_library(test_lua_cs
      PROJECT "${LUA_TEST_ROOT}/cs/test_lua_cs.csproj"
      OUTPUT_VAR LUA_TEST_CS_LIB)
  else()
    message(STATUS "Skipping test_lua_csharp; NativeAOT runtime pack is not available locally for ${TARGET_PLATFORM}")
  endif()

  if(TARGET test_lua_cs)
    defold_add_executable(test_lua_csharp "${LUA_TEST_ROOT}/test_lua_csharp.cpp")
    target_include_directories(test_lua_csharp PRIVATE
      "${LUA_ROOT}/src"
      "${LUA_TEST_ROOT}")
    defold_target_link_libraries(test_lua_csharp "${TARGET_PLATFORM}"
      testmain
      dlib
      profile_null)
    target_link_libraries(test_lua_csharp PRIVATE test_lua_cs)
    defold_target_link_lua(test_lua_csharp "${TARGET_PLATFORM}")
    defold_target_link_socket(test_lua_csharp "${TARGET_PLATFORM}" PRIVATE)
    defold_target_link_csharp_runtime(test_lua_csharp "${TARGET_PLATFORM}")
    defold_register_test_target(test_lua_csharp ON "${LUA_ROOT}")
  endif()
else()
  message(STATUS "Skipping test_lua_csharp for ${TARGET_PLATFORM}; NativeAOT test is currently enabled for macOS desktop targets")
endif()
