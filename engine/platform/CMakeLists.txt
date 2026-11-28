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
set(_DEFOLD_PLATFORM_HEADERS
    "${PLAT_SRC_DIR}/platform_app.h"
    "${PLAT_SRC_DIR}/platform_window_android.h"
    "${PLAT_SRC_DIR}/platform_window_constants.h"
    "${PLAT_SRC_DIR}/platform_window_ios.h"
    "${PLAT_SRC_DIR}/platform_window_linux.h"
    "${PLAT_SRC_DIR}/platform_window_opengl.h"
    "${PLAT_SRC_DIR}/platform_window_osx.h"
    "${PLAT_SRC_DIR}/platform_window_vulkan.h"
    "${PLAT_SRC_DIR}/platform_window_win32.h"
    "${PLAT_SRC_DIR}/window.h"
    "${PLAT_SRC_DIR}/window.hpp")
if(NOT TARGET_PLATFORM_OS MATCHES "^(macos|win32)$")
    list(APPEND _DEFOLD_PLATFORM_HEADERS "${PLAT_SRC_DIR}/window_placement.h")
endif()
foreach(_platform_header IN LISTS _DEFOLD_PLATFORM_HEADERS)
    get_filename_component(_platform_header_name "${_platform_header}" NAME)
    configure_file("${_platform_header}" "${PLATFORM_GENERATED_INCLUDE_DIR}/platform/${_platform_header_name}" COPYONLY)
endforeach()

# Select platform sources (centralized)
set(_platform_mobile_web_sources
    "${PLAT_SRC_DIR}/window.cpp"
    "${PLAT_SRC_DIR}/window_mobile_web_constants.cpp"
    "${PLAT_SRC_DIR}/window_touch.c"
    "${PLAT_SRC_DIR}/joystick_guid.c")
set(_platform_mobile_sources
    ${_platform_mobile_web_sources}
    "${PLAT_SRC_DIR}/mobile_shared/window.cpp"
    "${PLAT_SRC_DIR}/mobile_shared/window_init.c"
    "${PLAT_SRC_DIR}/mobile_shared/window_input.c"
    "${PLAT_SRC_DIR}/mobile_shared/window_opengl.cpp")

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
        "${PLAT_SRC_DIR}/macos/window_opengl.mm"
        "${PLAT_SRC_DIR}/window_egl.cpp")
elseif(TARGET_PLATFORM_OS STREQUAL "win32")
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
        "${PLAT_SRC_DIR}/win32/window_opengl.cpp"
        "${PLAT_SRC_DIR}/window_egl.cpp")
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
        "${PLAT_SRC_DIR}/linux/window_wayland_session.cpp"
        "${PLAT_SRC_DIR}/linux/window_placement.cpp"
        "${PLAT_SRC_DIR}/linux/window_opengl.cpp"
        $<TARGET_OBJECTS:platform_wayland_protocols>)
elseif(TARGET_PLATFORM_OS STREQUAL "android")
    set(PLATFORM_USE_ANDROID ON)
    set(_platform_sources
        ${_platform_mobile_sources}
        "${PLAT_SRC_DIR}/android/app_android.cpp"
        "${PLAT_SRC_DIR}/android/platform_window_android.cpp"
        "${PLAT_SRC_DIR}/android/android_init.c"
        "${PLAT_SRC_DIR}/android/android_jni.c"
        "${PLAT_SRC_DIR}/android/android_native_app_glue.c"
        "${PLAT_SRC_DIR}/android/window_android.c"
        "${PLAT_SRC_DIR}/android/window_joystick.c")
    set(_platform_gles_sources
        "${PLAT_SRC_DIR}/android/android_util.c"
        "${PLAT_SRC_DIR}/android/window_opengl.c")
elseif(TARGET_PLATFORM_OS STREQUAL "ios")
    set(PLATFORM_USE_IOS ON)
    set(_platform_sources
        ${_platform_mobile_sources}
        "${PLAT_SRC_DIR}/ios/app_ios.mm"
        "${PLAT_SRC_DIR}/ios/platform_window_ios.mm"
        "${PLAT_SRC_DIR}/ios/window_init.m"
        "${PLAT_SRC_DIR}/ios/window_ios.m"
        "${PLAT_SRC_DIR}/ios/app/AppDelegate.m"
        "${PLAT_SRC_DIR}/ios/app/AppDelegateProxy.m"
        "${PLAT_SRC_DIR}/ios/app/BaseView.m"
        "${PLAT_SRC_DIR}/ios/app/EAGLView.m"
        "${PLAT_SRC_DIR}/ios/app/MetalView.m"
        "${PLAT_SRC_DIR}/ios/app/SceneDelegate.m"
        "${PLAT_SRC_DIR}/ios/app/TextUtil.m"
        "${PLAT_SRC_DIR}/ios/app/ViewController.m")
