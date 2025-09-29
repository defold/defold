defold_log("tools_protoc.cmake:")

# Ensure that protoc is the one shipped with the Defold SDK (tmp/dynamo_home)
# Layout used by Defold packages:
#   ${DEFOLD_SDK_ROOT}/ext/bin/<host-platform>/protoc
#   ${DEFOLD_SDK_ROOT}/bin/ddfc_cxx (protoc plugin)

if(NOT DEFINED DEFOLD_SDK_ROOT OR NOT EXISTS "${DEFOLD_SDK_ROOT}")
  message(FATAL_ERROR "tools_protoc: DEFOLD_SDK_ROOT is not set or does not exist. Cannot validate protoc.")
endif()

find_program(_DEFOLD_PROTOC_EXECUTABLE NAMES protoc)
if(NOT _DEFOLD_PROTOC_EXECUTABLE)
  message(FATAL_ERROR "tools_protoc: 'protoc' not found on PATH. Please prepend ${DEFOLD_SDK_ROOT}/ext/bin/<host-platform> to PATH.")
endif()

# Expect protoc to reside under the SDK root
file(REAL_PATH "${_DEFOLD_PROTOC_EXECUTABLE}" _DEFOLD_PROTOC_REAL)
file(REAL_PATH "${DEFOLD_SDK_ROOT}" _DEFOLD_SDK_ROOT_REAL)

string(FIND "${_DEFOLD_PROTOC_REAL}" "${_DEFOLD_SDK_ROOT_REAL}" _sdk_pos)
if(_sdk_pos EQUAL -1)
  message(FATAL_ERROR "tools_protoc: Using system protoc at '${_DEFOLD_PROTOC_REAL}'. Please use the SDK's protoc under '${DEFOLD_SDK_ROOT}/ext/bin/<host-platform>/protoc'.")
endif()

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
