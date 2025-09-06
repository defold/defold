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
  message(FATAL_ERROR "Could not infer TARGET_PLATFORM. Please pass -DTARGET_PLATFORM=<triple> (e.g. x86_64-macos)")
endif()

message(STATUS "TARGET_PLATFORM: ${TARGET_PLATFORM}")

# Common compile settings
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  # Apply per-language flags using separate generator expressions per option
  add_compile_options(
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
  )
elseif(MSVC)
  # Disable RTTI; don't force /EH to avoid changing exception model globally
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/GR->)
endif()

# Platform specific options

if (TARGET_PLATFORM MATCHES "arm64-macos|x86_64-macos")
    include(platform_macos)
endif()

# message(STATUS "CFLAGS: ${CMAKE_C_FLAGS}")
# message(STATUS "CXXFLAGS: ${CMAKE_CXX_FLAGS}")

