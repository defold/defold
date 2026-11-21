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
if(NOT TARGET_PLATFORM_OS MATCHES "^(macos|win32)$")
    list(APPEND _DEFOLD_PLATFORM_HEADERS "${PLAT_SRC_DIR}/window_placement.h")
endif()
foreach(_platform_header IN LISTS _DEFOLD_PLATFORM_HEADERS)
    get_filename_component(_platform_header_name "${_platform_header}" NAME)
    configure_file("${_platform_header}" "${PLATFORM_GENERATED_INCLUDE_DIR}/platform/${_platform_header_name}" COPYONLY)
endforeach()
configure_file("${PLAT_SRC_DIR}/window.h" "${PLATFORM_GENERATED_INCLUDE_DIR}/platform/window.h" COPYONLY)
configure_file("${PLAT_SRC_DIR}/window.hpp" "${PLATFORM_GENERATED_INCLUDE_DIR}/platform/window.hpp" COPYONLY)

# Select platform sources (centralized)
set(PLATFORM_USE_LINUX OFF)
set(PLATFORM_USE_COCOA OFF)
set(PLATFORM_USE_WIN32 OFF)
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
elseif(TARGET_PLATFORM_OS STREQUAL "macos")
    set(PLATFORM_USE_COCOA ON)
    set(_platform_sources
        "${PLAT_SRC_DIR}/window.cpp"
        "${PLAT_SRC_DIR}/window_state.cpp"
        "${PLAT_SRC_DIR}/window_placement.cpp"
        "${PLAT_SRC_DIR}/window_desktop_constants.cpp"
        "${PLAT_SRC_DIR}/macos/window_macos.mm"
        "${PLAT_SRC_DIR}/macos/window_placement.mm"
        "${PLAT_SRC_DIR}/macos/window_input.mm"
        "${PLAT_SRC_DIR}/macos/window_joystick.mm"
        "${PLAT_SRC_DIR}/macos/window_opengl.mm")
elseif(TARGET_PLATFORM STREQUAL "x86_64-win32")
    set(PLATFORM_USE_WIN32 ON)
    set(_platform_sources
        "${PLAT_SRC_DIR}/window.cpp"
        "${PLAT_SRC_DIR}/window_state.cpp"
        "${PLAT_SRC_DIR}/window_placement.cpp"
        "${PLAT_SRC_DIR}/window_desktop_constants.cpp"
        "${PLAT_SRC_DIR}/win32/window_win32.cpp"
        "${PLAT_SRC_DIR}/win32/window_display.cpp"
        "${PLAT_SRC_DIR}/win32/window_placement.cpp"
        "${PLAT_SRC_DIR}/win32/window_input.cpp"
        "${PLAT_SRC_DIR}/win32/window_joystick.cpp"
        "${PLAT_SRC_DIR}/win32/joystick_driver.cpp"
        "${PLAT_SRC_DIR}/win32/joystick_packet.cpp"
        "${PLAT_SRC_DIR}/win32/window_opengl.cpp")
elseif(TARGET_PLATFORM_OS STREQUAL "linux")
    set(PLATFORM_USE_LINUX ON)
    include("${PLAT_SRC_DIR}/linux/wayland_protocols.cmake")
    set(_platform_sources
        "${PLAT_SRC_DIR}/window.cpp"
        "${PLAT_SRC_DIR}/window_state.cpp"
        "${PLAT_SRC_DIR}/window_placement.cpp"
        "${PLAT_SRC_DIR}/window_desktop_constants.cpp"
        "${PLAT_SRC_DIR}/linux/window_linux.cpp"
        "${PLAT_SRC_DIR}/linux/window_input.cpp"
        "${PLAT_SRC_DIR}/linux/window_joystick.cpp"
        "${PLAT_SRC_DIR}/linux/window_x11.cpp"
        "${PLAT_SRC_DIR}/linux/window_x11_keys.cpp"
        "${PLAT_SRC_DIR}/linux/wayland_client.cpp"
        "${PLAT_SRC_DIR}/linux/window_wayland.cpp"
        "${PLAT_SRC_DIR}/linux/window_wayland_frame.cpp"
        "${PLAT_SRC_DIR}/linux/window_wayland_input.cpp"
        "${PLAT_SRC_DIR}/linux/window_placement.cpp"
        "${PLAT_SRC_DIR}/linux/window_opengl.cpp"
        $<TARGET_OBJECTS:platform_wayland_protocols>)
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

# The mobile and web backends are engine sources, built with the platform library.
set(PLATFORM_NATIVE_DIR "${PLAT_SRC_DIR}/native")
set(PLATFORM_NATIVE_SOURCES)
set(PLATFORM_NATIVE_VULKAN_SOURCES)
if(NOT PLATFORM_USE_LINUX AND NOT PLATFORM_USE_COCOA AND NOT PLATFORM_USE_WIN32 AND NOT DEFOLD_IS_PRIVATE_VENDOR AND NOT DEFOLD_PLATFORM_WINDOW_SOURCES)
    if(TARGET_PLATFORM_OS STREQUAL "web")
        set(PLATFORM_NATIVE_SOURCES "${PLATFORM_NATIVE_DIR}/web/web.c" "${PLATFORM_NATIVE_DIR}/native_touch.c" "${PLATFORM_NATIVE_DIR}/joystick_guid.c")
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
            list(FILTER PLATFORM_NATIVE_VULKAN_SOURCES EXCLUDE REGEX "/(android_util|android_window_gl)\\.c$")
            list(FILTER PLATFORM_NATIVE_SOURCES EXCLUDE REGEX "/android_window_no_api\\.c$")
        endif()
    endif()
