defold_log("functions_test.cmake:")

set(_DEFOLD_BUN_MIN_VERSION "1.3.13")

# Registers a test target with the global build_tests and run_tests targets.
#
# Usage:
#   defold_register_test_target(<target> [run_flag] [run_workdir]
#                               [CONFIGFILE <configfile>]
#                               [STAGE_FILES <source> <target> ...])
#   - run_flag: ON/OFF (default ON). If ON, creates a per-test run target and
#               adds it to global run_tests
#   - run_workdir: optional working directory for executing the test
#   - CONFIGFILE: optional config file path relative to run_workdir
#   - STAGE_FILES: optional flattened list of SOURCE TARGET pairs for test
#                  runtime staging. Platform-specific runners decide if/how
#                  these files are uploaded or otherwise prepared.

function(_defold_is_bun_version_supported candidate out_var)
  set(_ok TRUE)
  if(candidate MATCHES "bun[^/]*$")
    execute_process(
      COMMAND "${candidate}" "--version"
      RESULT_VARIABLE _bun_version_result
      OUTPUT_VARIABLE _bun_version
      ERROR_QUIET
      OUTPUT_STRIP_TRAILING_WHITESPACE
      TIMEOUT 10)
    if(NOT _bun_version_result EQUAL 0 OR _bun_version VERSION_LESS _DEFOLD_BUN_MIN_VERSION)
      set(_ok FALSE)
    endif()
  endif()
  set(${out_var} "${_ok}" PARENT_SCOPE)
endfunction()

function(_defold_is_wasm_runner candidate out_var)
  set(_ok FALSE)
  if(candidate)
    _defold_is_bun_version_supported("${candidate}" _bun_version_ok)
    if(_bun_version_ok)
      set(_check_script "const fs = require('fs'); if (typeof process === 'undefined') throw new Error('process is missing'); if (!process.versions || !process.versions.node) throw new Error('Node-compatible process.versions.node is missing'); if (typeof Buffer === 'undefined') throw new Error('Buffer is missing'); if (typeof WebAssembly === 'undefined') throw new Error('WebAssembly is missing'); fs.realpathSync(process.cwd());")
      execute_process(
        COMMAND "${candidate}" "-e" "${_check_script}"
        RESULT_VARIABLE _runner_result
        OUTPUT_QUIET
        ERROR_QUIET
        TIMEOUT 10)
      if(_runner_result EQUAL 0)
        set(_ok TRUE)
      endif()
    endif()
  endif()
  set(${out_var} "${_ok}" PARENT_SCOPE)
endfunction()

function(_defold_find_wasm_runner out_var)
  set(_runner "")

  find_program(_bun bun)
  _defold_is_wasm_runner("${_bun}" _bun_ok)
  if(_bun_ok)
    set(${out_var} "${_bun}" PARENT_SCOPE)
    return()
  endif()

  set(_node_candidates "")
  if(DEFINED DEFOLD_SDK_ROOT)
    file(GLOB _nodes "${DEFOLD_SDK_ROOT}/ext/SDKs/emsdk*/node/*/bin/node")
    list(SORT _nodes)
    list(LENGTH _nodes _len)
    if(_len GREATER 0)
      math(EXPR _idx "${_len} - 1")
      list(GET _nodes ${_idx} _node)
      list(APPEND _node_candidates "${_node}")
    endif()
  endif()

  if(DEFINED ENV{EMSDK})
    file(GLOB _nodes "$ENV{EMSDK}/node/*/bin/node")
    list(SORT _nodes)
    list(LENGTH _nodes _len)
    if(_len GREATER 0)
      math(EXPR _idx "${_len} - 1")
      list(GET _nodes ${_idx} _node)
      list(APPEND _node_candidates "${_node}")
    endif()
  endif()

  if(DEFINED EMSCRIPTEN)
    get_filename_component(_emsdk_root "${EMSCRIPTEN}" DIRECTORY)
    get_filename_component(_emsdk_root "${_emsdk_root}" DIRECTORY)
    file(GLOB _nodes "${_emsdk_root}/node/*/bin/node")
    list(SORT _nodes)
    list(LENGTH _nodes _len)
    if(_len GREATER 0)
      math(EXPR _idx "${_len} - 1")
      list(GET _nodes ${_idx} _node)
      list(APPEND _node_candidates "${_node}")
    endif()
  endif()

  find_program(_nodejs NAMES node nodejs)
  if(_nodejs)
    list(APPEND _node_candidates "${_nodejs}")
  endif()

  foreach(_node IN LISTS _node_candidates)
    _defold_is_wasm_runner("${_node}" _node_ok)
    if(_node_ok)
      set(_runner "${_node}")
      break()
    endif()
  endforeach()

  set(${out_var} "${_runner}" PARENT_SCOPE)
endfunction()

