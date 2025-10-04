if(DEFINED DEFOLD_PLATFORM_HOST_CMAKE_INCLUDED)
  return()
endif()
set(DEFOLD_PLATFORM_HOST_CMAKE_INCLUDED ON CACHE INTERNAL "platform_host.cmake include guard")

defold_log("platform_host.cmake:")

# Robust host OS and architecture detection

# ------------------------------
# Detect host OS
# ------------------------------
set(_HOST_PLATFORM_OS "")
set(_defold_host_os_name "")

if(CMAKE_HOST_SYSTEM_NAME)
  set(_defold_host_os_name "${CMAKE_HOST_SYSTEM_NAME}")
elseif(CMAKE_SYSTEM_NAME)
  set(_defold_host_os_name "${CMAKE_SYSTEM_NAME}")
endif()

if(NOT _defold_host_os_name AND UNIX)
  execute_process(
    COMMAND uname -s
    OUTPUT_VARIABLE _defold_host_os_name
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
endif()

string(TOLOWER "${_defold_host_os_name}" _defold_host_os_name_lc)
if(_defold_host_os_name_lc MATCHES "darwin")
  set(_HOST_PLATFORM_OS "macos")
elseif(_defold_host_os_name_lc MATCHES "linux")
  set(_HOST_PLATFORM_OS "linux")
elseif(_defold_host_os_name_lc MATCHES "windows")
  set(_HOST_PLATFORM_OS "win32")
endif()

# ------------------------------
# Detect host CPU arch
# ------------------------------
set(_HOST_PLATFORM_ARCH "")
set(_defold_host_cpu "")

if(CMAKE_HOST_SYSTEM_PROCESSOR)
  set(_defold_host_cpu "${CMAKE_HOST_SYSTEM_PROCESSOR}")
elseif(CMAKE_SYSTEM_PROCESSOR)
  set(_defold_host_cpu "${CMAKE_SYSTEM_PROCESSOR}")
endif()

if(NOT _defold_host_cpu)
  if(UNIX)
    execute_process(
      COMMAND uname -m
      OUTPUT_VARIABLE _defold_host_cpu
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
  elseif(WIN32)
    if(DEFINED ENV{PROCESSOR_ARCHITEW6432})
      set(_defold_host_cpu "$ENV{PROCESSOR_ARCHITEW6432}")
    elseif(DEFINED ENV{PROCESSOR_ARCHITECTURE})
      set(_defold_host_cpu "$ENV{PROCESSOR_ARCHITECTURE}")
    endif()
  endif()
endif()

string(TOLOWER "${_defold_host_cpu}" _defold_host_cpu_lc)
if(_defold_host_cpu_lc MATCHES "^(aarch64|arm64)$")
  set(_HOST_PLATFORM_ARCH "arm64")
elseif(_defold_host_cpu_lc MATCHES "^(x86_64|amd64|x64|x86-64)$")
  set(_HOST_PLATFORM_ARCH "x86_64")
endif()

# Fallbacks if detection failed
if(NOT _HOST_PLATFORM_OS)
  if(WIN32)
    set(_HOST_PLATFORM_OS "win32")
  elseif(APPLE)
    set(_HOST_PLATFORM_OS "macos")
  elseif(UNIX)
    set(_HOST_PLATFORM_OS "linux")
  endif()
endif()

if(NOT _HOST_PLATFORM_ARCH)
  set(_HOST_PLATFORM_ARCH "x86_64")
endif()

set(HOST_PLATFORM "${_HOST_PLATFORM_ARCH}-${_HOST_PLATFORM_OS}")

message(STATUS "HOST_PLATFORM: ${HOST_PLATFORM}")
