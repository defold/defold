cmake_minimum_required(VERSION 4.0)

set(RECORD_TEST_RUN ON)
if(TARGET_PLATFORM MATCHES "ios$")
  set(RECORD_TEST_RUN OFF)
endif()

function(record_test name)
  defold_add_executable(${name} ${ARGN})
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
endif()
record_test(test_record_null test_record_null.cpp)
defold_target_link_libraries(test_record_null "${TARGET_PLATFORM}" record_null)

if(TARGET_PLATFORM STREQUAL HOST_PLATFORM AND TARGET_PLATFORM MATCHES "(macos|linux|win32)$")
  defold_add_executable(record_test_server record_test_server.cpp
    ../record/record.cpp ../record/record_common.cpp ../record/record_service.cpp)
  target_include_directories(record_test_server PRIVATE "${RECORD_ROOT}/src")
  defold_target_link_libraries(record_test_server "${TARGET_PLATFORM}" dlib profile_null)
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
