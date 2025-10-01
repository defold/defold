# CMake script used by defold_protoc_encode to run protoc with INPUT/OUTPUT files

if(NOT PROTOC_EXECUTABLE OR NOT PROTO_MESSAGE_NAME OR NOT PROTO_SCHEMA OR NOT PROTO_INFILE OR NOT PROTO_OUTFILE)
  message(FATAL_ERROR "run_protoc_encode.cmake: Missing required variables")
endif()

# PROTO_INCLUDES is a CMake list of --proto_path=... entries
set(_ARGS)
foreach(_a IN LISTS PROTO_INCLUDES)
  list(APPEND _ARGS "${_a}")
endforeach()

execute_process(
  COMMAND "${PROTOC_EXECUTABLE}" --encode=${PROTO_MESSAGE_NAME} ${_ARGS} "${PROTO_SCHEMA}"
  INPUT_FILE  "${PROTO_INFILE}"
  OUTPUT_FILE "${PROTO_OUTFILE}"
  RESULT_VARIABLE _res
  ERROR_VARIABLE  _err
)

if(NOT _res EQUAL 0)
  message(FATAL_ERROR "protoc encode failed (${_res}): ${_err}")
endif()

