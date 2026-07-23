if(DEFINED DEFOLD_PLATFORM_CMAKE_INCLUDED)
    return()
endif()
set(DEFOLD_PLATFORM_CMAKE_INCLUDED ON)

defold_log("platform.cmake:")

# Detect host OS/arch and compute HOST_PLATFORM
include(platform_host)

# Best-effort platform inference (can be overridden with -DTARGET_PLATFORM=...)
set(TARGET_PLATFORM "" CACHE STRING "Defold platform tuple (e.g. x86_64-macos, arm64-macos, x86_64-linux, x86_64-win32)")
if(NOT TARGET_PLATFORM)
    set(TARGET_PLATFORM "${HOST_PLATFORM}")
endif()

if(NOT TARGET_PLATFORM)
    message(FATAL_ERROR "Could not infer TARGET_PLATFORM. Please pass -DTARGET_PLATFORM=<2-tuple> (e.g. x86_64-macos)")
endif()

defold_log("TARGET_PLATFORM: ${TARGET_PLATFORM}")

# Derive OS part from TARGET_PLATFORM tuple (e.g., x86_64-win32 -> win32)
string(REGEX REPLACE "^[^-]+-" "" TARGET_PLATFORM_OS "${TARGET_PLATFORM}")
defold_log("TARGET_PLATFORM_OS: ${TARGET_PLATFORM_OS}")

# Include the sdk
include(sdk)

# Global target aggregating test build dependencies across subprojects.
add_custom_target(build_tests)


#**************************************************************************
# Common compile settings
# Provide the C++ standard via target-level usage requirements
target_compile_features(defold_sdk INTERFACE cxx_std_11)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

if (TARGET_PLATFORM MATCHES "arm64-macos|x86_64-macos")
        include(platform_macos)
elseif (TARGET_PLATFORM MATCHES "arm64-ios|x86_64-ios")
        include(platform_ios)
elseif (TARGET_PLATFORM MATCHES "armv7-android|arm64-android")
        include(platform_android)
elseif (TARGET_PLATFORM MATCHES "wasm-web|wasm_pthread-web")
        include(platform_html5)
