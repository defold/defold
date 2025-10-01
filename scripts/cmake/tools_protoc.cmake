defold_log("tools_protoc.cmake:")

# Ensure that protoc is the one shipped with the Defold SDK (tmp/dynamo_home)
# Layout used by Defold packages:
#   ${DEFOLD_SDK_ROOT}/ext/bin/<host-platform>/protoc
#   ${DEFOLD_SDK_ROOT}/bin/ddfc_cxx (protoc plugin)

if(NOT DEFINED DEFOLD_SDK_ROOT OR NOT EXISTS "${DEFOLD_SDK_ROOT}")
  message(FATAL_ERROR "tools_protoc: DEFOLD_SDK_ROOT is not set or does not exist. Cannot locate protoc.")
endif()

# Prefer packaged protoc from DEFOLD_SDK_ROOT/ext/bin/*/protoc
set(_DEFOLD_PROTOC_EXECUTABLE "")
if(WIN32)
  file(GLOB_RECURSE _protoc_pkgs "${DEFOLD_SDK_ROOT}/ext/bin/*/protoc.exe")
else()
  file(GLOB_RECURSE _protoc_pkgs "${DEFOLD_SDK_ROOT}/ext/bin/*/protoc")
endif()

if(_protoc_pkgs)
  list(SORT _protoc_pkgs COMPARE NATURAL)
  list(GET _protoc_pkgs -1 _DEFOLD_PROTOC_EXECUTABLE)
else()
  # Fallback to PATH
  find_program(_DEFOLD_PROTOC_EXECUTABLE NAMES protoc)
endif()

if(NOT _DEFOLD_PROTOC_EXECUTABLE)
  message(FATAL_ERROR "tools_protoc: Could not find protoc. Expected in ${DEFOLD_SDK_ROOT}/ext/bin/<host-platform>/ or on PATH.")
endif()

file(REAL_PATH "${_DEFOLD_PROTOC_EXECUTABLE}" _DEFOLD_PROTOC_REAL)
set(DEFOLD_PROTOC_EXECUTABLE "${_DEFOLD_PROTOC_REAL}" CACHE FILEPATH "Path to protoc executable used by Defold")

# Optional: Print protoc version
execute_process(
  COMMAND "${_DEFOLD_PROTOC_EXECUTABLE}" --version
  OUTPUT_VARIABLE _protoc_ver
  ERROR_VARIABLE _protoc_err
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_STRIP_TRAILING_WHITESPACE)
if(_protoc_ver)
  defold_log("tools_protoc: protoc ${_protoc_ver} at ${_DEFOLD_PROTOC_REAL}")
else()
  defold_log("tools_protoc: protoc at ${_DEFOLD_PROTOC_REAL}")
endif()
