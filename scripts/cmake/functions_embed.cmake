if(DEFINED DEFOLD_EMBED_GENERATE)
  if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT_HEADER OR NOT DEFINED OUTPUT_CPP OR NOT DEFINED SYMBOL)
    message(FATAL_ERROR "functions_embed: Missing vars (INPUT/OUTPUT_HEADER/OUTPUT_CPP/SYMBOL)")
  endif()

  get_filename_component(_header_dir "${OUTPUT_HEADER}" DIRECTORY)
  get_filename_component(_cpp_dir "${OUTPUT_CPP}" DIRECTORY)
  if(_header_dir)
    file(MAKE_DIRECTORY "${_header_dir}")
  endif()
  if(_cpp_dir)
    file(MAKE_DIRECTORY "${_cpp_dir}")
  endif()

  file(SIZE "${INPUT}" _sz)
  file(READ "${INPUT}" _data HEX)
  string(REGEX REPLACE "(..)" "0x\\1, " _bytes "${_data}")
  string(REGEX REPLACE "((0x[0-9a-f][0-9a-f], ){16})" "\\1\n    " _bytes "${_bytes}")

  file(WRITE "${OUTPUT_HEADER}" "#include <stdint.h>\n\n")
  file(APPEND "${OUTPUT_HEADER}" "extern unsigned char ${SYMBOL}[];\n")
  file(APPEND "${OUTPUT_HEADER}" "extern uint32_t ${SYMBOL}_SIZE;\n")

  file(WRITE "${OUTPUT_CPP}" "#include <stdint.h>\n")
  file(APPEND "${OUTPUT_CPP}" "#include \"dlib/align.h\"\n\n")
  file(APPEND "${OUTPUT_CPP}" "unsigned char DM_ALIGNED(16) ${SYMBOL}[] =\n")
  file(APPEND "${OUTPUT_CPP}" "{\n    ${_bytes}\n};\n")
  file(APPEND "${OUTPUT_CPP}" "uint32_t ${SYMBOL}_SIZE = ${_sz};\n")
  return()
endif()

if(COMMAND defold_log)
  defold_log("functions_embed.cmake:")
endif()

# Generate embedded C sources for a binary file.
# Usage:
#   defold_embed_to_header(INPUT <file> OUTPUT_HEADER <path> OUTPUT_CPP <path>
#                          [SYMBOL <name>] [OUT_HEADER_VAR var] [OUT_CPP_VAR var])
function(defold_embed_to_header)
  set(options)
  set(oneValueArgs INPUT OUTPUT_HEADER OUTPUT_CPP SYMBOL OUT_HEADER_VAR OUT_CPP_VAR)
  set(multiValueArgs)
  cmake_parse_arguments(EM "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT EM_INPUT)
    message(FATAL_ERROR "defold_embed_to_header: missing INPUT")
  endif()
  if(NOT EM_OUTPUT_HEADER)
    message(FATAL_ERROR "defold_embed_to_header: missing OUTPUT_HEADER")
  endif()
  if(NOT EM_OUTPUT_CPP)
    message(FATAL_ERROR "defold_embed_to_header: missing OUTPUT_CPP")
  endif()

  if(EM_SYMBOL)
    set(_sym "${EM_SYMBOL}")
  else()
    get_filename_component(_base "${EM_INPUT}" NAME)
    string(TOUPPER "${_base}" _sym)
    string(REPLACE "." "_" _sym "${_sym}")
    string(REPLACE "-" "_" _sym "${_sym}")
    string(REPLACE "@" "at" _sym "${_sym}")
  endif()

  add_custom_command(
    OUTPUT "${EM_OUTPUT_HEADER}" "${EM_OUTPUT_CPP}"
    COMMAND "${CMAKE_COMMAND}"
      -DDEFOLD_EMBED_GENERATE=ON
      -DINPUT:FILEPATH=${EM_INPUT}
      -DOUTPUT_HEADER:FILEPATH=${EM_OUTPUT_HEADER}
      -DOUTPUT_CPP:FILEPATH=${EM_OUTPUT_CPP}
      -DSYMBOL:STRING=${_sym}
      -P "${CMAKE_CURRENT_FUNCTION_LIST_FILE}"
    DEPENDS "${EM_INPUT}" "${CMAKE_CURRENT_FUNCTION_LIST_FILE}"
    VERBATIM)

  if(EM_OUT_HEADER_VAR)
    set(${EM_OUT_HEADER_VAR} "${EM_OUTPUT_HEADER}" PARENT_SCOPE)
  endif()
  if(EM_OUT_CPP_VAR)
    set(${EM_OUT_CPP_VAR} "${EM_OUTPUT_CPP}" PARENT_SCOPE)
  endif()
endfunction()
