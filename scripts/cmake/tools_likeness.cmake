# Host-side test dependencies; this check never creates a graphics context.
_defold_find_python(_likeness_python)
message(STATUS "Image-test Python: ${_likeness_python}")
execute_process(
  COMMAND "${_likeness_python}" "${DEFOLD_HOME}/scripts/likeness.py" --check
  RESULT_VARIABLE _likeness_result
  OUTPUT_VARIABLE _likeness_output
  ERROR_VARIABLE _likeness_error
  OUTPUT_STRIP_TRAILING_WHITESPACE)
message(STATUS "${_likeness_output}")
if(NOT _likeness_result EQUAL 0)
  message(FATAL_ERROR "Image comparison prerequisites failed: ${_likeness_error}")
endif()
