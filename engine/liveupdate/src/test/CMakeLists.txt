cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

set(LIVEUPDATE_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(LIVEUPDATE_ROOT "${LIVEUPDATE_TEST_ROOT}/../.." ABSOLUTE)
set(LIVEUPDATE_SRC_DIR "${LIVEUPDATE_ROOT}/src")

set(LIVEUPDATE_TEST_LIBS
  testmain
  dlib
  ddf
  extension
  profile_null
  resource
  script
  graphics_null
  image
  graphics_proto
  platform_null
  lua)

set(LIVEUPDATE_TEST_EXPORTS LiveUpdateExt ResourceProviderFile)
set(LIVEUPDATE_TEST_EXPORT_FILE "${CMAKE_CURRENT_BINARY_DIR}/liveupdate_test_exported_symbols.cpp")
defold_create_exported_symbols_file("${LIVEUPDATE_TEST_EXPORT_FILE}" "${LIVEUPDATE_TEST_EXPORTS}")

function(liveupdate_add_test target thread_count impl)
  set(_sources "${LIVEUPDATE_TEST_ROOT}/test_liveupdate_job.cpp" "${LIVEUPDATE_TEST_EXPORT_FILE}")
  defold_add_executable(${target} ${_sources})
  target_include_directories(${target} PRIVATE "${LIVEUPDATE_SRC_DIR}")
  target_compile_definitions(${target} PRIVATE DM_TEST_THREAD_COUNT=${thread_count})
  set(_run_test ON)
  if(impl STREQUAL "null")
    target_compile_definitions(${target} PRIVATE DM_LU_NULL_IMPLEMENTATION)
    set(_liveupdate_lib liveupdate_null)
    set(_run_test OFF)
  else()
    set(_liveupdate_lib liveupdate)
  endif()
  defold_target_link_libraries(${target} "${TARGET_PLATFORM}" ${LIVEUPDATE_TEST_LIBS} ${_liveupdate_lib})
  defold_target_link_socket(${target} "${TARGET_PLATFORM}" PRIVATE)
  defold_target_link_libraries_web(${target} "${TARGET_PLATFORM}" library_sys.js)
  if(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
    find_package(Threads REQUIRED)
    target_link_libraries(${target} PRIVATE Threads::Threads dl)
  endif()
  defold_register_test_target(${target} ${_run_test} "${LIVEUPDATE_ROOT}")
endfunction()

liveupdate_add_test(test_liveupdate_job_multi 1 "")
liveupdate_add_test(test_liveupdate_job_multi_null 1 "null")
liveupdate_add_test(test_liveupdate_job_single 0 "")
liveupdate_add_test(test_liveupdate_job_single_null 0 "null")
