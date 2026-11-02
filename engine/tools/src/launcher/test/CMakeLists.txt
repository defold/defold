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

if(TARGET_PLATFORM MATCHES "macos|linux")
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  if(TARGET_PLATFORM MATCHES "macos")
    set(TOOLS_LAUNCHER_PLATFORM_TEST test_launcher_macos)
  else()
    set(TOOLS_LAUNCHER_PLATFORM_TEST test_launcher_linux)
  endif()
  set(TOOLS_LAUNCHER_PLATFORM_TEST_COMMAND
    "${Python3_EXECUTABLE}" "${TOOLS_LAUNCHER_TEST_ROOT}/${TOOLS_LAUNCHER_PLATFORM_TEST}.py" "$<TARGET_FILE:launcher>")
  defold_test_run_settings(_launcher_test_runner _launcher_test_run_options shared)
  add_custom_target(run_${TOOLS_LAUNCHER_PLATFORM_TEST}
    COMMAND ${_launcher_test_runner} ${TOOLS_LAUNCHER_PLATFORM_TEST_COMMAND}
    DEPENDS launcher
    ${_launcher_test_run_options}
    VERBATIM)
  if(NOT CMAKE_GENERATOR STREQUAL "Xcode")
    defold_add_to_run_tests(run_${TOOLS_LAUNCHER_PLATFORM_TEST}
      COMMAND ${TOOLS_LAUNCHER_PLATFORM_TEST_COMMAND}
      DEPENDS launcher)
  endif()
  defold_register_sequential_test_command(${TOOLS_LAUNCHER_PLATFORM_TEST}
    COMMAND ${TOOLS_LAUNCHER_PLATFORM_TEST_COMMAND}
    DEPENDS launcher)
endif()
