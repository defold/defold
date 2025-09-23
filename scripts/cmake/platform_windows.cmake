message("platform_windows.cmake:")

if(NOT WIN32)
    message(FATAL_ERROR "platform_windows.cmake included on non-Windows host")
endif()

# Ensure C++11 on Windows builds
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Detect compiler front-end (prefer inspecting CMAKE_C_COMPILER)
set(_DEFOLD_MSVC_LIKE OFF)
set(_DEFOLD_CLANG_CL  OFF)
set(_DEFOLD_MSVC_NATIVE OFF)

if(CMAKE_C_COMPILER)
    get_filename_component(_defold_cc_name "${CMAKE_C_COMPILER}" NAME)
    string(TOLOWER "${_defold_cc_name}" _defold_cc_name_lc)
    if(_defold_cc_name_lc MATCHES "^clang-cl(\\.exe)?$")
        set(_DEFOLD_MSVC_LIKE ON)
        set(_DEFOLD_CLANG_CL ON)
    elseif(_defold_cc_name_lc MATCHES "^cl(\\.exe)?$")
        set(_DEFOLD_MSVC_LIKE ON)
        set(_DEFOLD_MSVC_NATIVE ON)
    endif()
endif()

# Common compile definitions (mirrors waf_dynamo.py for Win32)
add_compile_definitions(
    DM_PLATFORM_WINDOWS
    __STDC_LIMIT_MACROS
    DDF_EXPOSE_DESCRIPTORS
    WINVER=0x0600
    _WIN32_WINNT=0x0600
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
    UNICODE
    _UNICODE
)

# Use static multithreaded runtime (/MT) like waf_dynamo
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded" CACHE STRING "Use static MSVC runtime (/MT)" FORCE)

# Compiler options for MSVC or clang-cl
if(_DEFOLD_MSVC_LIKE)
    # /Oy- Disable frame pointer omission (keeps stack traces reliable)
    # /Z7  Generate debug info in .obj (matches waf_dynamo)
    add_compile_options(/Oy- /Z7)
else()
    message(WARNING "platform_windows: Non-MSVC-like compiler detected (${CMAKE_CXX_COMPILER_ID}). Skipping MSVC flags; link libs will still be added.")
endif()
