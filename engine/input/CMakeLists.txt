# Minimum CMake version and project
cmake_minimum_required(VERSION 4.0)

# Early toolchain/SDK detection (for standalone configure)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_input LANGUAGES ${DEFOLD_LANGUAGE_LIST})

# Define log domain for this module
add_compile_definitions(DLIB_LOG_DOMAIN="INPUT")

# Sources
set(INPUT_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/src")

defold_protoc_gen_py("${CMAKE_CURRENT_BINARY_DIR}/python/input/input_ddf_pb2.py"
                          "${CMAKE_CURRENT_SOURCE_DIR}/proto/input/input_ddf.proto"
                          INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/proto" "${DEFOLD_SDK_ROOT}/share/proto")
add_custom_target(input_proto_py DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/python/input/input_ddf_pb2.py")

defold_protoc_gen_cpp("${CMAKE_CURRENT_BINARY_DIR}/input/input_ddf.pb.cc"
                          "${CMAKE_CURRENT_SOURCE_DIR}/proto/input/input_ddf.proto"
                          INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/proto" "${DEFOLD_SDK_ROOT}/share/proto")
add_custom_target(input_proto_headers
    DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/input/input_ddf.pb.cc"
        "${CMAKE_CURRENT_BINARY_DIR}/input/input_ddf.h")

add_library(input STATIC
    "${INPUT_SRC_DIR}/input.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/input/input_ddf.pb.cc"
)
target_compile_definitions(input PRIVATE "DLIB_LOG_DOMAIN=\"INPUT\"")

target_include_directories(input PUBLIC ${INPUT_SRC_DIR} ${CMAKE_CURRENT_BINARY_DIR})
# just to make sure the extra dependency is actually built
add_dependencies(input input_proto_headers input_proto_py)
add_dependencies(input hid_sdk_headers)

# Add GLFW3-specific compile define when applicable
defold_get_glfw_version(_GLFW_VERSION "${TARGET_PLATFORM}")
if (_GLFW_VERSION EQUAL 3)
    target_compile_definitions(input PUBLIC DM_INPUT_USE_GLFW3)
endif()

# ===================== Tests (optional) =====================

if(BUILD_TESTS)
    add_subdirectory(src/test)
endif()

# ===================== Install =====================

defold_install_targets(TARGETS input
    ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
    LIBRARY DESTINATION lib/${TARGET_PLATFORM}
    RUNTIME DESTINATION bin/${TARGET_PLATFORM}
)

# Install public headers
defold_add_sdk_headers_target(input_public_sdk_headers
    DESTINATION include/input
    SOURCE_ROOT "${INPUT_SRC_DIR}"
    FILES "${INPUT_SRC_DIR}/input.h"
    DEPENDS input)
defold_add_sdk_headers_target(input_generated_sdk_headers
    DESTINATION include/input
    SOURCE_ROOT "${CMAKE_CURRENT_BINARY_DIR}/input"
    FILES "${CMAKE_CURRENT_BINARY_DIR}/input/input_ddf.h"
    DEPENDS input)
add_custom_target(input_sdk_headers
    DEPENDS input_public_sdk_headers input_generated_sdk_headers)

install(FILES "${INPUT_SRC_DIR}/input.h" DESTINATION include/input)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/input/input_ddf.h" DESTINATION include/input)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/python/input/__init__.py" DESTINATION lib/python/input)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/python/input/input_ddf_pb2.py" DESTINATION lib/python/input)
