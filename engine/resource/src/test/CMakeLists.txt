cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

set(RESOURCE_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(RESOURCE_ROOT "${RESOURCE_TEST_ROOT}/../..")
set(RESOURCE_SRC_DIR "${RESOURCE_ROOT}/src")

function(resource_configure_test target run_test)
  set(_resource_exports ${ARGN})
  if(_resource_exports)
    set(_resource_export_file "${CMAKE_CURRENT_BINARY_DIR}/${target}_exported_symbols.cpp")
    defold_create_exported_symbols_file("${_resource_export_file}" "${_resource_exports}")
    target_sources(${target} PRIVATE "${_resource_export_file}")
  endif()
  target_include_directories(${target} PRIVATE
    "${RESOURCE_TEST_ROOT}"
    "${RESOURCE_SRC_DIR}"
    "${RESOURCE_ROOT}/proto"
    "${CMAKE_CURRENT_BINARY_DIR}/../..")
  defold_target_link_libraries(${target} "${TARGET_PLATFORM}"
    testmain
    ddf
    dlib
    mbedtls
    profile_null
    lua
    resource)
  defold_target_link_socket(${target} "${TARGET_PLATFORM}" PRIVATE)
  if(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
    find_package(Threads REQUIRED)
    target_link_libraries(${target} PRIVATE Threads::Threads dl)
  endif()
  defold_register_test_target(${target} ${run_test} "${RESOURCE_ROOT}"
    STAGE_FILES
      "src/test/archive_data" "src/test/archive_data"
      "src/test/files" "src/test/files"
      "src/test/mountfiles" "src/test/mountfiles"
      "src/test/overrides" "src/test/overrides"
      "src/test/zipfiles" "src/test/zipfiles"
      "build/src/test" "build/src/test")
endfunction()

defold_add_executable(test_block_allocator "${RESOURCE_TEST_ROOT}/test_block_allocator.cpp")
resource_configure_test(test_block_allocator ON)

defold_add_executable(test_provider_file "${RESOURCE_TEST_ROOT}/test_provider_file.cpp")
resource_configure_test(test_provider_file ON ResourceProviderFile)

defold_add_executable(test_provider_zip "${RESOURCE_TEST_ROOT}/test_provider_zip.cpp")
resource_configure_test(test_provider_zip ON ResourceProviderZip)

defold_add_executable(test_provider_multi "${RESOURCE_TEST_ROOT}/test_provider_multi.cpp")
resource_configure_test(test_provider_multi ON ResourceProviderArchive ResourceProviderZip ResourceProviderFile)

defold_add_executable(test_resource_chunk_cache "${RESOURCE_TEST_ROOT}/test_resource_chunk_cache.cpp")
resource_configure_test(test_resource_chunk_cache ON ResourceProviderArchive)
