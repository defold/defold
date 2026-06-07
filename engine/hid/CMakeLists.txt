# Minimum CMake version and project
cmake_minimum_required(VERSION 4.0)

# Early toolchain/SDK detection (for standalone configure)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_hid LANGUAGES ${DEFOLD_LANGUAGE_LIST})

# ***********************************************************************

# Define log domain for all targets in this directory
add_compile_definitions(DLIB_LOG_DOMAIN="HID")

# Sources
set(HID_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/src")
set(HID_DEPENDENCIES
    dlib
    platform)
set(HID_NULL_DEPENDENCIES
    dlib
    platform_null)
set(HID_PRIVATE_SRC_DIR "${DEFOLD_PLATFORM_HID_SOURCE_DIR}")
set(HID_PLATFORM_INCLUDE_DIRS)
set(HID_GLFW_PLATFORM_SOURCES
    "${HID_SRC_DIR}/glfw/keyboard_glfw.cpp"
    "${HID_SRC_DIR}/glfw/mouse_glfw.cpp"
    "${HID_SRC_DIR}/glfw/touch_glfw.cpp"
)
set(HID_NULL_INPUT_SOURCES
    "${HID_SRC_DIR}/null/keyboard_null.cpp"
    "${HID_SRC_DIR}/null/mouse_null.cpp"
    "${HID_SRC_DIR}/null/touch_null.cpp"
)
file(GLOB_RECURSE HID_DMSDK_HEADERS CONFIGURE_DEPENDS
    "${HID_SRC_DIR}/dmsdk/*.h"
    "${HID_SRC_DIR}/dmsdk/*.hpp")
set(HID_PRIVATE_DMSDK_HEADERS)

# hid (native backend)
set(hid_SOURCES
    "${HID_SRC_DIR}/hid.cpp"
    "${HID_SRC_DIR}/hid_common.cpp"
    "${HID_SRC_DIR}/gamepad_guid.cpp"
)

if(TARGET_PLATFORM MATCHES "arm64-macos|x86_64-macos|arm64-ios|x86_64-ios")
    list(APPEND hid_SOURCES
        "${HID_SRC_DIR}/gamepad.cpp"
        ${HID_GLFW_PLATFORM_SOURCES}
        "${HID_SRC_DIR}/apple/gamepad_apple.mm"
    )
    list(APPEND HID_PLATFORM_INCLUDE_DIRS "${HID_SRC_DIR}/apple" "${HID_SRC_DIR}/glfw")
    set(WITH_VULKAN OFF)
    set(WITH_OPENGL ON)
elseif(TARGET_PLATFORM MATCHES "x86_64-win32|win32")
    list(APPEND hid_SOURCES
        "${HID_SRC_DIR}/gamepad.cpp"
        ${HID_GLFW_PLATFORM_SOURCES}
        "${HID_SRC_DIR}/win32/gamepad_win32.cpp"
        "${HID_SRC_DIR}/win32/gamepad_win32_switch.cpp")
    list(APPEND HID_PLATFORM_INCLUDE_DIRS "${HID_SRC_DIR}/glfw" "${HID_SRC_DIR}/win32")
elseif(TARGET_PLATFORM MATCHES "x86_64-xbone")
    list(APPEND hid_SOURCES
        "${HID_SRC_DIR}/gamepad.cpp"
        ${HID_NULL_INPUT_SOURCES}
        "${HID_SRC_DIR}/win32/gamepad_win32.cpp")
    list(APPEND HID_PLATFORM_INCLUDE_DIRS "${HID_SRC_DIR}/null" "${HID_SRC_DIR}/win32")
elseif(DEFOLD_IS_PRIVATE_VENDOR)
    if(NOT HID_PRIVATE_SRC_DIR)
        message(FATAL_ERROR "Private HID platform requires DEFOLD_PLATFORM_HID_SOURCE_DIR")
    endif()
    file(GLOB _hid_vendor_sources
        "${HID_PRIVATE_SRC_DIR}/${TARGET_PLATFORM_OS}/*.c"
        "${HID_PRIVATE_SRC_DIR}/${TARGET_PLATFORM_OS}/*.cpp"
        "${HID_PRIVATE_SRC_DIR}/${TARGET_PLATFORM_OS}/*.mm")
    list(APPEND hid_SOURCES ${_hid_vendor_sources})
    list(APPEND HID_PLATFORM_INCLUDE_DIRS "${HID_PRIVATE_SRC_DIR}/${TARGET_PLATFORM_OS}")
else()
    list(APPEND hid_SOURCES
        "${HID_SRC_DIR}/gamepad.cpp"
        ${HID_GLFW_PLATFORM_SOURCES}
        "${HID_SRC_DIR}/glfw/gamepad_glfw.cpp")
    list(APPEND HID_PLATFORM_INCLUDE_DIRS "${HID_SRC_DIR}/glfw")
endif()

defold_add_library(hid STATIC ${hid_SOURCES})
target_compile_definitions(hid PRIVATE "DLIB_LOG_DOMAIN=\"HID\"")
if(TARGET_PLATFORM MATCHES "^(arm64-macos|x86_64-macos|arm64-ios|x86_64-ios)$")
    target_link_libraries(hid PUBLIC "-framework GameController")
endif()
if(TARGET_PLATFORM MATCHES "x86_64-win32|win32")
    target_link_libraries(hid PUBLIC setupapi.lib)
    if(DEFOLD_WINSDK_LIB_UM_DIR)
        target_link_libraries(hid PUBLIC "${DEFOLD_WINSDK_LIB_UM_DIR}/hid.lib")
    else()
        target_link_options(hid PUBLIC "/DEFAULTLIB:hid.lib")
    endif()
endif()
defold_target_link_libraries(hid "${TARGET_PLATFORM}" SCOPE PUBLIC ${HID_DEPENDENCIES})

target_include_directories(hid PUBLIC
    "${HID_SRC_DIR}"
    "${HID_SRC_DIR}/external"
    "${HID_SRC_DIR}/default/src"
    ${HID_PLATFORM_INCLUDE_DIRS}
) # match Waf include layout for hid sources

if(HID_PRIVATE_SRC_DIR)
    target_compile_definitions(hid PUBLIC DM_HID_USE_VENDOR_NATIVE_DEFINES)
    file(GLOB HID_PRIVATE_DMSDK_HEADERS CONFIGURE_DEPENDS
        "${HID_PRIVATE_SRC_DIR}/dmsdk/hid/hid_native_defines_vendor.h")

    foreach(_hid_private_dmsdk_dir IN LISTS DEFOLD_PLATFORM_HID_DMSDK_DIRS)
        file(GLOB_RECURSE _hid_private_platform_headers CONFIGURE_DEPENDS
            "${_hid_private_dmsdk_dir}/*.h"
            "${_hid_private_dmsdk_dir}/*.hpp")
        list(APPEND HID_PRIVATE_DMSDK_HEADERS ${_hid_private_platform_headers})
    endforeach()

    target_include_directories(hid PUBLIC
        "${HID_PRIVATE_SRC_DIR}"
        "${HID_PRIVATE_SRC_DIR}/native"
        "${HID_PRIVATE_SRC_DIR}/external"
        "${HID_PRIVATE_SRC_DIR}/default/src"
    )
endif()

defold_add_library(hid_null STATIC
    "${HID_SRC_DIR}/hid_common.cpp"
    "${HID_SRC_DIR}/gamepad.cpp"
    "${HID_SRC_DIR}/gamepad_guid.cpp"
    "${HID_SRC_DIR}/null/hid_null.cpp"
)
target_include_directories(hid_null PUBLIC
    "${HID_SRC_DIR}"
    "${HID_SRC_DIR}/null"
    "${HID_SRC_DIR}/external"
    "${HID_SRC_DIR}/default/src"
)
target_compile_definitions(hid_null PRIVATE "DLIB_LOG_DOMAIN=\"HID\"")
defold_target_link_libraries(hid_null "${TARGET_PLATFORM}" SCOPE PUBLIC ${HID_NULL_DEPENDENCIES})
set_target_properties(hid_null PROPERTIES DEFOLD_SDK_HEADERS_TARGET hid_sdk_headers)

if(HID_PRIVATE_SRC_DIR)
    target_compile_definitions(hid_null PUBLIC DM_HID_USE_VENDOR_NATIVE_DEFINES)
    target_include_directories(hid_null PUBLIC
        "${HID_PRIVATE_SRC_DIR}"
        "${HID_PRIVATE_SRC_DIR}/null"
        "${HID_PRIVATE_SRC_DIR}/external"
        "${HID_PRIVATE_SRC_DIR}/default/src"
    )
endif()


# ===================== Tests =====================

if(BUILD_TESTS)
    # test_hid uses jc_test and links against Defold support libs
    defold_add_executable(test_hid "${HID_SRC_DIR}/test/test_hid.cpp")

    defold_target_link_libraries(test_hid "${TARGET_PLATFORM}"
        dlib
        profile_null
        testmain
        platform_null
        hid_null
    )

    if (TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
        find_package(Threads REQUIRED)
        target_link_libraries(test_hid PRIVATE Threads::Threads dl)
    endif()

    defold_target_link_glfw(test_hid "${TARGET_PLATFORM}" PRIVATE)
    defold_target_link_socket(test_hid "${TARGET_PLATFORM}" PRIVATE)

    if(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
        target_link_options(test_hid PRIVATE
            "SHELL:--js-library=${CMAKE_CURRENT_LIST_DIR}/../../external/glfw/js/library_glfw.js"
            "SHELL:--js-library=${DEFOLD_LIB_DIR}/js/library_sys.js")
    endif()

    # Register into global build_tests and run_tests
    defold_register_test_target(test_hid ON "${CMAKE_CURRENT_LIST_DIR}"
        STAGE_FILES
            "build/src/test/build" "build/src/test/build"
    )

    # =====================================================================================

    defold_get_graphics_symbols(EXPORTED_SYMBOLS ${TARGET_PLATFORM})

    message(STATUS "Symbols: ${EXPORTED_SYMBOLS}")

    defold_create_exported_symbols_file("${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols.cpp" "${EXPORTED_SYMBOLS}")

    defold_add_executable(test_app_hid
        "${HID_SRC_DIR}/test/test_app_hid.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols.cpp"
    )

    if (TARGET_PLATFORM MATCHES "arm64-android|armv7-android")
        set(_DM_USE_OPENGLES ON)

    elseif (TARGET_PLATFORM MATCHES "arm64-ios|x86_64-ios")
        target_link_libraries(test_app_hid PRIVATE "-framework IOKit" "-framework GameController")

    elseif (TARGET_PLATFORM MATCHES "arm64-macos|x86_64-macos")
        target_link_libraries(test_app_hid PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo" "-framework GameController")

    elseif (TARGET_PLATFORM MATCHES "x86_64-win32|win32")
        target_link_libraries(test_app_hid PRIVATE user32 gdi32 shell32)

    elseif (TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
        find_package(Threads REQUIRED)
        target_link_libraries(test_app_hid PRIVATE Threads::Threads dl)
    endif()

    defold_target_link_libraries(test_app_hid "${TARGET_PLATFORM}"
        dlib
        profile_null
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
defold_install_targets(TARGETS hid hid_null
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM}
)

# Install public headers
defold_add_sdk_headers_target(hid_public_sdk_headers
    DESTINATION include/hid
    SOURCE_ROOT "${HID_SRC_DIR}"
    FILES "${HID_SRC_DIR}/hid.h"
    DEPENDS hid)
defold_add_sdk_headers_target(hid_dmsdk_sdk_headers
    DESTINATION sdk/include/dmsdk
    SOURCE_ROOT "${HID_SRC_DIR}/dmsdk"
    FILES ${HID_DMSDK_HEADERS}
    DEPENDS hid)
set(HID_SDK_HEADER_TARGETS hid_public_sdk_headers hid_dmsdk_sdk_headers)
if(HID_PRIVATE_DMSDK_HEADERS)
    defold_add_sdk_headers_target(hid_private_dmsdk_sdk_headers
        DESTINATION sdk/include/dmsdk/hid
        SOURCE_ROOT "${HID_PRIVATE_SRC_DIR}/dmsdk/hid"
        FILES ${HID_PRIVATE_DMSDK_HEADERS}
        DEPENDS hid)
    list(APPEND HID_SDK_HEADER_TARGETS hid_private_dmsdk_sdk_headers)
endif()
add_custom_target(hid_sdk_headers
    DEPENDS ${HID_SDK_HEADER_TARGETS})

install(FILES "${HID_SRC_DIR}/hid.h" DESTINATION include/hid)

# Install dmsdk headers for HID with folder structure preserved
install(DIRECTORY "${HID_SRC_DIR}/dmsdk/"
                DESTINATION sdk/include/dmsdk
                FILES_MATCHING
                PATTERN "*.h"
                PATTERN "*.hpp")

if(HID_PRIVATE_DMSDK_HEADERS)
    install(DIRECTORY "${HID_PRIVATE_SRC_DIR}/dmsdk/hid/"
            DESTINATION sdk/include/dmsdk/hid
            FILES_MATCHING
            PATTERN "*.h"
            PATTERN "*.hpp")
endif()
