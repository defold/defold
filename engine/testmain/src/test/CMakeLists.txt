cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

set(TESTMAIN_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(TESTMAIN_ROOT "${TESTMAIN_TEST_ROOT}/../..")

defold_add_executable(test_testmain "${TESTMAIN_TEST_ROOT}/test_testmain.cpp")
target_include_directories(test_testmain PRIVATE "${TESTMAIN_TEST_ROOT}/..")
target_link_libraries(test_testmain PRIVATE testmain)
defold_register_test_target(test_testmain ON "${TESTMAIN_ROOT}")
