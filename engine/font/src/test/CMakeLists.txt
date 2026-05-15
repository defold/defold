cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

set(FONT_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(FONT_ROOT "${FONT_TEST_ROOT}/../..")
set(FONT_SRC_DIR "${FONT_ROOT}/src")

function(font_configure_test target font_target)
  target_include_directories(${target} PRIVATE
    "${FONT_TEST_ROOT}"
    "${FONT_SRC_DIR}"
    "${FONT_ROOT}")
  defold_target_link_libraries(${target} "${TARGET_PLATFORM}"
    testmain
    dlib
    profile_null
    ${font_target})
  defold_target_link_socket(${target} "${TARGET_PLATFORM}" PRIVATE)
  defold_register_test_target(${target} ON "${FONT_ROOT}"
    STAGE_FILES
      "src/test/data" "src/test/data")
endfunction()

defold_add_executable(test_font "${FONT_TEST_ROOT}/test_font.cpp")
font_configure_test(test_font font)

defold_add_executable(test_font_skribidi "${FONT_TEST_ROOT}/test_font.cpp")
target_compile_definitions(test_font_skribidi PRIVATE FONT_USE_HARFBUZZ FONT_USE_SKRIBIDI)
font_configure_test(test_font_skribidi font_skribidi)
