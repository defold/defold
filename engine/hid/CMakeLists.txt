# Minimum CMake version and project
cmake_minimum_required(VERSION 3.16)

include(../../scripts/cmake/defold.cmake)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(defold_hid LANGUAGES C CXX)

# Options
#option(BUILD_HID_TESTS "Build HID unit tests" ON)

# ***********************************************************************

# Define log domain for all targets in this directory
add_compile_definitions(DLIB_LOG_DOMAIN="HID")

# Sources
set(HID_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/src")

# hid (native backend)
set(hid_SOURCES )
list(APPEND hid_SOURCES "${HID_SRC_DIR}/hid.cpp")

if (NOT TARGET_PLATFORM MATCHES "x86_64-ps4|x86_64-ps5|arm64-nx64")
    list(APPEND hid_SOURCES "${HID_SRC_DIR}/native/hid_native.cpp")
    list(APPEND hid_SOURCES "${HID_SRC_DIR}/native/hid_gamepad_driver_glfw.cpp")
endif()

add_library(hid STATIC ${hid_SOURCES})

target_include_directories(hid PUBLIC "${HID_SRC_DIR}") # for dmsdk/hid and local headers

# hid_null (null backend)
add_library(hid_null STATIC
    "${HID_SRC_DIR}/hid.cpp"
    "${HID_SRC_DIR}/hid_null.cpp"
)
target_include_directories(hid_null PUBLIC "${HID_SRC_DIR}")


# ===================== Tests =====================

if(BUILD_TESTS)
    # test_hid uses jc_test and links against Defold support libs
    add_executable(test_hid "${HID_SRC_DIR}/test/test_hid.cpp")

    # Do not build test executables by default (optional via explicit target)
    set_target_properties(test_hid PROPERTIES EXCLUDE_FROM_ALL TRUE)

    target_link_libraries(test_hid PRIVATE
        ${defold_libprefix}dlib
        ${defold_libprefix}profile_null
        ${defold_libprefix}platform_null
        ${defold_libprefix}testmain
        hid_null
    )

    if (TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
        find_package(Threads REQUIRED)
        target_link_libraries(test_hid PRIVATE glfw3 Threads::Threads dl)

    elseif (TARGET_PLATFORM MATCHES "x86-win32|x86_64-win32")
        target_link_libraries(test_hid PRIVATE)
    endif()

    defold_target_link_socket(test_hid "${TARGET_PLATFORM}" PRIVATE)

    # Register into global build_tests and run_tests
    defold_register_test_target(test_hid)

    # =====================================================================================

    defold_get_graphics_symbols(EXPORTED_SYMBOLS ${TARGET_PLATFORM})

    message(STATUS "Symbols: ${EXPORTED_SYMBOLS}")

    defold_create_exported_symbols_file("${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols.cpp" "${EXPORTED_SYMBOLS}")

    add_executable(test_app_hid
        "${HID_SRC_DIR}/test/test_app_hid.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols.cpp"
    )

    if (TARGET_PLATFORM MATCHES "arm64-android|armv7-android")
        set(_DM_USE_OPENGLES ON)

    elseif (TARGET_PLATFORM MATCHES "arm64-ios|x86_64-ios")
        target_link_libraries(test_app_hid PRIVATE "-framework IOKit")

    elseif (TARGET_PLATFORM MATCHES "arm64-macos|x86_64-macos")
        target_link_libraries(test_app_hid PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")

    elseif (TARGET_PLATFORM MATCHES "x86_64-win32|win32")
        target_link_libraries(test_app_hid PRIVATE user32 gdi32 shell32)

    elseif (TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
        find_package(Threads REQUIRED)
        target_link_libraries(test_app_hid PRIVATE Threads::Threads dl)
    endif()

    target_link_libraries(test_app_hid PRIVATE
        ${defold_libprefix}dlib
        ${defold_libprefix}profile_null
        hid
    )

    defold_target_link_graphics(test_app_hid "${TARGET_PLATFORM}" PRIVATE)
    defold_target_link_platform(test_app_hid "${TARGET_PLATFORM}" PRIVATE)
    defold_target_link_socket(test_app_hid "${TARGET_PLATFORM}" PRIVATE)

    # Register into global build_tests and run_tests
    defold_register_test_target(test_app_hid FALSE)

endif()

# ===================== Install =====================

# Install static libraries to lib/<target-platform>
install(TARGETS hid hid_null
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM}
)

# Install public headers
install(FILES "${HID_SRC_DIR}/hid.h" DESTINATION include/hid)

# Install dmsdk headers for HID with folder structure preserved
install(DIRECTORY "${HID_SRC_DIR}/dmsdk/"
                DESTINATION sdk/include/dmsdk
                FILES_MATCHING
                PATTERN "*.h"
                PATTERN "*.hpp")
