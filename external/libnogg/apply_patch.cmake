# FetchContent can repeat this step on the same extracted sources.
execute_process(COMMAND "${GIT_EXECUTABLE}" apply --no-index --reverse --check "${PATCH_FILE}"
  RESULT_VARIABLE reverse_check ERROR_QUIET)
if(NOT reverse_check EQUAL 0)
  execute_process(COMMAND "${GIT_EXECUTABLE}" apply --no-index "${PATCH_FILE}"
    COMMAND_ERROR_IS_FATAL ANY)
endif()
