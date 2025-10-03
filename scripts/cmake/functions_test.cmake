defold_log("functions_test.cmake:")

# Registers a test target with the global build_tests and run_tests targets.
#
# Usage:
#   defold_register_test_target(<target> [run_flag] [run_workdir])
#   - run_flag: ON/OFF (default ON). If ON, creates a per-test run target and
#               adds it to global run_tests
#   - run_workdir: optional working directory for executing the test

function(defold_register_test_target target_name)
  if(NOT TARGET ${target_name})
    message(FATAL_ERROR "defold_register_test_target: target '${target_name}' does not exist")
  endif()

  # Keep tests out of the default 'all' build. They are built via build_tests
  # or when directly requested. This mirrors typical Waf behavior.
  set_target_properties(${target_name} PROPERTIES EXCLUDE_FROM_ALL TRUE)

  # Ensure global aggregator exists
  if(NOT TARGET build_tests)
    add_custom_target(build_tests)
  endif()
  add_dependencies(build_tests ${target_name})

  # Optional run flag (default ON)
  set(_RUN_TEST ON)
  if(ARGC GREATER 1)
    set(_RUN_TEST "${ARGV1}")
  endif()
  string(TOUPPER "${_RUN_TEST}" _RUN_TEST_UPPER)
  if(_RUN_TEST_UPPER STREQUAL "0" OR _RUN_TEST_UPPER STREQUAL "OFF" OR _RUN_TEST_UPPER STREQUAL "FALSE" OR _RUN_TEST_UPPER STREQUAL "NO")
    set(_RUN_TEST FALSE)
  else()
    set(_RUN_TEST TRUE)
  endif()

  # Optional working directory
  set(_RUN_DIR "")
  if(ARGC GREATER 2)
    set(_RUN_DIR "${ARGV2}")
  endif()
  set(_RUN_DIR_NORM "")
  if(_RUN_DIR)
    get_filename_component(_RUN_DIR_NORM "${_RUN_DIR}" ABSOLUTE)
    get_filename_component(_RUN_DIR_NORM "${_RUN_DIR_NORM}" REALPATH)
  endif()

  if(_RUN_TEST)
    set(_run_target "run_${target_name}")
    if(NOT TARGET ${_run_target})
      if(_RUN_DIR_NORM)
        add_custom_target(${_run_target}
          COMMAND ${CMAKE_COMMAND} -E chdir "${_RUN_DIR_NORM}" $<TARGET_FILE:${target_name}>
          DEPENDS ${target_name}
          USES_TERMINAL
          COMMENT "Running ${target_name} in ${_RUN_DIR_NORM}")
      else()
        add_custom_target(${_run_target}
          COMMAND $<TARGET_FILE:${target_name}>
          DEPENDS ${target_name}
          USES_TERMINAL
          COMMENT "Running ${target_name}")
      endif()
    endif()

    if(NOT TARGET run_tests)
      add_custom_target(run_tests)
    endif()
    add_dependencies(run_tests ${_run_target})
  endif()
endfunction()
