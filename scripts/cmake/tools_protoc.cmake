defold_log("tools_protoc.cmake:")

# Ensure that protoc is the one shipped with the Defold SDK (tmp/dynamo_home)
# Layout used by Defold packages:
#   ${DEFOLD_SDK_ROOT}/ext/bin/<host-platform>/protoc
#   ${DEFOLD_SDK_ROOT}/bin/ddfc_cxx (protoc plugin)

if(NOT DEFINED DEFOLD_SDK_ROOT OR NOT EXISTS "${DEFOLD_SDK_ROOT}")
  message(FATAL_ERROR "Could not find protoc. Run ./scripts/build.py install_ext to build and install it.\n\nDEFOLD_SDK_ROOT is not set or does not exist: ${DEFOLD_SDK_ROOT}")
endif()

# Search afresh so a cached PATH compiler cannot override an installed SDK compiler.
set(_DEFOLD_PROTOC_EXECUTABLE "_DEFOLD_PROTOC_EXECUTABLE-NOTFOUND")

# Ensure HOST_PLATFORM is known
if(NOT DEFINED HOST_PLATFORM)
  include(platform_host)
endif()

set(_host_bin "${DEFOLD_SDK_ROOT}/ext/bin/${HOST_PLATFORM}")
# Protoc runs on the host, including when cross-compiling. Search its SDK folder
# before allowing CMake's normal program search (including PATH).
find_program(_DEFOLD_PROTOC_EXECUTABLE NAMES protoc
  PATHS "${_host_bin}" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH NO_CACHE)
if(NOT _DEFOLD_PROTOC_EXECUTABLE)
  find_program(_DEFOLD_PROTOC_EXECUTABLE NAMES protoc NO_CMAKE_FIND_ROOT_PATH NO_CACHE)
endif()

if(NOT _DEFOLD_PROTOC_EXECUTABLE)
  message(FATAL_ERROR "Could not find protoc. Run ./scripts/build.py install_ext to build and install it.\n\nExpected in ${_host_bin}/ or on PATH.")
endif()

file(REAL_PATH "${_DEFOLD_PROTOC_EXECUTABLE}" _DEFOLD_PROTOC_REAL)

execute_process(
  COMMAND "${_DEFOLD_PROTOC_EXECUTABLE}" --version
  RESULT_VARIABLE _protoc_result
  OUTPUT_VARIABLE _protoc_ver
  ERROR_VARIABLE _protoc_err
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_STRIP_TRAILING_WHITESPACE)
if(NOT _protoc_result EQUAL 0)
  message(FATAL_ERROR "Could not run protoc. Run ./scripts/build.py install_ext to build and install it.\n\n${_DEFOLD_PROTOC_REAL}: ${_protoc_err}")
endif()
# Reconfiguration must replace a previously cached PATH compiler with the installed SDK compiler.
set(DEFOLD_PROTOC_EXECUTABLE "${_DEFOLD_PROTOC_REAL}" CACHE FILEPATH "Path to protoc executable used by Defold" FORCE)
if(_protoc_ver)
  defold_log("tools_protoc: protoc ${_protoc_ver} at ${_DEFOLD_PROTOC_REAL}")
else()
  defold_log("tools_protoc: protoc at ${_DEFOLD_PROTOC_REAL}")
endif()