function(_defold_find_python out_var)
  find_program(_python NAMES python3 python)
  if(NOT _python)
    message(FATAL_ERROR "defold_register_test_target: python3/python not found")
  endif()
  set(${out_var} "${_python}" PARENT_SCOPE)
endfunction()

function(_defold_build_stage_file_args out_var)
  set(_stage_args "")
  set(_stage_list ${ARGN})
  list(LENGTH _stage_list _stage_len)
  math(EXPR _stage_remainder "${_stage_len} % 2")
  if(NOT _stage_remainder EQUAL 0)
    message(FATAL_ERROR "defold_register_test_target: STAGE_FILES requires SOURCE TARGET pairs")
  endif()

  set(_idx 0)
  while(_idx LESS _stage_len)
    list(GET _stage_list ${_idx} _source)
    math(EXPR _target_idx "${_idx} + 1")
    list(GET _stage_list ${_target_idx} _target)
    list(APPEND _stage_args --stage "${_source}" "${_target}")
    math(EXPR _idx "${_idx} + 2")
  endwhile()

  set(${out_var} "${_stage_args}" PARENT_SCOPE)
endfunction()

function(_defold_register_android_batch_target out_var target_name run_dir_norm configfile)
  set(_stage_files ${ARGN})
  _defold_find_python(_python)
  set(_runner "${DEFOLD_HOME}/build_tools/build_android.py")

  set(_config_args "")
  if(configfile)
    list(APPEND _config_args --config "${configfile}")
  endif()

  _defold_build_stage_file_args(_stage_args ${_stage_files})

  set(_library_key "${run_dir_norm}|${configfile}|${_stage_files}")
  string(MD5 _library_id "${_library_key}")
  set(_prepare_target "prepare_android_tests_${_library_id}")
  set(_stop_target "stop_android_tests_${_library_id}")
  set(_batch_run_target "run_android_batch_${target_name}")

  if(NOT TARGET ${_prepare_target})
    add_custom_target(${_prepare_target}
      COMMAND "${_python}" "${_runner}" prepare --cwd "${run_dir_norm}" ${_config_args} ${_stage_args}
      USES_TERMINAL
      COMMAND_EXPAND_LISTS
      COMMENT "Preparing Android test library in ${run_dir_norm}")
  endif()

  if(NOT TARGET ${_batch_run_target})
    add_custom_target(${_batch_run_target}
      COMMAND "${_python}" "${_runner}" run-test --cwd "${run_dir_norm}" --program "$<TARGET_FILE:${target_name}>" ${_config_args}
      DEPENDS ${target_name} ${_prepare_target}
      USES_TERMINAL
      COMMAND_EXPAND_LISTS
      COMMENT "Running ${target_name} on Android device")
  endif()

  if(NOT TARGET ${_stop_target})
    add_custom_target(${_stop_target}
      COMMAND "${_python}" "${_runner}" stop --cwd "${run_dir_norm}" ${_config_args}
      USES_TERMINAL
      COMMAND_EXPAND_LISTS
      COMMENT "Stopping Android test library in ${run_dir_norm}")
  endif()

  add_dependencies(${_stop_target} ${_batch_run_target})
  set(${out_var} "${_stop_target}" PARENT_SCOPE)
endfunction()

