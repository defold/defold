cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../../scripts/cmake")
include(defold)

set(TOOLS_LAUNCHER_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(TOOLS_ROOT "${TOOLS_LAUNCHER_TEST_ROOT}/../../../.." ABSOLUTE)
set(TOOLS_LAUNCHER_SRC_DIR "${TOOLS_ROOT}/src/launcher")

defold_add_executable(test_launcher "${TOOLS_LAUNCHER_TEST_ROOT}/test_launcher.cpp")
target_include_directories(test_launcher PRIVATE
  "${TOOLS_LAUNCHER_SRC_DIR}"
  "${TOOLS_LAUNCHER_TEST_ROOT}")
defold_target_link_libraries(test_launcher "${TARGET_PLATFORM}"
  testmain
  dlib
  zip
  mbedtls
  profile_null
  platform_null
  launcherutil)
if(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
  find_package(Threads REQUIRED)
  target_link_libraries(test_launcher PRIVATE Threads::Threads dl)
endif()
defold_register_test_target(test_launcher ON "${TOOLS_ROOT}")
