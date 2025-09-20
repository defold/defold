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
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
elseif (TARGET_PLATFORM MATCHES "arm64-win32|x86_64-win32")
        include(platform_windows)
endif()

#**************************************************************************
# Common defines

set(CMAKE_CXX_FLAGS_RELEASE "") # remove -DNDEBUG and -O3

add_compile_definitions(
    __STDC_LIMIT_MACROS
    DDF_EXPOSE_DESCRIPTORS
    GOOGLE_PROTOBUF_NO_RTTI
    DM_USE_CMAKE # for some tests, as the build folders change
)

if (TARGET_PLATFORM MATCHES "^arm64|^x86_64")
        add_compile_definitions(DM_PLATFORM_64BIT)
else()
        add_compile_definitions(DM_PLATFORM_32BIT)
endif()

#**************************************************************************
# Common flags

set(_MSVC_SYNTAX OFF)
set(_CLANG_SYNTAX OFF)
if(MSVC OR (DEFINED _DEFOLD_MSVC_LIKE AND _DEFOLD_MSVC_LIKE) OR (DEFINED _DEFOLD_MSVC_NATIVE AND _DEFOLD_MSVC_NATIVE))
    set(_MSVC_SYNTAX ON)
endif()

# Treat clang-cl as MSVC-like on Windows
if(_MSVC_SYNTAX)
    # Disable RTTI; don't force /EH to avoid changing exception model globally
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")

    if (DEFINED _DEFOLD_MSVC_LIKE AND _DEFOLD_MSVC_LIKE)
        # for ninja to output colors
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color")
    endif()

else()

    # Apply per-language flags using separate generator expressions per option
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=format")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
    # for ninja to output colors
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color")

endif()


#**************************************************************************
# Optimization flags (after platform detection)

if(_MSVC_SYNTAX)
    if (CMAKE_BUILD_TYPE MATCHES "Release")
        add_compile_options(/O2)
    else()
        add_compile_options(/Od)
    endif()
else()
    if (CMAKE_BUILD_TYPE MATCHES "Release")
        add_compile_options(-O3)
    else()
        add_compile_options(-O0)
    endif()
endif()

# message(STATUS "CFLAGS: ${CMAKE_C_FLAGS}")
# message(STATUS "CXXFLAGS: ${CMAKE_CXX_FLAGS}")

message(STATUS "CC: ${CMAKE_C_COMPILER}")
message(STATUS "CXX: ${CMAKE_CXX_COMPILER}")