function(defold_register_test_target target_name)
  if(NOT TARGET ${target_name})
    message(FATAL_ERROR "defold_register_test_target: target '${target_name}' does not exist")
  endif()

  if(TARGET_PLATFORM MATCHES "arm64-android|armv7-android")
    target_compile_definitions(${target_name} PRIVATE JC_TEST_USE_COLORS=1)
  endif()

  # Keep tests out of the default 'all' build. They are built via build_tests
  # or when directly requested. This mirrors typical Waf behavior.
  set_target_properties(${target_name} PROPERTIES EXCLUDE_FROM_ALL TRUE)

  # Ensure global aggregator exists
  if(NOT TARGET build_tests)
    add_custom_target(build_tests)
  endif()
  add_dependencies(build_tests ${target_name})

  set(_known_keywords CONFIGFILE STAGE_FILES)
  set(_legacy_args "")
  set(_keyword_args "")
  set(_in_keyword_args FALSE)
  foreach(_arg IN LISTS ARGN)
    list(FIND _known_keywords "${_arg}" _keyword_idx)
    if(NOT _keyword_idx EQUAL -1)
      set(_in_keyword_args TRUE)
    endif()

    if(_in_keyword_args)
      list(APPEND _keyword_args "${_arg}")
    else()
      list(APPEND _legacy_args "${_arg}")
    endif()
  endforeach()

  list(LENGTH _legacy_args _legacy_len)
  if(_legacy_len GREATER 2)
    message(FATAL_ERROR "defold_register_test_target: expected at most [run_flag] [run_workdir] before keyword arguments")
  endif()

  cmake_parse_arguments(DEFOLD_TEST "" "CONFIGFILE" "STAGE_FILES" ${_keyword_args})

  set(_TEST_CONFIGFILE "${DEFOLD_TEST_CONFIGFILE}")

  # Optional run flag (default ON)
  set(_RUN_TEST ON)
  if(_legacy_len GREATER 0)
    list(GET _legacy_args 0 _RUN_TEST)
  endif()
  string(TOUPPER "${_RUN_TEST}" _RUN_TEST_UPPER)
  if(_RUN_TEST_UPPER STREQUAL "0" OR _RUN_TEST_UPPER STREQUAL "OFF" OR _RUN_TEST_UPPER STREQUAL "FALSE" OR _RUN_TEST_UPPER STREQUAL "NO")
    set(_RUN_TEST FALSE)
  else()
    set(_RUN_TEST TRUE)
  endif()

  # Optional working directory
  set(_RUN_DIR "")
  if(_legacy_len GREATER 1)
    list(GET _legacy_args 1 _RUN_DIR)
  endif()
  set(_RUN_DIR_NORM "")
  if(_RUN_DIR)
    get_filename_component(_RUN_DIR_NORM "${_RUN_DIR}" ABSOLUTE)
    get_filename_component(_RUN_DIR_NORM "${_RUN_DIR_NORM}" REALPATH)
  endif()

  if(_RUN_TEST)
    set(_run_target "run_${target_name}")
    if(NOT TARGET ${_run_target})
      if(TARGET_PLATFORM MATCHES "wasm-web|wasm_pthread-web")
        set(_pre_js "${DEFOLD_SDK_ROOT}/share/web-pre.js")
        if(NOT EXISTS "${_pre_js}")
          if(NOT DEFINED DEFOLD_HOME)
            get_filename_component(DEFOLD_HOME "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
          endif()
          set(_pre_js "${DEFOLD_HOME}/share/web-pre.js")
        endif()
        if(EXISTS "${_pre_js}")
          target_link_options(${target_name} PRIVATE "--pre-js" "${_pre_js}" "-lnodefs.js")
        else()
          message(FATAL_ERROR "defold_register_test_target: missing pre-js file for web test '${target_name}'. Checked '${DEFOLD_SDK_ROOT}/share/web-pre.js' and '${DEFOLD_HOME}/share/web-pre.js'")
        endif()
      endif()
      set(_run_exe "$<TARGET_FILE:${target_name}>")
      set(_run_args "")
      if(TARGET_PLATFORM MATCHES "wasm-web|wasm_pthread-web")
        _defold_find_wasm_runner(_wasm_runner)
        if(NOT _wasm_runner)
          message(WARNING "defold_register_test_target: Bun or Node.js not found for web test '${target_name}'. The run target will fail until a runner is installed.")
          set(_run_exe "${CMAKE_COMMAND}")
          set(_run_args "-E" "false")
        else()
          set(_run_exe "${_wasm_runner}")
          set(_run_args "$<TARGET_FILE:${target_name}>")
        endif()
      endif()
      if(TARGET_PLATFORM MATCHES "arm64-android|armv7-android")
        if(NOT _RUN_DIR_NORM)
          message(FATAL_ERROR "defold_register_test_target: Android test '${target_name}' requires a run_workdir")
        endif()
        _defold_find_python(_python)
        set(_runner "${DEFOLD_HOME}/build_tools/build_android.py")
        set(_config_args "")
        if(_TEST_CONFIGFILE)
          list(APPEND _config_args --config "${_TEST_CONFIGFILE}")
        endif()
        _defold_build_stage_file_args(_stage_args ${DEFOLD_TEST_STAGE_FILES})
        add_custom_target(${_run_target}
          COMMAND "${_python}" "${_runner}" prepare --cwd "${_RUN_DIR_NORM}" ${_config_args} ${_stage_args}
          COMMAND "${_python}" "${_runner}" run-test --cwd "${_RUN_DIR_NORM}" --program "$<TARGET_FILE:${target_name}>" ${_config_args}
          COMMAND "${_python}" "${_runner}" stop --cwd "${_RUN_DIR_NORM}" ${_config_args}
          DEPENDS ${target_name}
          USES_TERMINAL
          COMMAND_EXPAND_LISTS
          COMMENT "Running ${target_name} on Android device")
      elseif(_RUN_DIR_NORM)
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
    if(TARGET_PLATFORM MATCHES "arm64-android|armv7-android")
      _defold_register_android_batch_target(_android_stop_target ${target_name} "${_RUN_DIR_NORM}" "${_TEST_CONFIGFILE}" ${DEFOLD_TEST_STAGE_FILES})
      add_dependencies(run_tests ${_android_stop_target})
    else()
      add_dependencies(run_tests ${_run_target})
    endif()
  endif()
endfunction()
