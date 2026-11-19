defold_log("functions_glfw.cmake:")

# Determine GLFW major version (3 for Linux, 0 for native backends) for a given target platform.
#
# Usage:
#   include(functions_glfw)
#   defold_get_glfw_version(_GLFW_VER "x86_64-linux")
#   # -> _GLFW_VER set to 3

function(defold_get_glfw_version OUT_VAR PLATFORM)
    if(NOT PLATFORM)
        message(FATAL_ERROR "defold_get_glfw_version: PLATFORM argument is required")
    endif()

    # Platforms using GLFW 3
    set(_DEFOLD_GLFW3_PLATFORMS
        x86_64-linux
        arm64-linux)

    if("${PLATFORM}" IN_LIST _DEFOLD_GLFW3_PLATFORMS)
        set(${OUT_VAR} 3 PARENT_SCOPE)
    else()
        set(${OUT_VAR} 0 PARENT_SCOPE)
    endif()
endfunction()


# Link the appropriate GLFW library for a given platform tuple.
# - For platforms that use GLFW 3 (Linux) link against 'glfw3'.
# - Mobile and web implementations are part of the platform library.
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

    if("${platform}" STREQUAL "x86_64-xbone" OR (DEFINED DEFOLD_PLATFORM_HAS_GLFW AND NOT DEFOLD_PLATFORM_HAS_GLFW))
        return()
    endif()

    # Determine GLFW version for this platform
    defold_get_glfw_version(_GLFW_VER "${platform}")

    if(_GLFW_VER EQUAL 3)
        set(_glfw_lib glfw3)
        if(WITH_MSAN)
            defold_import_external(glfw3 "${DEFOLD_SDK_ROOT}/ext/lib/${platform}/libglfw3.a")
        endif()
    else()
        return()
    endif()

    target_link_libraries(${target} ${DGLFW_SCOPE} ${_glfw_lib})
endfunction()
