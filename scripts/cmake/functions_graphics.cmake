message("functions_graphics.cmake:")

# Returns a CMake list of exported graphics adapter symbols for a given
# TARGET_PLATFORM-like tuple (e.g., x86_64-macos, arm64-linux, x86_64-win32).
#
# The selection mirrors waf_dynamo.py: platform_graphics_libs_and_symbols.
# It considers the toggles WITH_OPENGL, WITH_VULKAN, WITH_DX12, WITH_WEBGPU
# if they are defined in the including scope; otherwise assumes OFF.
#
# Usage:
#   include(functions_graphics)
#   defold_get_graphics_symbols_for_platform(_SYMS "x86_64-macos")
#   # _SYMS now contains a semicolon-separated list, e.g. "GraphicsAdapterOpenGL;GraphicsAdapterVulkan"

function(defold_get_graphics_symbols_for_platform OUT_VAR PLATFORM)
    # Read feature toggles with default OFF if not defined
    set(_WITH_OPENGL  OFF)
    set(_WITH_VULKAN  OFF)
    set(_WITH_DX12    OFF)
    set(_WITH_WEBGPU  OFF)
    if(DEFINED WITH_OPENGL AND WITH_OPENGL)
        set(_WITH_OPENGL ON)
    endif()
    if(DEFINED WITH_VULKAN AND WITH_VULKAN)
        set(_WITH_VULKAN ON)
    endif()
    if(DEFINED WITH_DX12 AND WITH_DX12)
        set(_WITH_DX12 ON)
    endif()
    if(DEFINED WITH_WEBGPU AND WITH_WEBGPU)
        set(_WITH_WEBGPU ON)
    endif()

    set(_use_opengl   OFF)
    set(_use_opengles OFF)
    set(_use_vulkan   OFF)

    # Base selection per platform
    if(PLATFORM STREQUAL "arm64-macos" OR PLATFORM STREQUAL "x86_64-macos" OR PLATFORM STREQUAL "arm64-nx64")
        set(_use_opengl ${_WITH_OPENGL})
        set(_use_vulkan ON)
    elseif(PLATFORM STREQUAL "arm64-linux")
        set(_use_opengles ON)
        set(_use_vulkan ${_WITH_VULKAN})
    else()
        set(_use_opengl ON)
        set(_use_vulkan ${_WITH_VULKAN})
    endif()

    # Compute symbols
    set(_SYMS)

    # Platform overrides
    if(PLATFORM STREQUAL "arm64-nx64")
        list(APPEND _SYMS GraphicsAdapterVulkan)
    elseif(PLATFORM STREQUAL "x86_64-ps4")
        list(APPEND _SYMS GraphicsAdapterPS4)
    elseif(PLATFORM STREQUAL "x86_64-ps5")
        list(APPEND _SYMS GraphicsAdapterPS5)
    else()
        if(_use_opengles)
            list(APPEND _SYMS GraphicsAdapterOpenGLES)
        elseif(_use_opengl)
            list(APPEND _SYMS GraphicsAdapterOpenGL)
        endif()
        if(_use_vulkan)
            list(APPEND _SYMS GraphicsAdapterVulkan)
        endif()
        if(_WITH_DX12 AND PLATFORM STREQUAL "x86_64-win32")
            list(APPEND _SYMS GraphicsAdapterDX12)
        endif()
        if(_WITH_WEBGPU AND (PLATFORM STREQUAL "js-web" OR PLATFORM STREQUAL "wasm-web" OR PLATFORM STREQUAL "wasm_pthread-web"))
            list(APPEND _SYMS GraphicsAdapterWebGPU)
        endif()
    endif()

    set(${OUT_VAR} "${_SYMS}" PARENT_SCOPE)
endfunction()

