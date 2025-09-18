# Minimum CMake version and project
cmake_minimum_required(VERSION 3.16)

include(../../scripts/cmake/defold.cmake)
include(functions_glfw)
include(functions_protoc)

project(defold_input LANGUAGES C CXX)

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

add_library(input STATIC
    "${INPUT_SRC_DIR}/input.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/input/input_ddf.pb.cc"
)

target_include_directories(input PUBLIC "${INPUT_SRC_DIR}")
# just to make sure the extra dependency is actually built
add_dependencies(input input_proto_py)

# Add GLFW3-specific compile define when applicable
defold_get_glfw_version(_GLFW_VERSION "${TARGET_PLATFORM}")
if (_GLFW_VERSION EQUAL 3)
    target_compile_definitions(input PUBLIC DM_INPUT_USE_GLFW3)
endif()

# ===================== Tests (optional) =====================

if(BUILD_TESTS)
    add_executable(test_input "${INPUT_SRC_DIR}/test/test_input.cpp")

    target_include_directories(input PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

    target_link_libraries(test_input PRIVATE
        dlib
        mbedtls
        ddf
        platform_null
        profile_null
        hid_null
        input
        testmain
    )

    if (_GLFW_VERSION EQUAL 3)
        target_link_libraries(test_input PRIVATE glfw3)
    else()
        target_link_libraries(test_input PRIVATE dmglfw)
    endif()

    if (TARGET_PLATFORM MATCHES "arm64-android|armv7-android")
        #set(_DM_USE_OPENGLES ON)

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

    # Register into global build_tests and run_tests
    defold_register_test_target(test_input)
endif()

# ===================== Install =====================

install(TARGETS input
    ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
    LIBRARY DESTINATION lib/${TARGET_PLATFORM}
    RUNTIME DESTINATION bin/${TARGET_PLATFORM}
)

# Install public headers
install(FILES "${INPUT_SRC_DIR}/input.h" DESTINATION include/input)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/input/input_ddf.h" DESTINATION include/input)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/python/input/__init__.py" DESTINATION lib/python/input)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/python/input/input_ddf_pb2.py" DESTINATION lib/python/input)