elseif(TARGET_PLATFORM_OS STREQUAL "web")
    set(PLATFORM_USE_WEB ON)
    set(_platform_sources ${_platform_mobile_web_sources} "${PLAT_SRC_DIR}/web/window_web.cpp")
else()
    message(FATAL_ERROR "No window backend configured for ${TARGET_PLATFORM}")
endif()

# Debug HTTP service. The final engine chooses platform/platform_vulkan/platform_null;
# linking a concrete backend here would pull a native window into headless engines.
defold_add_library(platform_service STATIC
  "${PLAT_SRC_DIR}/service/platform_service.cpp"
  "${PLAT_SRC_DIR}/service/platform_service_extension.cpp")
target_include_directories(platform_service PRIVATE "${PLAT_SRC_DIR}" "${PLATFORM_GENERATED_INCLUDE_DIR}")
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
defold_add_library(platform STATIC ${_platform_sources} ${_platform_gles_sources})
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
elseif(PLATFORM_USE_ANDROID)
    target_link_libraries(platform PUBLIC android log EGL GLESv1_CM GLESv2)
elseif(PLATFORM_USE_IOS)
    target_link_libraries(platform PUBLIC ${DEFOLD_IOS_WINDOW_FRAMEWORKS})
elseif(PLATFORM_USE_WEB)
    target_link_options(platform INTERFACE "SHELL:--js-library=${PLAT_SRC_DIR}/web/library_platform.js")
    set_property(TARGET platform APPEND PROPERTY INTERFACE_LINK_DEPENDS "${PLAT_SRC_DIR}/web/library_platform.js")
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
    elseif(PLATFORM_USE_ANDROID)
        list(APPEND _platform_vulkan_sources "${PLAT_SRC_DIR}/android/window_no_api.c")
    endif()
    list(REMOVE_DUPLICATES _platform_vulkan_sources)
    defold_add_library(platform_vulkan STATIC ${_platform_vulkan_sources})
    target_include_directories(platform_vulkan PUBLIC "${PLAT_SRC_DIR}" "${PLATFORM_GENERATED_INCLUDE_DIR}")
    target_compile_definitions(platform_vulkan PRIVATE "DLIB_LOG_DOMAIN=\"PLATFORM\"")
    defold_target_link_libraries(platform_vulkan "${TARGET_PLATFORM}" SCOPE PUBLIC ${PLATFORM_DEPENDENCIES})
    if(PLATFORM_USE_LINUX)
        target_include_directories(platform_vulkan BEFORE PRIVATE "${_wayland_generated}")
        target_link_libraries(platform_vulkan PUBLIC ${DEFOLD_LINUX_WINDOW_LIBRARIES})
    elseif(PLATFORM_USE_COCOA)
        target_link_libraries(platform_vulkan PUBLIC ${DEFOLD_COCOA_WINDOW_FRAMEWORKS})
    elseif(PLATFORM_USE_WIN32)
        target_link_libraries(platform_vulkan PUBLIC ${DEFOLD_WIN32_WINDOW_LIBRARIES})
    elseif(TARGET_PLATFORM_OS STREQUAL "android")
        target_compile_definitions(platform_vulkan PRIVATE DM_PLATFORM_NO_OPENGL)
        target_link_libraries(platform_vulkan PUBLIC android log)
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

install(FILES ${_DEFOLD_PLATFORM_HEADERS} DESTINATION include/platform)

defold_add_sdk_headers_target(platform_public_sdk_headers
    DESTINATION include/platform
    SOURCE_ROOT "${PLAT_SRC_DIR}"
    FILES ${_DEFOLD_PLATFORM_HEADERS}
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
    "${PLAT_SRC_DIR}/dmsdk/platform/window.h"
    DESTINATION sdk/include/dmsdk/platform)

if(TARGET_PLATFORM_OS STREQUAL "android")
    set(PLATFORM_ANDROID_CLASSES "${CMAKE_CURRENT_BINARY_DIR}/java")
    set(PLATFORM_ANDROID_JAR "${DEFOLD_SDK_ROOT}/share/java/platform_android.jar")
    set(PLATFORM_ANDROID_JAVA "${PLAT_SRC_DIR}/android/java/com/dynamo/android/DefoldActivity.java")
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
    install(FILES "${PLAT_SRC_DIR}/web/library_platform.js" DESTINATION "lib/${TARGET_PLATFORM}/js")
endif()

# ********************* Tests *********************

if(BUILD_TESTS)
    add_subdirectory(src/test)
endif()
