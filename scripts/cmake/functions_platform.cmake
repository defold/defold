defold_log("functions_platform.cmake:")

set(DEFOLD_WIN32_WINDOW_LIBRARIES user32 gdi32 opengl32 imm32 dinput8 dxguid)
set(DEFOLD_COCOA_WINDOW_FRAMEWORKS "-framework AppKit" "-framework CoreGraphics" "-framework IOKit" "-framework QuartzCore")

# Link the correct platform library for a target and platform tuple.
#
# Usage:
#   include(functions_platform)
#   defold_target_link_platform(<target> <platform> [SCOPE <PRIVATE|PUBLIC|INTERFACE>])
#
# Selection rules:
# - A private platform may explicitly select its platform library
# - Web targets -> platform
# - Desktop targets (macOS, Linux, Win32):
#     - If WITH_VULKAN=ON
#       -> platform_vulkan
#     - Else -> platform
# - Other targets -> platform

function(defold_target_link_platform target platform)
    set(options)
    set(oneValueArgs SCOPE)
    set(multiValueArgs)
    cmake_parse_arguments(DPL "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if(NOT DPL_SCOPE)
        set(DPL_SCOPE PRIVATE)
    endif()

    if(NOT platform)
        message(FATAL_ERROR "functions_platform: platform argument is required")
    endif()

    if(DEFOLD_PLATFORM_WINDOW_LIBRARY)
        set(_plat_lib "${DEFOLD_PLATFORM_WINDOW_LIBRARY}")
    elseif(WITH_VULKAN AND (platform MATCHES "^(arm64|x86_64)-(macos|linux)$" OR platform STREQUAL "x86_64-win32"))
        set(_plat_lib platform_vulkan)
    elseif(platform MATCHES "android$" AND WITH_VULKAN AND NOT WITH_OPENGL)
        set(_plat_lib platform_vulkan)
    else()
        set(_plat_lib platform)
    endif()

    # Prefer linking to a CMake target if it exists
    target_link_libraries(${target} ${DPL_SCOPE} ${_plat_lib})

    if(NOT TARGET "${_plat_lib}")
        # Installed archives do not carry the CMake target's link requirements.
        if(platform MATCHES "^(arm64|x86_64)-macos$" AND _plat_lib MATCHES "^platform(_vulkan)?$")
            target_link_libraries(${target} ${DPL_SCOPE} ${DEFOLD_COCOA_WINDOW_FRAMEWORKS})
        elseif(platform STREQUAL "x86_64-win32" AND _plat_lib MATCHES "^platform(_vulkan)?$")
            target_link_libraries(${target} ${DPL_SCOPE} ${DEFOLD_WIN32_WINDOW_LIBRARIES})
        elseif(_plat_lib STREQUAL "platform")
            defold_target_link_libraries_web(${target} "${platform}" SCOPE ${DPL_SCOPE} library_platform.js)
        endif()
    endif()
endfunction()


# Adds socket-related system libraries per platform (mirrors waf_dynamo.py LIB_PLATFORM_SOCKET).
# Usage:
#   defold_target_link_socket(<target> <platform> [SCOPE <PRIVATE|PUBLIC|INTERFACE>])
function(defold_target_link_socket target platform)
    set(options)
    set(oneValueArgs SCOPE)
    set(multiValueArgs)
    cmake_parse_arguments(DPLS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if(NOT DPLS_SCOPE)
        set(DPLS_SCOPE PRIVATE)
    endif()

    if(NOT platform)
        message(FATAL_ERROR "functions_platform: platform argument is required for defold_target_link_socket")
    endif()

    # Derive OS from tuple (e.g., x86_64-win32 -> win32)
    string(REGEX REPLACE "^[^-]+-" "" _PLAT_OS "${platform}")

    set(_socket_libs)

    if(_PLAT_OS STREQUAL "win32")
        # Based on waf_dynamo.py: WS2_32 Iphlpapi AdvAPI32 Bcrypt.lib
        list(APPEND _socket_libs WS2_32.lib Iphlpapi.lib AdvAPI32.lib Bcrypt.lib)
    elseif(_PLAT_OS STREQUAL "xbone")
        # Based on waf_dynamo_vendor.py: LIB_SOCKET and LINKFLAGS_DLIB.
        list(APPEND _socket_libs Ws2_32.lib Iphlpapi.lib Bcrypt.lib)
    else()
        # Other platforms do not require additional socket libs in waf
    endif()

    if(DEFOLD_PLATFORM_SOCKET_LIBS)
        list(APPEND _socket_libs ${DEFOLD_PLATFORM_SOCKET_LIBS})
    endif()

    if(_socket_libs)
        target_link_libraries(${target} ${DPLS_SCOPE} ${_socket_libs})
    endif()
endfunction()
