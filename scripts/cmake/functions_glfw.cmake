message("functions_glfw.cmake:")

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
        win32
        x86_64-linux
        arm64-linux)

    if("${PLATFORM}" IN_LIST _DEFOLD_GLFW3_PLATFORMS)
        set(${OUT_VAR} 3 PARENT_SCOPE)
    else()
        set(${OUT_VAR} 2 PARENT_SCOPE)
    endif()
endfunction()

