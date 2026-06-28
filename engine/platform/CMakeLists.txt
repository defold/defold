# Minimum CMake version and project
cmake_minimum_required(VERSION 4.0)

# Early toolchain/SDK detection (for standalone configure)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_platform LANGUAGES ${DEFOLD_LANGUAGE_LIST})

# Sources
set(PLAT_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/src")
set(PLATFORM_DEPENDENCIES dlib)

file(GLOB _DEFOLD_PLATFORM_HEADERS "${PLAT_SRC_DIR}/platform_*.h")
list(APPEND _DEFOLD_PLATFORM_HEADERS
    "${PLAT_SRC_DIR}/window.h"
    "${PLAT_SRC_DIR}/window.hpp")

# Select platform sources (centralized)
set(PLATFORM_USE_GLFW3 OFF)
if(DEFOLD_PLATFORM_WINDOW_SOURCES)
    set(_platform_private_extra_sources)
    if(DEFOLD_PLATFORM_WINDOW_JOYSTICK_GUID_STUB)
        set(_platform_private_compat_source "${CMAKE_CURRENT_BINARY_DIR}/platform_window_private_compat.cpp")
        file(WRITE "${_platform_private_compat_source}"
            "#include <stdint.h>\n"
            "#include \"window.h\"\n"
            "namespace dmPlatform\n"
            "{\n"
            "    const char* GetJoystickDeviceGuid(HWindow window, uint32_t joystick_index)\n"
            "    {\n"
            "        return 0;\n"
            "    }\n"
            "}\n")
        list(APPEND _platform_private_extra_sources "${_platform_private_compat_source}")
    endif()
    set(_platform_glfw_sources
        "${PLAT_SRC_DIR}/window.cpp"
        ${DEFOLD_PLATFORM_WINDOW_SOURCES}
        ${_platform_private_extra_sources})
elseif(DEFOLD_IS_PRIVATE_VENDOR)
    if(TARGET_PLATFORM_OS STREQUAL "xbone")
        set(_platform_glfw_sources
            "${PLAT_SRC_DIR}/window.cpp"
            "${DEFOLD_PRIVATE_REPO_ROOT}/engine/platform/src/xbox/platform_window_xbox.cpp")
    else()
        # Private vendor (e.g., Switch): only include native window integration
        set(_platform_glfw_sources
            "${PLAT_SRC_DIR}/window.cpp"
            "${PLAT_SRC_DIR}/${TARGET_PLATFORM_OS}/platform_window_native.cpp")
    endif()
else()
    defold_get_glfw_version(_GLFW_VERSION "${TARGET_PLATFORM}")
    if (_GLFW_VERSION EQUAL 3)
        set(PLATFORM_USE_GLFW3 ON)
        set(_platform_glfw_sources
            "${PLAT_SRC_DIR}/window.cpp"
            "${PLAT_SRC_DIR}/platform_window_glfw3.cpp")
        if (TARGET_PLATFORM MATCHES "arm64-macos|x86_64-macos")
            list(APPEND _platform_glfw_sources "${PLAT_SRC_DIR}/platform_window_glfw3_osx.mm")
        elseif (TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
            list(APPEND _platform_glfw_sources "${PLAT_SRC_DIR}/platform_window_glfw3_linux.cpp")
        elseif (TARGET_PLATFORM MATCHES "arm64-win32|x86_64-win32|x86-win32")
            list(APPEND _platform_glfw_sources "${PLAT_SRC_DIR}/platform_window_glfw3_win32.cpp")
        endif()
    else()
        set(_platform_glfw_sources
            "${PLAT_SRC_DIR}/window.cpp"
            "${PLAT_SRC_DIR}/platform_window_glfw.cpp")
        if (TARGET_PLATFORM MATCHES "arm64-ios|x86_64-ios")
            list(APPEND _platform_glfw_sources "${PLAT_SRC_DIR}/platform_window_glfw_ios.mm")
        elseif (TARGET_PLATFORM MATCHES "armv7-android|arm64-android")
            list(APPEND _platform_glfw_sources "${PLAT_SRC_DIR}/platform_window_glfw_android.cpp")
        endif()
    endif()
endif()

# platform
defold_add_library(platform STATIC ${_platform_glfw_sources})
target_include_directories(platform PUBLIC "${PLAT_SRC_DIR}")
target_compile_definitions(platform PRIVATE "DLIB_LOG_DOMAIN=\"PLATFORM\"")
defold_target_link_libraries(platform "${TARGET_PLATFORM}" SCOPE PUBLIC ${PLATFORM_DEPENDENCIES})

if (PLATFORM_USE_GLFW3)
    target_compile_definitions(platform PUBLIC DM_INPUT_USE_GLFW3)
endif()

# platform_null
defold_add_library(platform_null STATIC
    "${PLAT_SRC_DIR}/window.cpp"
    "${PLAT_SRC_DIR}/platform_window_null.cpp")
target_include_directories(platform_null PUBLIC "${PLAT_SRC_DIR}")
target_compile_definitions(platform_null PRIVATE "DLIB_LOG_DOMAIN=\"PLATFORM\"")
defold_target_link_libraries(platform_null "${TARGET_PLATFORM}" SCOPE PUBLIC ${PLATFORM_DEPENDENCIES})
set_target_properties(platform_null PROPERTIES DEFOLD_SDK_HEADERS_TARGET platform_sdk_headers)

# platform_vulkan (adds Vulkan-capable window integration on top of GLFW3)
if (PLATFORM_USE_GLFW3)
    set(_platform_vulkan_sources ${_platform_glfw_sources} "${PLAT_SRC_DIR}/platform_window_glfw3_vulkan.cpp")
    list(REMOVE_DUPLICATES _platform_vulkan_sources)
    defold_add_library(platform_vulkan STATIC ${_platform_vulkan_sources})
    target_include_directories(platform_vulkan PUBLIC "${PLAT_SRC_DIR}")
    target_compile_definitions(platform_vulkan PRIVATE "DLIB_LOG_DOMAIN=\"PLATFORM\"")
    defold_target_link_libraries(platform_vulkan "${TARGET_PLATFORM}" SCOPE PUBLIC ${PLATFORM_DEPENDENCIES})
    set_target_properties(platform_vulkan PROPERTIES DEFOLD_SDK_HEADERS_TARGET platform_sdk_headers)
endif()


# ********************* Install *********************

defold_install_targets(TARGETS platform platform_null
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})

if (PLATFORM_USE_GLFW3)
    defold_install_targets(TARGETS platform_vulkan
            ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
            LIBRARY DESTINATION lib/${TARGET_PLATFORM}
            RUNTIME DESTINATION bin/${TARGET_PLATFORM})
endif()

# Install public platform headers (platform_*.h) into include/platform
defold_add_sdk_headers_target(platform_public_sdk_headers
    DESTINATION include/platform
    SOURCE_ROOT "${PLAT_SRC_DIR}"
    FILES ${_DEFOLD_PLATFORM_HEADERS}
    DEPENDS platform)
install(FILES ${_DEFOLD_PLATFORM_HEADERS} DESTINATION include/platform)

defold_add_sdk_headers_target(platform_dmsdk_sdk_headers
    DESTINATION sdk/include/dmsdk
    SOURCE_ROOT "${PLAT_SRC_DIR}/dmsdk"
    FILES "${PLAT_SRC_DIR}/dmsdk/platform/window.h"
    DEPENDS platform)
add_custom_target(platform_sdk_headers
    DEPENDS platform_public_sdk_headers platform_dmsdk_sdk_headers)
set_target_properties(platform PROPERTIES DEFOLD_SDK_HEADERS_TARGET platform_sdk_headers)

install(FILES
    "${PLAT_SRC_DIR}/dmsdk/platform/window.h"
    DESTINATION sdk/include/dmsdk/platform)

# ********************* Tests *********************

add_subdirectory(src/test)
