cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

set(EXTENSION_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(EXTENSION_ROOT "${EXTENSION_TEST_ROOT}/../..")
set(EXTENSION_SRC_DIR "${EXTENSION_ROOT}/src")

defold_add_library(test_extension_lib STATIC "${EXTENSION_TEST_ROOT}/test_extension_lib.cpp")
target_include_directories(test_extension_lib PUBLIC
  "${EXTENSION_TEST_ROOT}"
  "${EXTENSION_SRC_DIR}")
defold_target_link_libraries(test_extension_lib "${TARGET_PLATFORM}" ddf dlib)

set(_extension_exports TestExt)
defold_create_exported_symbols_file("${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols.cpp" "${_extension_exports}")

defold_add_executable(test_extension
  "${EXTENSION_TEST_ROOT}/test_extension.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols.cpp")
target_include_directories(test_extension PRIVATE
  "${EXTENSION_TEST_ROOT}"
  "${EXTENSION_SRC_DIR}")
defold_target_link_libraries(test_extension "${TARGET_PLATFORM}"
  testmain
  dlib
  profile_null
  lua
  extension
  test_extension_lib)
defold_target_link_socket(test_extension "${TARGET_PLATFORM}" PRIVATE)
if(TARGET extension_cs)
  target_link_libraries(test_extension PRIVATE extension_cs)
  defold_target_link_csharp_runtime(test_extension "${TARGET_PLATFORM}")
endif()
if(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
  find_package(Threads REQUIRED)
  target_link_libraries(test_extension PRIVATE Threads::Threads dl)
endif()

defold_register_test_target(test_extension ON "${EXTENSION_ROOT}")