elseif (TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
        include(platform_linux)
elseif (TARGET_PLATFORM MATCHES "arm64-win32|x86_64-win32|x86-win32")
        include(platform_windows)
elseif (TARGET_PLATFORM MATCHES "x86_64-xbone")
        defold_get_private_repo_root(_DEFOLD_XBOX_PRIVATE_REPO_ROOT "${TARGET_PLATFORM}")
        if(NOT _DEFOLD_XBOX_PRIVATE_REPO_ROOT)
            message(FATAL_ERROR "Private Xbox platform requires a private repo root for ${TARGET_PLATFORM}")
        endif()

        set(_DEFOLD_XBOX_PLATFORM_MODULE "")
        foreach(_DEFOLD_XBOX_PLATFORM_MODULE_NAME IN ITEMS platform_xbox platform_xbone)
            include("${_DEFOLD_XBOX_PRIVATE_REPO_ROOT}/scripts/cmake/${_DEFOLD_XBOX_PLATFORM_MODULE_NAME}.cmake" OPTIONAL RESULT_VARIABLE _DEFOLD_XBOX_PLATFORM_MODULE)
            if(_DEFOLD_XBOX_PLATFORM_MODULE)
                break()
            endif()
        endforeach()
        if(NOT _DEFOLD_XBOX_PLATFORM_MODULE)
            message(FATAL_ERROR "Private Xbox platform module not found in ${_DEFOLD_XBOX_PRIVATE_REPO_ROOT}/scripts/cmake")
        endif()
        unset(_DEFOLD_XBOX_PLATFORM_MODULE)
        unset(_DEFOLD_XBOX_PLATFORM_MODULE_NAME)
        unset(_DEFOLD_XBOX_PRIVATE_REPO_ROOT)
elseif (TARGET_PLATFORM MATCHES "arm64-nx64")
        # Mark this configuration as using a private vendor platform (e.g., Switch)
        set(DEFOLD_IS_PRIVATE_VENDOR ON CACHE BOOL "Building with private vendor platform configuration" FORCE)
        include(platform_vendor_switch)
else()
        set(DEFOLD_IS_PRIVATE_VENDOR ON CACHE BOOL "Building with private vendor platform configuration" FORCE)
        include("platform_${TARGET_PLATFORM_OS}" OPTIONAL RESULT_VARIABLE _DEFOLD_PRIVATE_PLATFORM_MODULE)
        if(NOT _DEFOLD_PRIVATE_PLATFORM_MODULE)
            message(FATAL_ERROR "Unsupported platform: ${TARGET_PLATFORM}")
        endif()
endif()

#**************************************************************************
# Common defines

# Attach common definitions to defold_sdk target
target_compile_definitions(defold_sdk INTERFACE
    __STDC_LIMIT_MACROS
    DDF_EXPOSE_DESCRIPTORS
    GOOGLE_PROTOBUF_NO_RTTI
    DM_USE_CMAKE)

if(DEFOLD_MSVC_IDE_SOLUTION)
    target_compile_definitions(defold_sdk INTERFACE
        DM_LOG_TO_DEBUGGER)
    if(NOT DEFOLD_TEST_COLORS)
        target_compile_definitions(defold_sdk INTERFACE JC_TEST_USE_COLORS=0)
    endif()
elseif(DEFINED ENV{GITHUB_WORKFLOW})
    target_compile_definitions(defold_sdk INTERFACE GITHUB_CI)
    if(DEFOLD_TEST_COLORS)
        target_compile_definitions(defold_sdk INTERFACE JC_TEST_USE_COLORS=1)
    endif()
endif()

set(DEFOLD_PLATFORM_SUPPORTS_COMPUTE ON)
if(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web|x86_64-ios)$")
    set(DEFOLD_PLATFORM_SUPPORTS_COMPUTE OFF)
endif()

if(DEFOLD_PLATFORM_SUPPORTS_COMPUTE)
    target_compile_definitions(defold_sdk INTERFACE DM_HAVE_PLATFORM_COMPUTE_SUPPORT)
endif()

if (TARGET_PLATFORM MATCHES "^arm64|^x86_64")
    target_compile_definitions(defold_sdk INTERFACE DM_PLATFORM_64BIT)
else()
    target_compile_definitions(defold_sdk INTERFACE DM_PLATFORM_32BIT)
endif()

#**************************************************************************
# Common flags

if(MSVC_CL)
    target_compile_definitions(defold_sdk INTERFACE _HAS_EXCEPTIONS=0)

    # Match Waf: disable RTTI and C++ exception handling for engine code.
    # CMake's MSVC defaults add /EHsc, which conflicts with SEH __try blocks
    # that contain C++ objects requiring unwinding.
    target_compile_options(defold_sdk INTERFACE
        /GR-
        /W3
        $<$<COMPILE_LANGUAGE:CXX>:/EHs->
        $<$<COMPILE_LANGUAGE:CXX>:/EHa->)
else()
    # Apply per-language flags via target options
    set(_DEFOLD_NON_MSVC_OPTIONS
        -Wall
        -Werror=format
        -Werror=return-type
        -fvisibility=hidden
        -fno-exceptions
        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
    if(NOT TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
        list(APPEND _DEFOLD_NON_MSVC_OPTIONS -g)
    endif()
    if(NOT DEFINED DEFOLD_PLATFORM_SUPPORTS_FPIC)
        set(DEFOLD_PLATFORM_SUPPORTS_FPIC ON)
        if(TARGET_PLATFORM_OS STREQUAL "win32")
            set(DEFOLD_PLATFORM_SUPPORTS_FPIC OFF)
        endif()
    endif()
    if(DEFOLD_PLATFORM_SUPPORTS_FPIC)
        list(APPEND _DEFOLD_NON_MSVC_OPTIONS -fPIC)
    endif()
    if(DEFINED ENV{GITHUB_WORKFLOW})
        # Mirrors waf_dynamo.py: deterministic debug info paths on CI, so DWARF
        # source paths can be mapped to a local checkout with a single path
        # substitution (SDK headers appear under defoldsdk/, repo sources as
        # repo-relative paths)
        list(APPEND _DEFOLD_NON_MSVC_OPTIONS
            "-fdebug-compilation-dir=."
            "-fdebug-prefix-map=${DEFOLD_SDK_ROOT}=defoldsdk"
            "-fdebug-prefix-map=${DEFOLD_HOME}/=")
    endif()
    target_compile_options(defold_sdk INTERFACE ${_DEFOLD_NON_MSVC_OPTIONS})
endif()


#**************************************************************************
# Optimization flags (after platform detection)

set(_DEFOLD_OPT_CONFIG_EXPR "$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>")
set(_DEFOLD_RELWITHDEBINFO_OPT "-O2")
if(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
    set(_DEFOLD_RELWITHDEBINFO_OPT "-O3")
endif()

if(MSVC_CL)
    # Optimization flags scoped to targets using defold_sdk
    target_compile_options(defold_sdk INTERFACE
        "$<${_DEFOLD_OPT_CONFIG_EXPR}:/O2>"
        "$<$<NOT:${_DEFOLD_OPT_CONFIG_EXPR}>:/Od>")
else()
    target_compile_options(defold_sdk INTERFACE
        "$<$<CONFIG:Release>:-O2>"
        "$<$<CONFIG:RelWithDebInfo>:${_DEFOLD_RELWITHDEBINFO_OPT}>"
        "$<$<NOT:${_DEFOLD_OPT_CONFIG_EXPR}>:-O0>")
    if(NOT TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
        target_compile_options(defold_sdk INTERFACE -g)
        target_link_options(defold_sdk INTERFACE -g)
    endif()
    if(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
        target_link_options(defold_sdk INTERFACE "$<$<CONFIG:RelWithDebInfo>:-O3>")
    endif()
endif()

defold_log("CC: ${CMAKE_C_COMPILER}")
defold_log("CXX: ${CMAKE_CXX_COMPILER}")
