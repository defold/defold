cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

set(RECORD_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(RECORD_ROOT "${RECORD_TEST_ROOT}/../.." ABSOLUTE)
set(RECORD_SRC_DIR "${RECORD_ROOT}/src")

if(TARGET record)
  set(RECORD_TEST_LIB record)
  set(RECORD_TEST_EXTRA_LIBS vpx)
  set(RECORD_TEST_DEFINES)
else()
  set(RECORD_TEST_LIB record_null)
  set(RECORD_TEST_EXTRA_LIBS)
  set(RECORD_TEST_DEFINES DM_RECORD_NULL)
endif()

if(TARGET_PLATFORM MATCHES "ios$")
  set(RECORD_TEST_DEFINES DM_NO_SYSTEM_FUNCTION)
endif()

defold_add_executable(test_record "${RECORD_TEST_ROOT}/test_record.cpp")
target_include_directories(test_record PRIVATE "${RECORD_SRC_DIR}" "${RECORD_TEST_ROOT}")
target_compile_definitions(test_record PRIVATE ${RECORD_TEST_DEFINES})
defold_target_link_libraries(test_record "${TARGET_PLATFORM}"
  testmain
  dlib
  profile_null
  ${RECORD_TEST_EXTRA_LIBS}
  ${RECORD_TEST_LIB})
if(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
  find_package(Threads REQUIRED)
  target_link_libraries(test_record PRIVATE Threads::Threads dl)
endif()
defold_register_test_target(test_record ON "${RECORD_ROOT}")
