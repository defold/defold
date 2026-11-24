cmake_minimum_required(VERSION 4.0)

# Early toolchain/SDK detection (for standalone configure)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

#******************************* Test Data *******************************

set(INPUT_TEST_RUNTIME_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")
set(INPUT_TEST_BUILD_DIR "${INPUT_TEST_RUNTIME_DIR}/build/src/test/build")

defold_protoc_encode(
  "${INPUT_TEST_BUILD_DIR}/test.input_bindingc"                    # OUT_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/test.input_binding"                  # SRC_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../proto/input/input_ddf.proto"   # SCHEMA_PROTO
  "dmInputDDF.InputBinding"                                         # MESSAGE_NAME
  INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/../../proto"
)

defold_protoc_encode(
  "${INPUT_TEST_BUILD_DIR}/test2.input_bindingc"                   # OUT_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/test2.input_binding"                 # SRC_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../proto/input/input_ddf.proto"   # SCHEMA_PROTO
  "dmInputDDF.InputBinding"                                         # MESSAGE_NAME
  INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/../../proto"
)

defold_protoc_encode(
  "${INPUT_TEST_BUILD_DIR}/test_text.input_bindingc"               # OUT_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/test_text.input_binding"             # SRC_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../proto/input/input_ddf.proto"   # SCHEMA_PROTO
  "dmInputDDF.InputBinding"                                         # MESSAGE_NAME
  INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/../../proto"
)

defold_protoc_encode(
  "${INPUT_TEST_BUILD_DIR}/combinations.input_bindingc"            # OUT_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/combinations.input_binding"          # SRC_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../proto/input/input_ddf.proto"   # SCHEMA_PROTO
  "dmInputDDF.InputBinding"                                         # MESSAGE_NAME
  INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/../../proto"
)

defold_protoc_encode(
  "${INPUT_TEST_BUILD_DIR}/test.gamepadsc"                         # OUT_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/test.gamepads"                       # SRC_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../proto/input/input_ddf.proto"   # SCHEMA_PROTO
  "dmInputDDF.GamepadMapsRuntime"                                   # MESSAGE_NAME
  INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/../../proto"
)

#***********************************************************************

# Build separate C++ and C framework executables.
defold_add_executable(test_input "${CMAKE_CURRENT_LIST_DIR}/test_input.cpp")
defold_add_executable(test_input_api
    "${CMAKE_CURRENT_LIST_DIR}/test_input_api.c"
    "${CMAKE_CURRENT_LIST_DIR}/test_input_api.cpp")
target_compile_features(test_input_api PRIVATE c_std_11)

# make sure the test data is built
add_custom_target(input_test_data DEPENDS
    "${INPUT_TEST_BUILD_DIR}/test.input_bindingc"
    "${INPUT_TEST_BUILD_DIR}/test2.input_bindingc"
    "${INPUT_TEST_BUILD_DIR}/test_text.input_bindingc"
    "${INPUT_TEST_BUILD_DIR}/combinations.input_bindingc"
    "${INPUT_TEST_BUILD_DIR}/test.gamepadsc")

function(input_configure_test target)
  add_dependencies(${target} input_test_data)

  # Include parent binary dir for generated headers (e.g. input/input_ddf.h)
  target_include_directories(${target} PRIVATE
      "${CMAKE_CURRENT_BINARY_DIR}/../../"
  )

  defold_target_link_libraries(${target} "${TARGET_PLATFORM}"
      dlib
      mbedtls
      ddf
      profile_null
      testmain
      hid_null
      hid_test
      input
  )

  target_link_libraries(${target} PRIVATE platform_null)
  defold_target_link_socket(${target} "${TARGET_PLATFORM}" PRIVATE)
  defold_target_link_glfw(${target} "${TARGET_PLATFORM}" PRIVATE)

  # Platform-specific libs
  if (TARGET_PLATFORM MATCHES "arm64-android|armv7-android|x86_64-android")
      # nothing extra for now
  elseif (TARGET_PLATFORM MATCHES "arm64-ios|arm64_sim-ios")
      target_link_libraries(${target} PRIVATE "-framework IOKit")
  elseif (TARGET_PLATFORM MATCHES "arm64-macos|x86_64-macos")
      target_link_libraries(${target} PRIVATE "-framework IOKit")
  elseif (TARGET_PLATFORM STREQUAL "x86_64-win32")
      target_link_libraries(${target} PRIVATE user32 gdi32 shell32)
  elseif (TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
      find_package(Threads REQUIRED)
      target_link_libraries(${target} PRIVATE Threads::Threads dl)
  endif()

  # Register into global build_tests and run_tests with working directory set to engine/input
  defold_register_test_target(${target} ON "${INPUT_TEST_RUNTIME_DIR}"
      STAGE_FILES
          "build/src/test/build" "build/src/test/build"
  )
endfunction()

input_configure_test(test_input)
input_configure_test(test_input_api)
