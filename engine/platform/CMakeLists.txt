# Minimum CMake version and project
cmake_minimum_required(VERSION 4.0)

# Early toolchain/SDK detection (for standalone configure)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_platform LANGUAGES ${DEFOLD_LANGUAGE_LIST})

# Sources
set(PLAT_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/src")
set(PLATFORM_BUILD_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")

file(GLOB _DEFOLD_PLATFORM_HEADERS "${PLAT_SRC_DIR}/platform_*.h")
list(APPEND _DEFOLD_PLATFORM_HEADERS
    "${PLAT_SRC_DIR}/window.h"
    "${PLAT_SRC_DIR}/window.hpp")
foreach(_DEFOLD_PLATFORM_HEADER IN LISTS _DEFOLD_PLATFORM_HEADERS)
    get_filename_component(_DEFOLD_PLATFORM_HEADER_NAME "${_DEFOLD_PLATFORM_HEADER}" NAME)
    configure_file("${_DEFOLD_PLATFORM_HEADER}" "${PLATFORM_BUILD_INCLUDE_DIR}/platform/${_DEFOLD_PLATFORM_HEADER_NAME}" COPYONLY)
endforeach()

# Select platform sources (centralized)
set(PLATFORM_USE_GLFW3 OFF)
if(DEFOLD_IS_PRIVATE_VENDOR)
    # Private vendor (e.g., Switch): only include native window integration
    set(_platform_glfw_sources
        "${PLAT_SRC_DIR}/window.cpp"
        "${PLAT_SRC_DIR}/nx64/platform_window_native.cpp")
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
target_include_directories(platform PUBLIC "${PLAT_SRC_DIR}" "${PLATFORM_BUILD_INCLUDE_DIR}")
target_compile_definitions(platform PRIVATE "DLIB_LOG_DOMAIN=\"PLATFORM\"")

if (PLATFORM_USE_GLFW3)
    target_compile_definitions(platform PUBLIC DM_INPUT_USE_GLFW3)
endif()

# platform_null
defold_add_library(platform_null STATIC
    "${PLAT_SRC_DIR}/window.cpp"
    "${PLAT_SRC_DIR}/platform_window_null.cpp")
target_include_directories(platform_null PUBLIC "${PLAT_SRC_DIR}")
target_include_directories(platform_null PUBLIC "${PLATFORM_BUILD_INCLUDE_DIR}")
target_compile_definitions(platform_null PRIVATE "DLIB_LOG_DOMAIN=\"PLATFORM\"")

# platform_vulkan (adds Vulkan-capable window integration on top of GLFW3)
if (PLATFORM_USE_GLFW3)
    set(_platform_vulkan_sources ${_platform_glfw_sources} "${PLAT_SRC_DIR}/platform_window_glfw3_vulkan.cpp")
    list(REMOVE_DUPLICATES _platform_vulkan_sources)
    defold_add_library(platform_vulkan STATIC ${_platform_vulkan_sources})
    target_include_directories(platform_vulkan PUBLIC "${PLAT_SRC_DIR}" "${PLATFORM_BUILD_INCLUDE_DIR}")
    target_compile_definitions(platform_vulkan PRIVATE "DLIB_LOG_DOMAIN=\"PLATFORM\"")
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
if(_DEFOLD_PLATFORM_HEADERS)
    install(FILES ${_DEFOLD_PLATFORM_HEADERS} DESTINATION include/platform)
endif()

install(FILES
    "${PLAT_SRC_DIR}/dmsdk/platform/window.h"
    DESTINATION sdk/include/dmsdk/platform)

# ********************* Tests *********************

add_subdirectory(src/test)
