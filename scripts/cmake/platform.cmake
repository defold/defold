message("platform.cmake:")

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

message(STATUS "TARGET_PLATFORM: ${TARGET_PLATFORM}")

# Global empty target to aggregate test build dependencies across subprojects
if(NOT TARGET build_tests)
  add_custom_target(build_tests)
endif()

# Test utilities (register tests, run targets)
include(functions_test)

#**************************************************************************
# Common compile settings
# Provide the C++ standard via target-level usage requirements
target_compile_features(defold_sdk INTERFACE cxx_std_11)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Platform specific options
include(sdk)

if (TARGET_PLATFORM MATCHES "arm64-macos|x86_64-macos")
        include(platform_macos)
elseif (TARGET_PLATFORM MATCHES "arm64-ios|x86_64-ios")
        include(platform_ios)
elseif (TARGET_PLATFORM MATCHES "armv7-android|arm64-android")
        include(platform_android)
elseif (TARGET_PLATFORM MATCHES "js-web|wasm-web|wasm_pthread-web")
        include(platform_html5)
elseif (TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
        include(platform_linux)
elseif (TARGET_PLATFORM MATCHES "arm64-win32|x86_64-win32|x86-win32")
        include(platform_windows)
endif()

#**************************************************************************
# Common defines

set(CMAKE_CXX_FLAGS_RELEASE "") # remove -DNDEBUG and -O3

# Attach common definitions to defold_sdk target
target_compile_definitions(defold_sdk INTERFACE
    __STDC_LIMIT_MACROS
    DDF_EXPOSE_DESCRIPTORS
    GOOGLE_PROTOBUF_NO_RTTI
    DM_USE_CMAKE)

if (TARGET_PLATFORM MATCHES "^arm64|^x86_64")
    target_compile_definitions(defold_sdk INTERFACE DM_PLATFORM_64BIT)
else()
    target_compile_definitions(defold_sdk INTERFACE DM_PLATFORM_32BIT)
endif()

#**************************************************************************
# Common flags

set(_MSVC_SYNTAX OFF)
if(MSVC OR (DEFINED _DEFOLD_MSVC_LIKE AND _DEFOLD_MSVC_LIKE) OR (DEFINED _DEFOLD_MSVC_NATIVE AND _DEFOLD_MSVC_NATIVE))
    set(_MSVC_SYNTAX ON)
endif()

# Treat clang-cl as MSVC-like on Windows
if(_MSVC_SYNTAX)
    # Disable RTTI; don't force /EH to avoid changing exception model globally
    target_compile_options(defold_sdk INTERFACE /GR- /W3)
    if (DEFINED _DEFOLD_MSVC_NATIVE AND NOT _DEFOLD_MSVC_NATIVE)
        # for ninja to output colors (clang-cl)
        target_compile_options(defold_sdk INTERFACE -fdiagnostics-color)
    endif()
else()
    # Apply per-language flags via target options
    target_compile_options(defold_sdk INTERFACE
        -Wall
        -Werror=format
        -fPIC
        -fvisibility=hidden
        -fno-exceptions
        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
        -flto
        -g
        -fdiagnostics-color)
endif()


#**************************************************************************
# Optimization flags (after platform detection)

if(_MSVC_SYNTAX)
    # Optimization flags scoped to targets using defold_sdk
    target_compile_options(defold_sdk INTERFACE
        $<$<CONFIG:Release>:/O2>
        $<$<NOT:$<CONFIG:Release>>:/Od>)
else()
    target_compile_options(defold_sdk INTERFACE
        $<$<CONFIG:Release>:-O3>
        $<$<NOT:$<CONFIG:Release>>:-O0>)
endif()

#**************************************************************************
# WIP: Workaround since we've been using a non-standard way of naming windows libraries with "lib" prefix
# Goal is to not use that prefix at all.
set(defold_libprefix "")
if (TARGET_PLATFORM MATCHES "x86_64-win32|x86-win32")
    set(defold_libprefix "lib")
endif()

# message(STATUS "CFLAGS: ${CMAKE_C_FLAGS}")
# message(STATUS "CXXFLAGS: ${CMAKE_CXX_FLAGS}")

message(STATUS "CC: ${CMAKE_C_COMPILER}")
message(STATUS "CXX: ${CMAKE_CXX_COMPILER}")
