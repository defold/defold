cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

set(RIG_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(RIG_ROOT "${RIG_TEST_ROOT}/../..")
set(RIG_SRC_DIR "${RIG_ROOT}/src")

defold_add_executable(test_rig "${RIG_TEST_ROOT}/test_rig.cpp")
target_include_directories(test_rig PRIVATE
  "${RIG_TEST_ROOT}"
  "${RIG_SRC_DIR}"
  "${RIG_ROOT}/proto"
  "${CMAKE_CURRENT_BINARY_DIR}/../..")
defold_target_link_libraries(test_rig "${TARGET_PLATFORM}"
  testmain
  dlib
  mbedtls
  profile_null
  lua
  script
  platform_null
  graphics_null
  image
  graphics_proto
  rig)
defold_target_link_socket(test_rig "${TARGET_PLATFORM}" PRIVATE)
if(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
  find_package(Threads REQUIRED)
  target_link_libraries(test_rig PRIVATE Threads::Threads dl)
endif()

defold_register_test_target(test_rig ON "${RIG_ROOT}")
