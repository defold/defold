# Verify a Defold SDK/toolchain environment by loading core CMake modules.
# Usage:
#   cmake -P scripts/cmake/check_install.cmake
#   cmake -DTARGET_PLATFORM=x86_64-win32 -P scripts/cmake/check_install.cmake

# Ensure this directory is on the module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")

include("${CMAKE_CURRENT_LIST_DIR}/private_repos.cmake")
# helper functions (e.g. defold_log)
include(functions)

# Verbose logging is controlled by -DDEFOLD_VERBOSE=ON (if provided)

# build.py consumes these records to format checks for its output stream.
function(defold_sdk_check NAME)
  if(DEFOLD_SDK_CHECK)
    if(ARGC EQUAL 1)
      message("DEFOLD_SDK_CHECK\t${NAME}")
    else()
      message("DEFOLD_SDK_CHECK\t${NAME}\t${ARGV1}\t${ARGV2}")
    endif()
  endif()
endfunction()

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
if(DEFOLD_SDK_CHECK)
  include(tools_clang)
  defold_sdk_check("Java")
  include(tools_java)
  defold_sdk_check("Java" "Java ${Java_VERSION}" "${Java_JAVA_EXECUTABLE}")
  defold_sdk_check("Ninja")
  include(tools_ninja)
  if(_ninja_ver)
    defold_sdk_check("Ninja" "Ninja ${_ninja_ver}" "${_DEFOLD_NINJA_EXECUTABLE}")
  else()
    defold_sdk_check("Ninja" "Found" "${_DEFOLD_NINJA_EXECUTABLE}")
  endif()
  if(NOT DEFINED DEFOLD_REQUIRE_PROTOC OR DEFOLD_REQUIRE_PROTOC)
    defold_sdk_check("Protoc")
    include(tools_protoc)
    if(_protoc_ver)
      defold_sdk_check("Protoc" "${_protoc_ver}" "${_DEFOLD_PROTOC_REAL}")
    else()
      defold_sdk_check("Protoc" "Found" "${_DEFOLD_PROTOC_REAL}")
    endif()
  endif()
else()
  include(tools)
endif()

# Load SDK detection for the chosen TARGET_PLATFORM
defold_sdk_check("Toolchain")
include(sdk)
defold_sdk_check("Toolchain" "${TARGET_PLATFORM}" "${CMAKE_TOOLCHAIN_FILE}")

message(STATUS "check_install: Environment looks OK for ${TARGET_PLATFORM}")
