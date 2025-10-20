cmake_minimum_required(VERSION 4.0)

# Early toolchain/SDK detection (for standalone configure)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

project(defold_platform_tests LANGUAGES ${DEFOLD_LANGUAGE_LIST})

# test_platform (unit tests using null backend)
defold_add_executable(test_platform
    "${CMAKE_CURRENT_LIST_DIR}/test_platform.cpp")

# Include the platform src directory for headers
target_include_directories(test_platform PRIVATE "${CMAKE_CURRENT_LIST_DIR}/..")

defold_target_link_libraries(test_platform "${TARGET_PLATFORM}"
    dlib
    profile_null
    testmain
    platform_null
)

defold_target_link_socket(test_platform "${TARGET_PLATFORM}" PRIVATE)

# Ensure platform libs are built before tests when available
if(TARGET platform_null)
    add_dependencies(test_platform platform_null)
endif()

# Register into global build_tests and run_tests with working directory set to engine/input
defold_register_test_target(test_platform ON "${CMAKE_CURRENT_LIST_DIR}/../..")

# =============================================================
# test_platform_app (windowed test application)

if (NOT (TARGET_PLATFORM MATCHES "js-web|wasm-web|wasm_pthread-web"))

    defold_add_executable(test_platform_app
        "${CMAKE_CURRENT_LIST_DIR}/test_platform_app.cpp")

    # Include the platform src directory for headers
    target_include_directories(test_platform_app PRIVATE "${CMAKE_CURRENT_LIST_DIR}/..")

    # Platform specific extras
    if (TARGET_PLATFORM MATCHES "arm64-android|armv7-android")
    elseif (TARGET_PLATFORM MATCHES "arm64-ios|x86_64-ios")
        target_link_libraries(test_platform_app PRIVATE "-framework IOKit" "-framework QuartzCore")
    elseif (TARGET_PLATFORM MATCHES "arm64-macos|x86_64-macos")
        target_link_libraries(test_platform_app PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
    elseif (TARGET_PLATFORM MATCHES "x86_64-win32|x86-win32")
        target_link_libraries(test_platform_app PRIVATE user32 gdi32 shell32)
    elseif (TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
        find_package(Threads REQUIRED)
        target_link_libraries(test_platform_app PRIVATE Threads::Threads dl)
    endif()

    defold_target_link_libraries(test_platform_app "${TARGET_PLATFORM}"
        dlib
        profile_null
    )

    defold_target_link_opengl(test_platform_app "${TARGET_PLATFORM}" PRIVATE)
    defold_target_link_glfw(test_platform_app "${TARGET_PLATFORM}" PRIVATE)
    defold_target_link_platform(test_platform_app "${TARGET_PLATFORM}" PRIVATE)
    defold_target_link_socket(test_platform_app "${TARGET_PLATFORM}" PRIVATE)

    # Ensure platform libs are built before app test when available
    if(TARGET platform)
        add_dependencies(test_platform_app platform)
    endif()
    if(TARGET platform_vulkan)
        add_dependencies(test_platform_app platform_vulkan)
    endif()

    defold_register_test_target(test_platform_app FALSE)
endif()
