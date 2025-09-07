message("platform.cmake:")

# Best-effort platform triple inference (can be overridden with -DTARGET_PLATFORM=...)
set(TARGET_PLATFORM "" CACHE STRING "Defold platform triple (e.g. x86_64-macos, arm64-macos, x86_64-linux, x86_64-win32)")
if(NOT TARGET_PLATFORM)
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
      set(TARGET_PLATFORM "arm64-macos")
    else()
      set(TARGET_PLATFORM "x86_64-macos")
    endif()
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(TARGET_PLATFORM "x86_64-linux")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(TARGET_PLATFORM "x86_64-win32")
  endif()
endif()

if(NOT TARGET_PLATFORM)
  message(FATAL_ERROR "Could not infer TARGET_PLATFORM. Please pass -DTARGET_PLATFORM=<2-tuple> (e.g. x86_64-macos)")
endif()

message(STATUS "TARGET_PLATFORM: ${TARGET_PLATFORM}")

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
add_compile_definitions(
  __STDC_LIMIT_MACROS
  DDF_EXPOSE_DESCRIPTORS
  GOOGLE_PROTOBUF_NO_RTTI
)

#**************************************************************************
# Common flags

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  # Apply per-language flags using separate generator expressions per option
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=format")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

  if (CMAKE_BUILD_TYPE MATCHES "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
  endif()

elseif(MSVC)
  # Disable RTTI; don't force /EH to avoid changing exception model globally
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")

  if (CMAKE_BUILD_TYPE MATCHES "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Od")
  endif()

endif()

# Platform specific options
include(sdk)

if (TARGET_PLATFORM MATCHES "arm64-macos|x86_64-macos")
    include(platform_macos)
endif()

# message(STATUS "CFLAGS: ${CMAKE_C_FLAGS}")
# message(STATUS "CXXFLAGS: ${CMAKE_CXX_FLAGS}")

message(STATUS "CC: ${CMAKE_C_COMPILER}")
message(STATUS "CXX: ${CMAKE_CXX_COMPILER}")