endif()

function(platform_configure_native target)
    target_include_directories(${target} PRIVATE "${PLATFORM_NATIVE_DIR}" "${PLATFORM_NATIVE_DIR}/${TARGET_PLATFORM_OS}")
    target_compile_definitions(${target} PRIVATE SDL_JOYSTICK_IOKIT)
    if(TARGET_PLATFORM_OS STREQUAL "ios")
        target_link_libraries(${target} PUBLIC ${DEFOLD_IOS_WINDOW_FRAMEWORKS})
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

# Debug HTTP service. The final engine chooses platform/platform_vulkan/platform_null;
# linking a concrete backend here would pull a native window into headless engines.
defold_add_library(platform_service STATIC
  "${PLAT_SRC_DIR}/platform_service.cpp"
  "${PLAT_SRC_DIR}/platform_service_extension.cpp")
target_include_directories(platform_service PUBLIC "${PLAT_SRC_DIR}" "${PLATFORM_GENERATED_INCLUDE_DIR}")
target_compile_definitions(platform_service PRIVATE DLIB_LOG_DOMAIN="PLATFORM")
defold_target_link_libraries(platform_service "${TARGET_PLATFORM}" SCOPE PUBLIC dlib)

# Graphics is configured later in full builds; standalone builds use installed headers.
function(platform_service_add_graphics_header_dependency)
    if(TARGET graphics_sdk_headers)
        add_dependencies(platform_service graphics_sdk_headers)
    endif()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}"
    CALL platform_service_add_graphics_header_dependency)

# platform
defold_add_library(platform STATIC ${_platform_sources} ${PLATFORM_NATIVE_SOURCES})
target_include_directories(platform PUBLIC "${PLAT_SRC_DIR}" "${PLATFORM_GENERATED_INCLUDE_DIR}")
target_compile_definitions(platform PRIVATE "DLIB_LOG_DOMAIN=\"PLATFORM\"")
defold_target_link_libraries(platform "${TARGET_PLATFORM}" SCOPE PUBLIC ${PLATFORM_DEPENDENCIES})

if (PLATFORM_USE_LINUX)
    target_include_directories(platform BEFORE PRIVATE "${_wayland_generated}")
    target_link_libraries(platform PUBLIC ${DEFOLD_LINUX_WINDOW_LIBRARIES})
elseif(PLATFORM_USE_COCOA)
    target_link_libraries(platform PUBLIC ${DEFOLD_COCOA_WINDOW_FRAMEWORKS})
elseif(PLATFORM_USE_WIN32)
    target_link_libraries(platform PUBLIC ${DEFOLD_WIN32_WINDOW_LIBRARIES})
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
if (PLATFORM_USE_LINUX OR PLATFORM_USE_COCOA OR PLATFORM_USE_WIN32 OR TARGET_PLATFORM_OS STREQUAL "android")
    set(_platform_vulkan_sources ${_platform_sources})
    if(PLATFORM_USE_COCOA)
        list(APPEND _platform_vulkan_sources "${PLAT_SRC_DIR}/macos/window_vulkan.mm")
    elseif(PLATFORM_USE_WIN32)
        list(APPEND _platform_vulkan_sources "${PLAT_SRC_DIR}/win32/window_vulkan.cpp")
    elseif(PLATFORM_USE_LINUX)
        list(APPEND _platform_vulkan_sources "${PLAT_SRC_DIR}/linux/window_vulkan.cpp")
    else()
        list(APPEND _platform_vulkan_sources ${PLATFORM_NATIVE_VULKAN_SOURCES})
    endif()
    list(REMOVE_DUPLICATES _platform_vulkan_sources)
    defold_add_library(platform_vulkan STATIC ${_platform_vulkan_sources})
    target_include_directories(platform_vulkan PUBLIC "${PLAT_SRC_DIR}" "${PLATFORM_GENERATED_INCLUDE_DIR}")
    target_compile_definitions(platform_vulkan PRIVATE "DLIB_LOG_DOMAIN=\"PLATFORM\"")
    defold_target_link_libraries(platform_vulkan "${TARGET_PLATFORM}" SCOPE PUBLIC ${PLATFORM_DEPENDENCIES})
    if(PLATFORM_USE_LINUX)
        target_include_directories(platform_vulkan BEFORE PRIVATE "${_wayland_generated}")
        target_link_libraries(platform_vulkan PUBLIC ${DEFOLD_LINUX_WINDOW_LIBRARIES} X11-xcb)
    elseif(PLATFORM_USE_COCOA)
        target_link_libraries(platform_vulkan PUBLIC ${DEFOLD_COCOA_WINDOW_FRAMEWORKS})
    elseif(PLATFORM_USE_WIN32)
        target_link_libraries(platform_vulkan PUBLIC ${DEFOLD_WIN32_WINDOW_LIBRARIES})
    elseif(TARGET_PLATFORM_OS STREQUAL "android")
        target_compile_definitions(platform_vulkan PRIVATE DM_PLATFORM_NO_OPENGL)
        platform_configure_native(platform_vulkan)
    endif()
endif()


# ********************* Install *********************

install(TARGETS platform platform_null platform_service
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
if(TARGET platform_vulkan)
    set_target_properties(platform_vulkan PROPERTIES DEFOLD_SDK_HEADERS_TARGET platform_sdk_headers)
endif()

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
