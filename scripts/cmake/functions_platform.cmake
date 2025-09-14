message("functions_platform.cmake:")

# Link the correct platform library for a target and platform tuple.
# Mirrors waf_dynamo.py platform_get_platform_lib/platform_glfw_version.
#
# Usage:
#   include(functions_platform)
#   defold_target_link_platform(<target> <platform> [SCOPE <PRIVATE|PUBLIC|INTERFACE>])
#
# Selection rules:
# - Web targets -> platform
# - GLFW3 desktop targets (macOS, Linux, Win32, Nx64):
#     - If WITH_VULKAN=ON OR platform in {arm64-macos,x86_64-macos,arm64-nx64}
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

    # Web platforms map to platform
    if(platform STREQUAL "js-web" OR platform STREQUAL "wasm-web" OR platform STREQUAL "wasm_pthread-web")
        set(_plat_lib platform)
    else()
        # Platforms using GLFW 3 (same set as waf's platform_glfw_version == 3)
        set(_glfw3_platforms "x86_64-macos;arm64-macos;x86_64-win32;x86_64-linux;arm64-linux;arm64-nx64")

        list(FIND _glfw3_platforms "${platform}" _idx)
        if(NOT _idx EQUAL -1)
            # Vulkan if requested or on macOS/Nx64
            set(_WITH_VULKAN OFF)
            if(DEFINED WITH_VULKAN AND WITH_VULKAN)
                set(_WITH_VULKAN ON)
            endif()

            if(_WITH_VULKAN OR platform STREQUAL "arm64-macos" OR platform STREQUAL "x86_64-macos" OR platform STREQUAL "arm64-nx64")
                set(_plat_lib platform_vulkan)
            else()
                set(_plat_lib platform)
            endif()
        else()
            set(_plat_lib platform)
        endif()
    endif()

    target_link_libraries(${target} ${DPL_SCOPE} ${_plat_lib})
endfunction()
