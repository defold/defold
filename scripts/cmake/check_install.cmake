# Verify a Defold SDK/toolchain environment by loading core CMake modules.
# Usage:
#   cmake -P scripts/cmake/check_install.cmake
#   cmake -DTARGET_PLATFORM=x86_64-win32 -P scripts/cmake/check_install.cmake

# Ensure this directory is on the module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")

# helper functions (e.g. defold_log)
include(functions)

# Verbose logging is controlled by -DDEFOLD_VERBOSE=ON (if provided)

# Resolve DEFOLD_HOME (repo root: two levels up from scripts/cmake)
get_filename_component(DEFOLD_HOME "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# Try to set DEFOLD_SDK_ROOT if not provided
if(NOT DEFINED DEFOLD_SDK_ROOT)
  if(DEFINED ENV{DYNAMO_HOME} AND EXISTS "$ENV{DYNAMO_HOME}")
    set(DEFOLD_SDK_ROOT "$ENV{DYNAMO_HOME}")
  elseif(EXISTS "${DEFOLD_HOME}/tmp/dynamo_home")
    set(DEFOLD_SDK_ROOT "${DEFOLD_HOME}/tmp/dynamo_home")
  endif()
endif()

message(STATUS "check_install: DEFOLD_HOME=${DEFOLD_HOME}")
message(STATUS "check_install: DEFOLD_SDK_ROOT=${DEFOLD_SDK_ROOT}")

# Detect host platform and default TARGET_PLATFORM (respects -DTARGET_PLATFORM=...)
include(platform_host)
if(NOT DEFINED TARGET_PLATFORM OR NOT TARGET_PLATFORM)
  set(TARGET_PLATFORM "${HOST_PLATFORM}")
endif()
message(STATUS "check_install: TARGET_PLATFORM=${TARGET_PLATFORM}")

# Load tool checks (Java, Ninja, Protoc) and validate versions/paths
include(tools)

# Load SDK detection for the chosen TARGET_PLATFORM
include(sdk)

message(STATUS "check_install: Environment looks OK for ${TARGET_PLATFORM}")
