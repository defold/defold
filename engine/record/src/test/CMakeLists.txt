cmake_minimum_required(VERSION 4.0)

set(RECORD_TEST_RUN ON)
if(TARGET_PLATFORM MATCHES "ios$")
  set(RECORD_TEST_RUN OFF)
endif()

function(record_test name)
  defold_add_executable(${name} ${ARGN})
  target_compile_definitions(${name} PRIVATE DLIB_LOG_DOMAIN="RECORD_TEST")
  target_include_directories(${name} PRIVATE "${RECORD_ROOT}/src")
  defold_target_link_libraries(${name} "${TARGET_PLATFORM}" testmain dlib profile_null)
  if(TARGET_PLATFORM MATCHES "linux$")
    find_package(Threads REQUIRED)
    target_link_libraries(${name} PRIVATE Threads::Threads dl)
  endif()
  defold_register_test_target(${name} ${RECORD_TEST_RUN} "${RECORD_ROOT}")
endfunction()

# No permission prompt or native window is needed to test lifecycle races.
if(TARGET_PLATFORM MATCHES "(macos|linux|win32)$")
  record_test(test_record test_record.cpp ../record/record.cpp ../record/record_common.cpp)
  record_test(test_record_file test_record_file.cpp ../record/record_file.cpp ../record/record_common.cpp)
  if(TARGET record_script)
    record_test(test_record_script test_record_script.cpp
      ../record/record.cpp ../record/record_common.cpp)
    defold_target_link_libraries(test_record_script "${TARGET_PLATFORM}"
      record_script script resource extension ddf graphics_null platform_null image zip mbedtls)
    defold_target_link_lua(test_record_script "${TARGET_PLATFORM}")
    defold_target_link_socket(test_record_script "${TARGET_PLATFORM}")
    defold_target_link_app(test_record_script "${TARGET_PLATFORM}")
  endif()
endif()
record_test(test_record_timeline test_record_timeline.cpp ../record/record_timeline.cpp)

if(TARGET record AND TARGET_PLATFORM MATCHES "macos$")
  # Exercise native output path handling without a window or capture permission.
  record_test(test_record_osx test_record_osx.cpp)
  defold_target_link_libraries(test_record_osx "${TARGET_PLATFORM}" record)
  record_test(test_record_osx_native test_record_osx_native.mm)
  defold_target_link_libraries(test_record_osx_native "${TARGET_PLATFORM}" record)
endif()

if(TARGET record AND TARGET_PLATFORM MATCHES "linux$")
  # Real H.264 encode/decode and MP4 validation with a deterministic framebuffer;
  # no display server or capture permission is needed on CI.
  record_test(test_record_linux test_record_linux.cpp)
  target_include_directories(test_record_linux PRIVATE "${DEFOLD_HOME}/external/minimp4")
  defold_target_link_libraries(test_record_linux "${TARGET_PLATFORM}" record)
endif()

if(TARGET record AND TARGET_PLATFORM MATCHES "win32$")
  # Headless capability checks with controlled WinRT activation factories.
  record_test(test_record_win32_support test_record_win32_support.cpp)
  target_compile_features(test_record_win32_support PRIVATE cxx_std_17)
  defold_target_link_libraries(test_record_win32_support "${TARGET_PLATFORM}" record)

  # Explicit opt-in: opens a test window and plays a tone on the default device.
  # Keep it out of run_tests/build_tests on headless CI workers.
  defold_add_executable(test_record_win32_native test_record_win32.cpp)
  target_compile_features(test_record_win32_native PRIVATE cxx_std_17)
  target_include_directories(test_record_win32_native PRIVATE "${RECORD_ROOT}/src")
  defold_target_link_libraries(test_record_win32_native "${TARGET_PLATFORM}" record dlib profile_null)
  target_link_libraries(test_record_win32_native PRIVATE mfplat mfreadwrite mfuuid ole32 winmm user32 gdi32 dwmapi)
  set_target_properties(test_record_win32_native PROPERTIES EXCLUDE_FROM_ALL TRUE)
  _defold_find_python(record_native_python)
  add_custom_target(run_test_record_win32_native
    COMMAND "${record_native_python}" "${CMAKE_CURRENT_SOURCE_DIR}/test_win32.py"
      --test "$<TARGET_FILE:test_record_win32_native>"
    DEPENDS test_record_win32_native VERBATIM)
endif()

if(TARGET_PLATFORM STREQUAL HOST_PLATFORM AND TARGET_PLATFORM MATCHES "(macos|linux|win32)$")
  defold_add_executable(record_test_server record_test_server.cpp
    ../record/record.cpp ../record/record_common.cpp ../record/record_service.cpp ../record/record_file.cpp
    ../record/record_extension.cpp)
  target_include_directories(record_test_server PRIVATE "${RECORD_ROOT}/src")
  target_compile_definitions(record_test_server PRIVATE DLIB_LOG_DOMAIN="RECORD_TEST")
  defold_target_link_libraries(record_test_server "${TARGET_PLATFORM}" extension dlib profile_null)
  if(TARGET_PLATFORM MATCHES "linux$")
    target_link_libraries(record_test_server PRIVATE Threads::Threads dl)
  endif()
  set_target_properties(record_test_server PROPERTIES EXCLUDE_FROM_ALL TRUE)
  add_dependencies(build_tests record_test_server)
  _defold_find_python(record_python)
  set(record_service_command "${record_python}" "${CMAKE_CURRENT_SOURCE_DIR}/test_service.py"
    --server "$<TARGET_FILE:record_test_server>")
  add_test(NAME record_service COMMAND ${record_service_command})
  set_tests_properties(record_service PROPERTIES TIMEOUT 60)
  defold_test_run_settings(record_runner record_run_options record)
  add_custom_target(run_test_record_service
    COMMAND ${record_runner} ${record_service_command}
    DEPENDS record_test_server VERBATIM ${record_run_options})
  defold_register_sequential_test_command(record_service
    COMMAND ${record_service_command} DEPENDS record_test_server)
  if(NOT CMAKE_GENERATOR STREQUAL "Xcode")
    defold_add_to_run_tests(run_test_record_service RUN_GROUP record
      COMMAND ${record_service_command} DEPENDS record_test_server)
  endif()
endif()
