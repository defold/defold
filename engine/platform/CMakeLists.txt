# Minimum CMake version and project
cmake_minimum_required(VERSION 4.0)

# Early toolchain/SDK detection (for standalone configure)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_platform LANGUAGES C CXX)

# Sources
set(PLAT_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/src")

# Common GLFW sources
defold_get_glfw_version(_GLFW_VERSION "${TARGET_PLATFORM}")
if (_GLFW_VERSION EQUAL 3)
    set(_platform_glfw_sources "${PLAT_SRC_DIR}/platform_window_glfw3.cpp")

    if (TARGET_PLATFORM MATCHES "arm64-macos|x86_64-macos")
        list(APPEND _platform_glfw_sources "${PLAT_SRC_DIR}/platform_window_glfw3_osx.cpp")
    elseif (TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
        list(APPEND _platform_glfw_sources "${PLAT_SRC_DIR}/platform_window_glfw3_linux.cpp")
    elseif (TARGET_PLATFORM MATCHES "arm64-win32|x86_64-win32|x86-win32")
        list(APPEND _platform_glfw_sources "${PLAT_SRC_DIR}/platform_window_glfw3_win32.cpp")
    endif()
else()
    set(_platform_glfw_sources "${PLAT_SRC_DIR}/platform_window_glfw.cpp")
    if (TARGET_PLATFORM MATCHES "arm64-ios|x86_64-ios")
        list(APPEND _platform_glfw_sources "${PLAT_SRC_DIR}/platform_window_glfw_ios.cpp")
    elseif (TARGET_PLATFORM MATCHES "armv7-android|arm64-android")
        list(APPEND _platform_glfw_sources "${PLAT_SRC_DIR}/platform_window_glfw_android.cpp")
    endif()
endif()

# platform
defold_add_library(platform STATIC ${_platform_glfw_sources})
target_include_directories(platform PUBLIC "${PLAT_SRC_DIR}")
target_compile_definitions(platform PUBLIC "DLIB_LOG_DOMAIN=\"PLATFORM\"")

if (_GLFW_VERSION EQUAL 3)
    target_compile_definitions(platform PUBLIC DM_INPUT_USE_GLFW3)
endif()

# platform_null
defold_add_library(platform_null STATIC "${PLAT_SRC_DIR}/platform_window_null.cpp")
target_include_directories(platform_null PUBLIC "${PLAT_SRC_DIR}")
target_compile_definitions(platform_null PUBLIC "DLIB_LOG_DOMAIN=\"PLATFORM\"")

# platform_vulkan (adds Vulkan-capable window integration on top of GLFW3)
if (_GLFW_VERSION EQUAL 3)
    set(_platform_vulkan_sources ${_platform_glfw_sources} "${PLAT_SRC_DIR}/platform_window_glfw3_vulkan.cpp")
    list(REMOVE_DUPLICATES _platform_vulkan_sources)
    defold_add_library(platform_vulkan STATIC ${_platform_vulkan_sources})
    target_include_directories(platform_vulkan PUBLIC "${PLAT_SRC_DIR}")
    target_compile_definitions(platform_vulkan PUBLIC "DLIB_LOG_DOMAIN=\"PLATFORM\"")
endif()

# ===================== Tests =====================

add_subdirectory(src/test)

# ===================== Install =====================

install(TARGETS platform platform_null
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})

if (_GLFW_VERSION EQUAL 3)
    install(TARGETS platform_vulkan
            ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
            LIBRARY DESTINATION lib/${TARGET_PLATFORM}
            RUNTIME DESTINATION bin/${TARGET_PLATFORM})
endif()

# Install public platform headers (platform_*.h) into include/platform
file(GLOB _DEFOLD_PLATFORM_HEADERS "${PLAT_SRC_DIR}/platform_*.h")
if(_DEFOLD_PLATFORM_HEADERS)
    install(FILES ${_DEFOLD_PLATFORM_HEADERS} DESTINATION include/platform)
endif()
