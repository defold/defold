defold_log("functions_glfw.cmake:")

# Determine GLFW major version (2 or 3) for a given target platform.
# Mirrors build_tools/waf_dynamo.py: platform_glfw_version(platform)
#
# Usage:
#   include(functions_glfw)
#   defold_get_glfw_version(_GLFW_VER "x86_64-macos")
#   # -> _GLFW_VER set to 3

function(defold_get_glfw_version OUT_VAR PLATFORM)
    if(NOT PLATFORM)
        message(FATAL_ERROR "defold_get_glfw_version: PLATFORM argument is required")
    endif()

    # Platforms using GLFW 3 in Waf
    set(_DEFOLD_GLFW3_PLATFORMS
        x86_64-macos
        arm64-macos
        x86_64-win32
        x86-win32
        x86_64-linux
        arm64-linux)

    if("${PLATFORM}" IN_LIST _DEFOLD_GLFW3_PLATFORMS)
        set(${OUT_VAR} 3 PARENT_SCOPE)
    else()
        set(${OUT_VAR} 2 PARENT_SCOPE)
    endif()
endfunction()


# Link the appropriate GLFW library for a given platform tuple.
# - For platforms that use GLFW 3 in Waf (macOS, Linux, Win32) link against 'glfw3'.
# - Otherwise link against 'dmglfw'.
# Usage:
#   defold_target_link_glfw(<target> <platform> [SCOPE <PRIVATE|PUBLIC|INTERFACE>])
function(defold_target_link_glfw target platform)
    set(options)
    set(oneValueArgs SCOPE)
    set(multiValueArgs)
    cmake_parse_arguments(DGLFW "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if(NOT DGLFW_SCOPE)
        set(DGLFW_SCOPE PRIVATE)
    endif()

    if(NOT target OR NOT platform)
        message(FATAL_ERROR "defold_target_link_glfw: target and platform are required")
    endif()

    # Determine GLFW version for this platform
    defold_get_glfw_version(_GLFW_VER "${platform}")

    if(_GLFW_VER EQUAL 3)
        set(_glfw_lib glfw3)
    else()
        set(_glfw_lib dmglfw)
    endif()

    # If platform OS is win32, our prebuilt static libs are prefixed with "lib"
    string(REGEX REPLACE "^[^-]+-" "" _PLAT_OS "${platform}")
    if(_PLAT_OS STREQUAL "win32")
        if(NOT _glfw_lib MATCHES "^lib")
            set(_glfw_lib "lib${_glfw_lib}")
        endif()
    endif()

    target_link_libraries(${target} ${DGLFW_SCOPE} ${_glfw_lib})
endfunction()
