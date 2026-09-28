# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

# Minimum CMake version and project
cmake_minimum_required(VERSION 4.0)

# Early toolchain/SDK detection (for standalone configure)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_platform LANGUAGES ${DEFOLD_LANGUAGE_LIST})

# Sources
set(PLAT_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/src")
set(PLATFORM_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
set(PLATFORM_DEPENDENCIES dlib)

file(MAKE_DIRECTORY "${PLATFORM_GENERATED_INCLUDE_DIR}/platform")
file(GLOB _DEFOLD_PLATFORM_HEADERS "${PLAT_SRC_DIR}/platform_*.h")
foreach(_platform_header IN LISTS _DEFOLD_PLATFORM_HEADERS)
    get_filename_component(_platform_header_name "${_platform_header}" NAME)
    configure_file("${_platform_header}" "${PLATFORM_GENERATED_INCLUDE_DIR}/platform/${_platform_header_name}" COPYONLY)
endforeach()
configure_file("${PLAT_SRC_DIR}/window.h" "${PLATFORM_GENERATED_INCLUDE_DIR}/platform/window.h" COPYONLY)
configure_file("${PLAT_SRC_DIR}/window.hpp" "${PLATFORM_GENERATED_INCLUDE_DIR}/platform/window.hpp" COPYONLY)

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
    set(_platform_sources
        "${PLAT_SRC_DIR}/window.cpp"
        ${DEFOLD_PLATFORM_WINDOW_SOURCES}
        ${_platform_private_extra_sources})
elseif(DEFOLD_IS_PRIVATE_VENDOR)
    if(TARGET_PLATFORM_OS STREQUAL "xbone")
        set(_platform_sources
            "${PLAT_SRC_DIR}/window.cpp"
            "${DEFOLD_PRIVATE_REPO_ROOT}/engine/platform/src/xbox/platform_window_xbox.cpp")
    else()
        # Private vendor (e.g., Switch): only include native window integration
        set(_platform_sources
            "${PLAT_SRC_DIR}/window.cpp"
            "${PLAT_SRC_DIR}/${TARGET_PLATFORM_OS}/platform_window_native.cpp")
    endif()
else()
    defold_get_glfw_version(_GLFW_VERSION "${TARGET_PLATFORM}")
    if (_GLFW_VERSION EQUAL 3)
        set(PLATFORM_USE_GLFW3 ON)
        set(_platform_sources
            "${PLAT_SRC_DIR}/window.cpp"
            "${PLAT_SRC_DIR}/platform_window_glfw3.cpp")
        if (TARGET_PLATFORM MATCHES "arm64-macos|x86_64-macos")
            list(APPEND _platform_sources "${PLAT_SRC_DIR}/platform_window_glfw3_osx.mm")
        elseif (TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
            list(APPEND _platform_sources "${PLAT_SRC_DIR}/platform_window_glfw3_linux.cpp")
        elseif (TARGET_PLATFORM MATCHES "arm64-win32|x86_64-win32")
            list(APPEND _platform_sources "${PLAT_SRC_DIR}/platform_window_glfw3_win32.cpp")
        endif()
    else()
        set(_platform_sources
            "${PLAT_SRC_DIR}/window.cpp"
            "${PLAT_SRC_DIR}/platform_window_native.cpp")
        if (TARGET_PLATFORM MATCHES "arm64-ios|arm64_sim-ios")
            list(APPEND _platform_sources "${PLAT_SRC_DIR}/platform_window_ios.mm" "${PLAT_SRC_DIR}/platform_app_ios.mm")
        elseif (TARGET_PLATFORM MATCHES "armv7-android|arm64-android|x86_64-android")
            list(APPEND _platform_sources "${PLAT_SRC_DIR}/platform_window_android.cpp" "${PLAT_SRC_DIR}/platform_app_android.cpp")
        endif()
    endif()
endif()

# The mobile and web backends are engine sources, built with the platform library.
set(PLATFORM_NATIVE_DIR "${PLAT_SRC_DIR}/native")
set(PLATFORM_NATIVE_SOURCES)
set(PLATFORM_NATIVE_VULKAN_SOURCES)
if(NOT PLATFORM_USE_GLFW3 AND NOT DEFOLD_IS_PRIVATE_VENDOR AND NOT DEFOLD_PLATFORM_WINDOW_SOURCES)
    if(TARGET_PLATFORM_OS STREQUAL "web")
        set(PLATFORM_NATIVE_SOURCES "${PLATFORM_NATIVE_DIR}/web/web.c" "${PLATFORM_NATIVE_DIR}/native_touch.c")
    else()
        file(GLOB PLATFORM_NATIVE_COMMON CONFIGURE_DEPENDS "${PLATFORM_NATIVE_DIR}/*.c")
        if(TARGET_PLATFORM_OS STREQUAL "ios")
            file(GLOB PLATFORM_NATIVE_OS CONFIGURE_DEPENDS
                "${PLATFORM_NATIVE_DIR}/ios/*.m" "${PLATFORM_NATIVE_DIR}/ios/app/*.m")
        elseif(TARGET_PLATFORM_OS STREQUAL "android")
            file(GLOB PLATFORM_NATIVE_OS CONFIGURE_DEPENDS "${PLATFORM_NATIVE_DIR}/android/*.c")
        endif()
        set(PLATFORM_NATIVE_SOURCES ${PLATFORM_NATIVE_COMMON} ${PLATFORM_NATIVE_OS})
        if(TARGET_PLATFORM_OS STREQUAL "android")
            set(PLATFORM_NATIVE_VULKAN_SOURCES ${PLATFORM_NATIVE_SOURCES})
            list(FILTER PLATFORM_NATIVE_VULKAN_SOURCES EXCLUDE REGEX "/(android_util|android_glext|android_window_gl)\\.c$")
            list(FILTER PLATFORM_NATIVE_SOURCES EXCLUDE REGEX "/android_window_no_api\\.c$")
        endif()
    endif()
endif()

function(platform_configure_native target)
    target_include_directories(${target} PRIVATE "${PLATFORM_NATIVE_DIR}" "${PLATFORM_NATIVE_DIR}/${TARGET_PLATFORM_OS}")
    target_compile_definitions(${target} PRIVATE SDL_JOYSTICK_IOKIT)
    if(TARGET_PLATFORM_OS STREQUAL "ios")
        target_link_libraries(${target} PUBLIC "-framework UIKit" "-framework QuartzCore" "-framework OpenGLES"
            "-framework CoreVideo" "-framework CoreGraphics" "-framework CoreMotion")
    elseif(TARGET_PLATFORM_OS STREQUAL "android")
        target_link_libraries(${target} PUBLIC android log)
        if(NOT target STREQUAL "platform_vulkan")
            target_link_libraries(${target} PUBLIC EGL GLESv1_CM GLESv2)
        endif()
    elseif(TARGET_PLATFORM_OS STREQUAL "web")
        target_link_options(${target} INTERFACE "SHELL:--js-library=${PLATFORM_NATIVE_DIR}/web/library_platform.js")
        set_property(TARGET ${target} APPEND PROPERTY INTERFACE_LINK_DEPENDS "${PLATFORM_NATIVE_DIR}/web/library_platform.js")
    endif()
endfunction()

# platform
defold_add_library(platform STATIC ${_platform_sources} ${PLATFORM_NATIVE_SOURCES})
target_include_directories(platform PUBLIC "${PLAT_SRC_DIR}" "${PLATFORM_GENERATED_INCLUDE_DIR}")
target_compile_definitions(platform PRIVATE "DLIB_LOG_DOMAIN=\"PLATFORM\"")
defold_target_link_libraries(platform "${TARGET_PLATFORM}" SCOPE PUBLIC ${PLATFORM_DEPENDENCIES})

if (PLATFORM_USE_GLFW3)
    target_compile_definitions(platform PUBLIC DM_INPUT_USE_GLFW3)
elseif(PLATFORM_NATIVE_SOURCES)
    platform_configure_native(platform)
endif()

# platform_null
defold_add_library(platform_null STATIC
    "${PLAT_SRC_DIR}/window.cpp"
    "${PLAT_SRC_DIR}/platform_window_null.cpp")
target_include_directories(platform_null PUBLIC "${PLAT_SRC_DIR}" "${PLATFORM_GENERATED_INCLUDE_DIR}")
target_compile_definitions(platform_null PRIVATE "DLIB_LOG_DOMAIN=\"PLATFORM\"")
defold_target_link_libraries(platform_null "${TARGET_PLATFORM}" SCOPE PUBLIC ${PLATFORM_DEPENDENCIES})

# platform_vulkan includes either desktop surface support or the Android backend without GLES.
if (PLATFORM_USE_GLFW3 OR TARGET_PLATFORM_OS STREQUAL "android")
    set(_platform_vulkan_sources ${_platform_sources})
    if(PLATFORM_USE_GLFW3)
        list(APPEND _platform_vulkan_sources "${PLAT_SRC_DIR}/platform_window_glfw3_vulkan.cpp")
    else()
        list(APPEND _platform_vulkan_sources ${PLATFORM_NATIVE_VULKAN_SOURCES})
    endif()
    list(REMOVE_DUPLICATES _platform_vulkan_sources)
    defold_add_library(platform_vulkan STATIC ${_platform_vulkan_sources})
    target_include_directories(platform_vulkan PUBLIC "${PLAT_SRC_DIR}" "${PLATFORM_GENERATED_INCLUDE_DIR}")
    target_compile_definitions(platform_vulkan PRIVATE "DLIB_LOG_DOMAIN=\"PLATFORM\"")
    defold_target_link_libraries(platform_vulkan "${TARGET_PLATFORM}" SCOPE PUBLIC ${PLATFORM_DEPENDENCIES})
    if(TARGET_PLATFORM_OS STREQUAL "android")
        target_compile_definitions(platform_vulkan PRIVATE DM_PLATFORM_NO_OPENGL)
        platform_configure_native(platform_vulkan)
    endif()
endif()


# ********************* Install *********************

install(TARGETS platform platform_null
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})

if (TARGET platform_vulkan)
    install(TARGETS platform_vulkan
            ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
            LIBRARY DESTINATION lib/${TARGET_PLATFORM}
            RUNTIME DESTINATION bin/${TARGET_PLATFORM})
endif()

# Install public platform headers (platform_*.h) into include/platform
if(_DEFOLD_PLATFORM_HEADERS)
    install(FILES ${_DEFOLD_PLATFORM_HEADERS} DESTINATION include/platform)
endif()

defold_add_sdk_headers_target(platform_public_sdk_headers
    DESTINATION include/platform
    SOURCE_ROOT "${PLAT_SRC_DIR}"
    FILES
        ${_DEFOLD_PLATFORM_HEADERS}
        "${PLAT_SRC_DIR}/window.h"
        "${PLAT_SRC_DIR}/window.hpp"
    DEPENDS platform)
defold_add_sdk_headers_target(platform_dmsdk_sdk_headers
    DESTINATION sdk/include/dmsdk
    SOURCE_ROOT "${PLAT_SRC_DIR}/dmsdk"
    FILES "${PLAT_SRC_DIR}/dmsdk/platform/window.h"
    DEPENDS platform)
add_custom_target(platform_sdk_headers
    DEPENDS platform_public_sdk_headers platform_dmsdk_sdk_headers)
set_target_properties(platform PROPERTIES DEFOLD_SDK_HEADERS_TARGET platform_sdk_headers)

install(FILES
    "${PLAT_SRC_DIR}/window.h"
    "${PLAT_SRC_DIR}/window.hpp"
    DESTINATION include/platform)

install(FILES
    "${PLAT_SRC_DIR}/dmsdk/platform/window.h"
    DESTINATION sdk/include/dmsdk/platform)

if(TARGET_PLATFORM_OS STREQUAL "android")
    set(PLATFORM_ANDROID_CLASSES "${CMAKE_CURRENT_BINARY_DIR}/java")
    set(PLATFORM_ANDROID_JAR "${DEFOLD_SDK_ROOT}/share/java/platform_android.jar")
    file(GLOB_RECURSE PLATFORM_ANDROID_JAVA CONFIGURE_DEPENDS "${PLAT_SRC_DIR}/java/*.java")
    add_custom_command(OUTPUT "${PLATFORM_ANDROID_JAR}"
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${PLATFORM_ANDROID_CLASSES}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${PLATFORM_ANDROID_CLASSES}" "${DEFOLD_SDK_ROOT}/share/java"
        COMMAND "${DEFOLD_JAVAC_EXECUTABLE}" -g --release 8 -Xlint:-options
            -classpath "${DEFOLD_ANDROID_JAR}" -d "${PLATFORM_ANDROID_CLASSES}" ${PLATFORM_ANDROID_JAVA}
        COMMAND "${Java_JAR_EXECUTABLE}" cf "${PLATFORM_ANDROID_JAR}" -C "${PLATFORM_ANDROID_CLASSES}" .
        DEPENDS ${PLATFORM_ANDROID_JAVA} "${DEFOLD_ANDROID_JAR}"
        VERBATIM)
    add_custom_target(platform_android_jar ALL DEPENDS "${PLATFORM_ANDROID_JAR}")
    add_dependencies(platform platform_android_jar)
    add_dependencies(platform_vulkan platform_android_jar)
    install(FILES "${PLATFORM_ANDROID_JAR}" DESTINATION share/java)
elseif(TARGET_PLATFORM_OS STREQUAL "web")
    install(FILES "${PLATFORM_NATIVE_DIR}/web/library_platform.js" DESTINATION "lib/${TARGET_PLATFORM}/js")
endif()

# ********************* Tests *********************

if(BUILD_TESTS)
    add_subdirectory(src/test)
endif()
