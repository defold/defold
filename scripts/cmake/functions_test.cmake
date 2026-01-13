defold_log("functions_test.cmake:")

# Registers a test target with the global build_tests and run_tests targets.
#
# Usage:
#   defold_register_test_target(<target> [run_flag] [run_workdir])
#   - run_flag: ON/OFF (default ON). If ON, creates a per-test run target and
#               adds it to global run_tests
#   - run_workdir: optional working directory for executing the test

function(_defold_find_nodejs out_var)
  set(_nodejs "")
  if(DEFINED DEFOLD_SDK_ROOT)
    file(GLOB _nodes "${DEFOLD_SDK_ROOT}/ext/SDKs/emsdk*/node/*/bin/node")
    list(SORT _nodes)
    list(LENGTH _nodes _len)
    if(_len GREATER 0)
      math(EXPR _idx "${_len} - 1")
      list(GET _nodes ${_idx} _nodejs)
    endif()
  endif()

  if(DEFINED ENV{EMSDK})
    file(GLOB _nodes "$ENV{EMSDK}/node/*/bin/node")
    list(SORT _nodes)
    list(LENGTH _nodes _len)
    if(_len GREATER 0)
      math(EXPR _idx "${_len} - 1")
      list(GET _nodes ${_idx} _nodejs)
    endif()
  endif()

  if(NOT _nodejs AND DEFINED EMSCRIPTEN)
    get_filename_component(_emsdk_root "${EMSCRIPTEN}" DIRECTORY)
    get_filename_component(_emsdk_root "${_emsdk_root}" DIRECTORY)
    file(GLOB _nodes "${_emsdk_root}/node/*/bin/node")
    list(SORT _nodes)
    list(LENGTH _nodes _len)
    if(_len GREATER 0)
      math(EXPR _idx "${_len} - 1")
      list(GET _nodes ${_idx} _nodejs)
    endif()
  endif()

  if(NOT _nodejs)
    find_program(_nodejs node)
  endif()

  set(${out_var} "${_nodejs}" PARENT_SCOPE)
endfunction()

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
      if(TARGET_PLATFORM MATCHES "js-web|wasm-web|wasm_pthread-web")
        set(_pre_js "${DEFOLD_SDK_ROOT}/share/js-web-pre.js")
        if(EXISTS "${_pre_js}")
          target_link_options(${target_name} PRIVATE "--pre-js" "${_pre_js}" "-lnodefs.js")
        else()
          message(FATAL_ERROR "defold_register_test_target: missing pre-js file '${_pre_js}' for web test '${target_name}'")
        endif()
      endif()
      set(_run_exe "$<TARGET_FILE:${target_name}>")
      set(_run_args "")
      if(TARGET_PLATFORM MATCHES "js-web|wasm-web|wasm_pthread-web")
        _defold_find_nodejs(_nodejs)
        if(NOT _nodejs)
          message(FATAL_ERROR "defold_register_test_target: node not found for web test '${target_name}'")
        endif()
        set(_run_exe "${_nodejs}")
        set(_run_args "$<TARGET_FILE:${target_name}>")
      endif()
      if(_RUN_DIR_NORM)
        add_custom_target(${_run_target}
          COMMAND ${CMAKE_COMMAND} -E chdir "${_RUN_DIR_NORM}" ${_run_exe} ${_run_args}
          DEPENDS ${target_name}
          USES_TERMINAL
          COMMENT "Running ${target_name} in ${_RUN_DIR_NORM}")
      else()
        add_custom_target(${_run_target}
          COMMAND ${_run_exe} ${_run_args}
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
