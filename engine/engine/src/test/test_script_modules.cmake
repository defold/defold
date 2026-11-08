if(SUPPORTS_COMPUTE)
  set(_supports_compute 1)
else()
  set(_supports_compute 0)
endif()

if(WITH_ASAN AND CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
  # Match the graphics tests' Xlib suppression while keeping engine leak checks.
  set(ENV{LSAN_OPTIONS} "suppressions=${CMAKE_CURRENT_LIST_DIR}/test_script_modules.lsan.supp:$ENV{LSAN_OPTIONS}")
endif()

execute_process(
  COMMAND "${ENGINE}"
    "--config=resource.uri=${CONTENT_ROOT}"
    --config=bootstrap.main_collection=/script_modules/script_modules.collectionc
    --config=bootstrap.debug_init_script=
    --config=display.width=64
    --config=display.height=64
    "--config=test.supports_compute=${_supports_compute}"
    "${CONTENT_ROOT}/game.projectc"
  RESULT_VARIABLE _result
  OUTPUT_VARIABLE _stdout
  ERROR_VARIABLE _stderr
  TIMEOUT 30)
if(NOT "${_result}" STREQUAL "0")
  message(FATAL_ERROR "Script module registration failed in ${ENGINE}: ${_result}\n${_stdout}\n${_stderr}")
endif()
message(STATUS "Material and compute module registration passed in ${ENGINE}")
