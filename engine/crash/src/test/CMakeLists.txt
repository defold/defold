cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

set(CRASH_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(CRASH_ROOT "${CRASH_TEST_ROOT}/../.." ABSOLUTE)
set(CRASH_SRC_DIR "${CRASH_ROOT}/src")
set(CRASH_TEST_RUNTIME_DIR "${CRASH_ROOT}/build/src/test")

add_custom_target(crash_test_lua_data
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CRASH_TEST_RUNTIME_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CRASH_TEST_ROOT}/test_crash.lua" "${CRASH_TEST_RUNTIME_DIR}/test_crash.luac"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CRASH_TEST_ROOT}/test_crash_null.lua" "${CRASH_TEST_RUNTIME_DIR}/test_crash_null.luac"
  DEPENDS "${CRASH_TEST_ROOT}/test_crash.lua" "${CRASH_TEST_ROOT}/test_crash_null.lua"
  VERBATIM)

set(CRASH_TEST_LIBS
  testmain
  script
  extension
  resource
  ddf
  dlib
  image
  zip
  mbedtls
  profile_null
  lua
  graphics_null
  graphics_proto
  platform_null)
if(TARGET_PLATFORM MATCHES "android")
  list(APPEND CRASH_TEST_LIBS unwind)
endif()
if(TARGET_PLATFORM MATCHES "^(x86_64-ps4|x86_64-ps5|x86_64-xbone)$")
  set(CRASH_RUN_TESTS OFF)
else()
  set(CRASH_RUN_TESTS ON)
endif()

function(crash_configure_test target crash_lib run_test)
  target_include_directories(${target} PRIVATE "${CRASH_SRC_DIR}")
  defold_target_link_libraries(${target} "${TARGET_PLATFORM}" ${CRASH_TEST_LIBS} ${crash_lib})
  defold_target_link_socket(${target} "${TARGET_PLATFORM}" PRIVATE)
  defold_target_link_libraries_web(${target} "${TARGET_PLATFORM}" library_sys.js library_script.js)
  if(TARGET_PLATFORM MATCHES "win32$")
    target_link_libraries(${target} PRIVATE Psapi.lib)
  elseif(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
    find_package(Threads REQUIRED)
    target_link_libraries(${target} PRIVATE Threads::Threads dl)
  endif()
  defold_register_test_target(${target} ${run_test} "${CRASH_ROOT}"
    STAGE_FILES
      "build/src/test" "build/src/test")
  add_dependencies(${target} crash_test_lua_data)
  if(TARGET run_${target})
    add_dependencies(run_${target} crash_test_lua_data)
  endif()
endfunction()

defold_add_executable(test_crash "${CRASH_TEST_ROOT}/test_crash.cpp")
crash_configure_test(test_crash crashext ${CRASH_RUN_TESTS})

defold_add_executable(test_crash_null "${CRASH_TEST_ROOT}/test_crash.cpp")
target_compile_definitions(test_crash_null PRIVATE DM_CRASH_TEST_NULL)
crash_configure_test(test_crash_null crashext_null OFF)

set(CRASH_TEST_EXPORTS CrashExt ResourceProviderFile)
set(CRASH_TEST_EXPORT_FILE "${CMAKE_CURRENT_BINARY_DIR}/crash_test_exported_symbols.cpp")
defold_create_exported_symbols_file("${CRASH_TEST_EXPORT_FILE}" "${CRASH_TEST_EXPORTS}")

defold_add_executable(test_script_crash
  "${CRASH_TEST_ROOT}/test_script_crash.cpp"
  "${CRASH_TEST_EXPORT_FILE}")
crash_configure_test(test_script_crash crashext ${CRASH_RUN_TESTS})

defold_add_executable(test_script_crash_null
  "${CRASH_TEST_ROOT}/test_script_crash_null.cpp"
  "${CRASH_TEST_EXPORT_FILE}")
crash_configure_test(test_script_crash_null crashext_null ${CRASH_RUN_TESTS})
