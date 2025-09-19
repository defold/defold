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

if(MSVC)
  # Disable RTTI; don't force /EH to avoid changing exception model globally
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")

  if (CMAKE_BUILD_TYPE MATCHES "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Od")
  endif()

else()

  # Apply per-language flags using separate generator expressions per option
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=format")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
  # for ninja to output colors
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color")

  if (CMAKE_BUILD_TYPE MATCHES "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
  endif()

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
endif()

# message(STATUS "CFLAGS: ${CMAKE_C_FLAGS}")
# message(STATUS "CXXFLAGS: ${CMAKE_CXX_FLAGS}")

message(STATUS "CC: ${CMAKE_C_COMPILER}")
message(STATUS "CXX: ${CMAKE_CXX_COMPILER}")
