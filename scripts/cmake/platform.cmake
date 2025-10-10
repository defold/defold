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

# Include the sdk
include(sdk)

# Global empty target to aggregate test build dependencies across subprojects
if(NOT TARGET build_tests)
  add_custom_target(build_tests)
endif()


#**************************************************************************
# Common compile settings
# Provide the C++ standard via target-level usage requirements
target_compile_features(defold_sdk INTERFACE cxx_std_11)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

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

if(MSVC_CL)
    # Disable RTTI; don't force /EH to avoid changing exception model globally
    target_compile_options(defold_sdk INTERFACE /GR- /W3)
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
        -g)
endif()


#**************************************************************************
# Optimization flags (after platform detection)

set(_DEFOLD_OPT_CONFIG_EXPR "$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>")

if(MSVC_CL)
    # Optimization flags scoped to targets using defold_sdk
    target_compile_options(defold_sdk INTERFACE
        "$<${_DEFOLD_OPT_CONFIG_EXPR}:/O2>"
        "$<$<NOT:${_DEFOLD_OPT_CONFIG_EXPR}>:/Od>")
else()
    target_compile_options(defold_sdk INTERFACE
        "$<${_DEFOLD_OPT_CONFIG_EXPR}:-O3>"
        "$<$<NOT:${_DEFOLD_OPT_CONFIG_EXPR}>:-O0>")
endif()

defold_log("CC: ${CMAKE_C_COMPILER}")
defold_log("CXX: ${CMAKE_CXX_COMPILER}")
