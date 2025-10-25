cmake_minimum_required(VERSION 4.0)

# Early toolchain/SDK detection (for standalone configure)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

#================================ Test Data ================================

defold_protoc_encode(
  "${CMAKE_CURRENT_BINARY_DIR}/build/test.input_bindingc"           # OUT_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/test.input_binding"                  # SRC_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../proto/input/input_ddf.proto"   # SCHEMA_PROTO
  "dmInputDDF.InputBinding"                                         # MESSAGE_NAME
  INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/../../proto"
)

defold_protoc_encode(
  "${CMAKE_CURRENT_BINARY_DIR}/build/test2.input_bindingc"          # OUT_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/test2.input_binding"                 # SRC_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../proto/input/input_ddf.proto"   # SCHEMA_PROTO
  "dmInputDDF.InputBinding"                                         # MESSAGE_NAME
  INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/../../proto"
)

defold_protoc_encode(
  "${CMAKE_CURRENT_BINARY_DIR}/build/test_text.input_bindingc"      # OUT_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/test_text.input_binding"             # SRC_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../proto/input/input_ddf.proto"   # SCHEMA_PROTO
  "dmInputDDF.InputBinding"                                         # MESSAGE_NAME
  INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/../../proto"
)

defold_protoc_encode(
  "${CMAKE_CURRENT_BINARY_DIR}/build/combinations.input_bindingc"   # OUT_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/combinations.input_binding"          # SRC_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../proto/input/input_ddf.proto"   # SCHEMA_PROTO
  "dmInputDDF.InputBinding"                                         # MESSAGE_NAME
  INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/../../proto"
)

defold_protoc_encode(
  "${CMAKE_CURRENT_BINARY_DIR}/build/test.gamepadsc"                # OUT_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/test.gamepads"                       # SRC_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../proto/input/input_ddf.proto"   # SCHEMA_PROTO
  "dmInputDDF.GamepadMaps"                                          # MESSAGE_NAME
  INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/../../proto"
)

#===========================================================================

# Build the input test executable
defold_add_executable(test_input "${CMAKE_CURRENT_LIST_DIR}/test_input.cpp")

# make sure the test data is built
add_custom_target(input_test_data DEPENDS
    "${CMAKE_CURRENT_BINARY_DIR}/build/test.input_bindingc"
    "${CMAKE_CURRENT_BINARY_DIR}/build/test2.input_bindingc"
    "${CMAKE_CURRENT_BINARY_DIR}/build/test_text.input_bindingc"
    "${CMAKE_CURRENT_BINARY_DIR}/build/combinations.input_bindingc"
    "${CMAKE_CURRENT_BINARY_DIR}/build/test.gamepadsc")
add_dependencies(input input_test_data)


# Include parent binary dir for generated headers (e.g. input/input_ddf.h)
target_include_directories(test_input PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/../../"
)

defold_target_link_libraries(test_input "${TARGET_PLATFORM}"
    dlib
    mbedtls
    ddf
    profile_null
    testmain
    hid_null
    input
)

defold_target_link_platform(test_input "${TARGET_PLATFORM}" PRIVATE)
defold_target_link_socket(test_input "${TARGET_PLATFORM}" PRIVATE)
defold_target_link_glfw(test_input "${TARGET_PLATFORM}" PRIVATE)

# Platform-specific libs
if (TARGET_PLATFORM MATCHES "arm64-android|armv7-android")
    # nothing extra for now
elseif (TARGET_PLATFORM MATCHES "arm64-ios|x86_64-ios")
    target_link_libraries(test_input PRIVATE "-framework IOKit")
elseif (TARGET_PLATFORM MATCHES "arm64-macos|x86_64-macos")
    target_link_libraries(test_input PRIVATE "-framework IOKit")
elseif (TARGET_PLATFORM MATCHES "x86_64-win32|win32")
    target_link_libraries(test_input PRIVATE user32 gdi32 shell32)
elseif (TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
    find_package(Threads REQUIRED)
    target_link_libraries(test_input PRIVATE Threads::Threads dl)
endif()

# Graphics and platform linkages
defold_target_link_graphics(test_input "${TARGET_PLATFORM}" PRIVATE)
defold_target_link_platform(test_input "${TARGET_PLATFORM}" PRIVATE)

# Register into global build_tests and run_tests with working directory set to engine/input
defold_register_test_target(test_input ON "${CMAKE_CURRENT_LIST_DIR}/../..")
