cmake_minimum_required(VERSION 4.0)

# test_platform (unit tests using null backend)
add_executable(test_platform
    "${CMAKE_CURRENT_LIST_DIR}/test_platform.cpp")
set_target_properties(test_platform PROPERTIES EXCLUDE_FROM_ALL TRUE)

target_link_libraries(test_platform PRIVATE
    ${defold_libprefix}dlib
    ${defold_libprefix}profile_null
    ${defold_libprefix}testmain
    platform_null)

defold_target_link_socket(test_platform "${TARGET_PLATFORM}" PRIVATE)

# Register into global build_tests and run_tests with working directory set to engine/input
defold_register_test_target(test_platform ON "${CMAKE_CURRENT_LIST_DIR}/../..")

# =============================================================
# test_platform_app (windowed test application)

# Exported symbols for graphics adapters
defold_get_graphics_symbols(_PLAT_APP_EXPORTED_SYMBOLS "${TARGET_PLATFORM}")
defold_create_exported_symbols_file("${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols.cpp" "${_PLAT_APP_EXPORTED_SYMBOLS}")

add_executable(test_platform_app
    "${CMAKE_CURRENT_LIST_DIR}/test_platform_app.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols.cpp")
set_target_properties(test_platform_app PROPERTIES EXCLUDE_FROM_ALL TRUE)

# Platform specific extras
if (TARGET_PLATFORM MATCHES "arm64-android|armv7-android")
    # nothing specific
elseif (TARGET_PLATFORM MATCHES "arm64-ios|x86_64-ios")
    target_link_libraries(test_platform_app PRIVATE "-framework IOKit")
elseif (TARGET_PLATFORM MATCHES "arm64-macos|x86_64-macos")
    target_link_libraries(test_platform_app PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
elseif (TARGET_PLATFORM MATCHES "x86_64-win32|x86-win32")
    target_link_libraries(test_platform_app PRIVATE user32 gdi32 shell32)
elseif (TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
    find_package(Threads REQUIRED)
    target_link_libraries(test_platform_app PRIVATE Threads::Threads dl)
endif()

target_link_libraries(test_platform_app PRIVATE
    ${defold_libprefix}dlib
    ${defold_libprefix}profile_null)

defold_target_link_glfw(test_platform_app "${TARGET_PLATFORM}" PRIVATE)
defold_target_link_platform(test_platform_app "${TARGET_PLATFORM}" PRIVATE)
defold_target_link_socket(test_platform_app "${TARGET_PLATFORM}" PRIVATE)

defold_register_test_target(test_platform_app FALSE)

