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

function(_defold_cmake_quote out_var value)
  string(REPLACE "\\" "\\\\" _quoted "${value}")
  string(REPLACE "\"" "\\\"" _quoted "${_quoted}")
  set(${out_var} "\"${_quoted}\"" PARENT_SCOPE)
endfunction()

function(_defold_xcode_quote_args out_var)
  set(_quoted_args)
  foreach(_arg IN LISTS ARGN)
    _defold_cmake_quote(_quoted_arg "${_arg}")
    string(APPEND _quoted_args " ${_quoted_arg}")
  endforeach()
  set(${out_var} "${_quoted_args}" PARENT_SCOPE)
endfunction()

function(defold_register_sequential_test_command command_name)
  set(options)
  set(oneValueArgs)
  set(multiValueArgs COMMAND DEPENDS)
  cmake_parse_arguments(DXRT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
  if(NOT DXRT_COMMAND)
    message(FATAL_ERROR "defold_register_sequential_test_command: COMMAND is required")
  endif()

  _defold_cmake_quote(_quoted_name "${command_name}")
  _defold_xcode_quote_args(_quoted_command ${DXRT_COMMAND})
  set_property(GLOBAL APPEND_STRING PROPERTY DEFOLD_RUN_TESTS_SEQUENTIAL_SCRIPT_CONTENT
    "_defold_run_test(${_quoted_name}${_quoted_command})\n")
  if(DXRT_DEPENDS)
    set_property(GLOBAL APPEND PROPERTY DEFOLD_RUN_TESTS_SEQUENTIAL_TARGETS ${DXRT_DEPENDS})
  endif()
endfunction()

function(defold_xcode_register_sequential_test_command command_name)
  defold_register_sequential_test_command(${command_name} ${ARGN})
endfunction()

function(_defold_xcode_register_sequential_run_test target_name run_dir run_exe)
  set(_run_args ${ARGN})
  set(_command_args
    "${CMAKE_COMMAND}" "-E" "env"
    "DEFOLD_HOME=${DEFOLD_HOME}"
    "DYNAMO_HOME=${DEFOLD_SDK_ROOT}"
    "PYTHONPATH=${_test_pythonpath_env}")
  if(run_dir)
    list(APPEND _command_args "${CMAKE_COMMAND}" "-E" "chdir" "${run_dir}" "${run_exe}")
  else()
    list(APPEND _command_args "${run_exe}")
  endif()
  list(APPEND _command_args ${_run_args})

  defold_register_sequential_test_command(${target_name}
    COMMAND ${_command_args}
    DEPENDS prepare_${target_name})
endfunction()

function(defold_finalize_sequential_run_tests)
  if(TARGET run_tests_sequential)
    return()
  endif()

  if(TARGET_PLATFORM MATCHES "arm64-android|armv7-android|x86_64-xbone")
    return()
  endif()

  get_property(_run_test_targets GLOBAL PROPERTY DEFOLD_RUN_TESTS_SEQUENTIAL_TARGETS)
  if(NOT _run_test_targets)
    return()
  endif()
  list(REMOVE_DUPLICATES _run_test_targets)
  list(SORT _run_test_targets)

  _defold_find_python(_python)
  _defold_cmake_quote(_quoted_python "${_python}")
  _defold_cmake_quote(_quoted_interactive_runner "${DEFOLD_HOME}/scripts/cmake/run_interactive.py")

  get_property(_run_test_script_content GLOBAL PROPERTY DEFOLD_RUN_TESTS_SEQUENTIAL_SCRIPT_CONTENT)
  set(_run_test_script_preamble [=[
function(_defold_run_test test_name)
  message(STATUS "Running ${test_name}")
  execute_process(
    COMMAND @DEFOLD_SEQUENTIAL_TEST_PYTHON@ @DEFOLD_SEQUENTIAL_TEST_RUNNER@ ${ARGN}
    RESULT_VARIABLE _result)
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR "${test_name} failed with exit code ${_result}")
  endif()
endfunction()

]=])
  string(REPLACE "@DEFOLD_SEQUENTIAL_TEST_PYTHON@" "${_quoted_python}" _run_test_script_preamble "${_run_test_script_preamble}")
  string(REPLACE "@DEFOLD_SEQUENTIAL_TEST_RUNNER@" "${_quoted_interactive_runner}" _run_test_script_preamble "${_run_test_script_preamble}")
  set(_run_test_script "${CMAKE_BINARY_DIR}/defold_run_tests_sequential_$<CONFIG>.cmake")
  file(GENERATE OUTPUT "${_run_test_script}" CONTENT "${_run_test_script_preamble}${_run_test_script_content}")

  add_custom_target(run_tests_sequential
    COMMAND "${CMAKE_COMMAND}" -P "${_run_test_script}"
    DEPENDS ${_run_test_targets}
    USES_TERMINAL
    COMMENT "Running Defold tests sequentially")
endfunction()

function(defold_finalize_xcode_run_tests)
  if(NOT CMAKE_GENERATOR STREQUAL "Xcode")
    return()
  endif()

  defold_finalize_sequential_run_tests()
  if(TARGET run_tests_sequential AND NOT TARGET run_tests)
    add_custom_target(run_tests DEPENDS run_tests_sequential)
  endif()
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

function(_defold_force_load_ios_testmain target_name)
  if(TARGET_PLATFORM MATCHES "ios$" AND TARGET testmain)
    target_link_options(${target_name} PRIVATE "-Wl,-force_load,$<TARGET_FILE:testmain>")
  endif()
endfunction()

function(_defold_ios_bundle_component out_var value)
  string(TOLOWER "${value}" _component)
  string(REGEX REPLACE "[^a-z0-9-]+" "-" _component "${_component}")
  string(REGEX REPLACE "^-+" "" _component "${_component}")
  string(REGEX REPLACE "-+$" "" _component "${_component}")
  if(NOT _component)
    set(_component "test")
  endif()
  if(NOT _component MATCHES "^[a-z]")
    set(_component "t${_component}")
  endif()
  set(${out_var} "${_component}" PARENT_SCOPE)
endfunction()

function(_defold_ios_test_bundle_id out_var target_name)
  if(DEFOLD_IOS_TEST_BUNDLE_ID_PREFIX)
    set(_prefix "${DEFOLD_IOS_TEST_BUNDLE_ID_PREFIX}")
  else()
    set(_prefix "com.defold.tests")
  endif()
  string(REGEX REPLACE "\\.+$" "" _prefix "${_prefix}")
  _defold_ios_bundle_component(_bundle_component "${target_name}")
  set(${out_var} "${_prefix}.${_bundle_component}" PARENT_SCOPE)
endfunction()

function(_defold_ios_stage_target out_var source target)
  string(REPLACE "\\" "/" _target "${target}")
  string(REGEX REPLACE "^/+" "" _target "${_target}")
  if(NOT _target)
    get_filename_component(_target "${source}" NAME)
  endif()
  set(${out_var} "${_target}" PARENT_SCOPE)
endfunction()

function(_defold_stage_ios_xcode_test_app target_name run_dir_norm configfile)
  if(NOT (TARGET_PLATFORM MATCHES "ios$" AND CMAKE_GENERATOR STREQUAL "Xcode"))
    return()
  endif()

  set(_stage_files ${ARGN})
  get_filename_component(_library_name "${run_dir_norm}" NAME)
  if(NOT _library_name)
    set(_library_name "test")
  endif()
  set(_stage_root "defold-tests/${_library_name}")
  set(_copy_script "${DEFOLD_HOME}/scripts/cmake/functions_ios.cmake")

  if(configfile)
    if(IS_ABSOLUTE "${configfile}")
      set(_config_source "${configfile}")
    else()
      set(_config_source "${run_dir_norm}/${configfile}")
    endif()
    add_custom_command(TARGET ${target_name} POST_BUILD
      COMMAND "${CMAKE_COMMAND}"
        "-DDEFOLD_IOS_STAGE_SOURCE=${_config_source}"
        "-DDEFOLD_IOS_STAGE_BUNDLE_DESTINATION=${_stage_root}/unittest.cfg"
        -P "${_copy_script}"
      VERBATIM)
  endif()

  list(LENGTH _stage_files _stage_len)
  math(EXPR _stage_remainder "${_stage_len} % 2")
  if(NOT _stage_remainder EQUAL 0)
    message(FATAL_ERROR "defold_register_test_target: STAGE_FILES requires SOURCE TARGET pairs")
  endif()

  set(_idx 0)
  while(_idx LESS _stage_len)
    list(GET _stage_files ${_idx} _source)
    math(EXPR _target_idx "${_idx} + 1")
    list(GET _stage_files ${_target_idx} _target)
    if(IS_ABSOLUTE "${_source}")
      set(_source_path "${_source}")
    else()
      set(_source_path "${run_dir_norm}/${_source}")
    endif()
    _defold_ios_stage_target(_stage_target "${_source_path}" "${_target}")
    add_custom_command(TARGET ${target_name} POST_BUILD
      COMMAND "${CMAKE_COMMAND}"
        "-DDEFOLD_IOS_STAGE_SOURCE=${_source_path}"
        "-DDEFOLD_IOS_STAGE_BUNDLE_DESTINATION=${_stage_root}/${_stage_target}"
        -P "${_copy_script}"
      VERBATIM)
    math(EXPR _idx "${_idx} + 2")
  endwhile()
endfunction()

function(_defold_configure_ios_xcode_test_app target_name run_dir_norm configfile)
  if(NOT (TARGET_PLATFORM MATCHES "ios$" AND CMAKE_GENERATOR STREQUAL "Xcode"))
    return()
  endif()
  if(NOT DEFINED DEFOLD_HOME)
    get_filename_component(DEFOLD_HOME "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
  endif()

  _defold_ios_test_bundle_id(_bundle_id "${target_name}")
  defold_xcode_configure_ios_app(${target_name}
    BUNDLE_ID "${_bundle_id}"
    SHORT_VERSION "1.0"
    BUNDLE_VERSION "1")
  set_target_properties(${target_name} PROPERTIES
    XCODE_ATTRIBUTE_INFOPLIST_KEY_UIRequiresFullScreen "YES"
    XCODE_ATTRIBUTE_INFOPLIST_KEY_NSLocalNetworkUsageDescription "Defold iOS tests connect to the local test server running on the build host.")

  _defold_stage_ios_xcode_test_app(${target_name} "${run_dir_norm}" "${configfile}" ${ARGN})
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

function(_defold_add_ios_run_target run_target target_name run_dir_norm configfile ios_runner_platform)
  set(_stage_files ${ARGN})
  _defold_find_python(_python)
  set(_runner "${DEFOLD_HOME}/build_tools/build_ios.py")

  set(_config_args "")
  if(configfile)
    list(APPEND _config_args --config "${configfile}")
  endif()

  _defold_build_stage_file_args(_stage_args ${_stage_files})

  add_custom_target(${run_target}
    COMMAND ${_run_env} "${_python}" "${_runner}" run-test
      --platform "${ios_runner_platform}"
      --cwd "${run_dir_norm}"
      --program "$<TARGET_FILE:${target_name}>"
      --target "${target_name}"
      ${_config_args}
      ${_stage_args}
    DEPENDS ${target_name}
    USES_TERMINAL
    COMMAND_EXPAND_LISTS
    COMMENT "Running ${target_name} on iOS ${ios_runner_platform}")
endfunction()

function(defold_register_test_target target_name)
  if(NOT TARGET ${target_name})
    message(FATAL_ERROR "defold_register_test_target: target '${target_name}' does not exist")
  endif()

  if(TARGET_PLATFORM MATCHES "x86_64-xbone")
    target_compile_definitions(${target_name} PRIVATE
      JC_TEST_NO_DEATH_TEST
      JC_TEST_USE_PRINTF)
  endif()
  if(DEFINED DEFOLD_PLATFORM_TEST_DEFINES)
    set(_platform_test_defines ${DEFOLD_PLATFORM_TEST_DEFINES})
    list(FILTER _platform_test_defines EXCLUDE REGEX "^JC_TEST_USE_COLORS(=.*)?$")
    if(_platform_test_defines)
      target_compile_definitions(${target_name} PRIVATE ${_platform_test_defines})
    endif()
  endif()
  if(DEFOLD_TEST_COLORS)
    target_compile_definitions(${target_name} PRIVATE JC_TEST_USE_COLORS=1)
  else()
    target_compile_definitions(${target_name} PRIVATE JC_TEST_USE_COLORS=0)
  endif()
  if(DEFOLD_PLATFORM_TEST_REQUIRES_TESTMAIN AND TARGET testmain)
    target_link_libraries(${target_name} PRIVATE testmain)
  endif()
  if(DEFOLD_PLATFORM_TEST_LINK_OPTIONS)
    target_link_options(${target_name} PRIVATE ${DEFOLD_PLATFORM_TEST_LINK_OPTIONS})
  endif()
  _defold_force_load_ios_testmain(${target_name})

  # Keep tests out of the default 'all' build. They are built via build_tests
  # or when directly requested. This mirrors typical Waf behavior.
  set_target_properties(${target_name} PROPERTIES EXCLUDE_FROM_ALL TRUE)

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

  set(_IOS_RUNNER_PLATFORM "")
  set(_IOS_RUN_DIR_NORM "")
  if(TARGET_PLATFORM MATCHES "^(arm64-ios|x86_64-ios)$")
    if(_RUN_DIR_NORM)
      set(_IOS_RUN_DIR_NORM "${_RUN_DIR_NORM}")
    else()
      get_filename_component(_IOS_RUN_DIR_NORM "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
      get_filename_component(_IOS_RUN_DIR_NORM "${_IOS_RUN_DIR_NORM}" REALPATH)
    endif()
    if(TARGET_PLATFORM STREQUAL "x86_64-ios")
      set(_IOS_RUNNER_PLATFORM "simulator")
    else()
      set(_IOS_RUNNER_PLATFORM "device")
    endif()
  endif()

  if(_IOS_RUNNER_PLATFORM)
    _defold_configure_ios_xcode_test_app(${target_name} "${_IOS_RUN_DIR_NORM}" "${_TEST_CONFIGFILE}" ${DEFOLD_TEST_STAGE_FILES})
  endif()

  if(CMAKE_GENERATOR STREQUAL "Xcode")
    set(_xcode_test_metadata "${CMAKE_BINARY_DIR}/defold_xcode_test_schemes.tsv")
    get_property(_xcode_test_metadata_initialized GLOBAL PROPERTY DEFOLD_XCODE_TEST_METADATA_INITIALIZED)
    if(NOT _xcode_test_metadata_initialized)
      file(WRITE "${_xcode_test_metadata}" "")
      set_property(GLOBAL PROPERTY DEFOLD_XCODE_TEST_METADATA_INITIALIZED TRUE)
    endif()
    if(_IOS_RUNNER_PLATFORM)
      set(_xcode_run_dir "${_IOS_RUN_DIR_NORM}")
    else()
      set(_xcode_run_dir "${_RUN_DIR_NORM}")
    endif()
    set(_xcode_test_metadata_line "${target_name}\t${_xcode_run_dir}\t${TARGET_PLATFORM}\t${_IOS_RUNNER_PLATFORM}\t${_TEST_CONFIGFILE}")
    foreach(_stage_file IN LISTS DEFOLD_TEST_STAGE_FILES)
      string(APPEND _xcode_test_metadata_line "\t${_stage_file}")
    endforeach()
    string(APPEND _xcode_test_metadata_line "\n")
    file(APPEND "${_xcode_test_metadata}" "${_xcode_test_metadata_line}")
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
      if(NOT DEFINED DEFOLD_HOME)
        get_filename_component(DEFOLD_HOME "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
      endif()
      set(_test_pythonpath_entries
        "${DEFOLD_SDK_ROOT}/lib/python"
        "${DEFOLD_HOME}/build_tools"
        "${DEFOLD_SDK_ROOT}/ext/lib/python")
      if(DEFINED ENV{PYTHONPATH} AND NOT "$ENV{PYTHONPATH}" STREQUAL "")
        file(TO_CMAKE_PATH "$ENV{PYTHONPATH}" _test_existing_pythonpath)
        list(APPEND _test_pythonpath_entries ${_test_existing_pythonpath})
      endif()
      cmake_path(CONVERT "${_test_pythonpath_entries}" TO_NATIVE_PATH_LIST _test_pythonpath)
      string(REPLACE ";" "$<SEMICOLON>" _test_pythonpath_env "${_test_pythonpath}")
      set(_run_env
        "${CMAKE_COMMAND}" "-E" "env"
        "DEFOLD_HOME=${DEFOLD_HOME}"
        "DYNAMO_HOME=${DEFOLD_SDK_ROOT}"
        "PYTHONPATH=${_test_pythonpath_env}")
      if(DEFOLD_MSVC_IDE_SOLUTION AND NOT TARGET_PLATFORM MATCHES "arm64-android|armv7-android|wasm-web|wasm_pthread-web")
        set(_vs_debugger_working_directory "$<TARGET_FILE_DIR:${target_name}>")
        if(_RUN_DIR_NORM)
          set(_vs_debugger_working_directory "${_RUN_DIR_NORM}")
        endif()
        set(_vs_debugger_environment
          "DEFOLD_HOME=${DEFOLD_HOME}\nDYNAMO_HOME=${DEFOLD_SDK_ROOT}\nPYTHONPATH=${_test_pythonpath_env}")
        set_target_properties(${target_name} PROPERTIES
          VS_DEBUGGER_WORKING_DIRECTORY "${_vs_debugger_working_directory}"
          VS_DEBUGGER_ENVIRONMENT "${_vs_debugger_environment}")
      endif()
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
      elseif(TARGET_PLATFORM MATCHES "^(arm64-ios|x86_64-ios)$")
        _defold_add_ios_run_target(${_run_target} ${target_name} "${_IOS_RUN_DIR_NORM}" "${_TEST_CONFIGFILE}" "${_IOS_RUNNER_PLATFORM}" ${DEFOLD_TEST_STAGE_FILES})

      elseif(TARGET_PLATFORM MATCHES "x86_64-xbone")
        _defold_find_python(_python)
        set(_runner "")
        if(DEFOLD_XBONE_PRIVATE_REPO_ROOT)
          set(_runner_candidate "${DEFOLD_XBONE_PRIVATE_REPO_ROOT}/scripts/build_xbone.py")
          if(EXISTS "${_runner_candidate}")
            set(_runner "${_runner_candidate}")
          endif()
        endif()
        set(_run_dir "${_RUN_DIR_NORM}")
        if(NOT _run_dir)
          set(_run_dir "${CMAKE_CURRENT_BINARY_DIR}")
        endif()
        set(_config_args "")
        if(_TEST_CONFIGFILE)
          list(APPEND _config_args --config "${_TEST_CONFIGFILE}")
        endif()
        if(_runner)
          add_custom_target(${_run_target}
            COMMAND "${_python}" "${_runner}" run-test
              --cwd "${_run_dir}"
              --program "$<TARGET_FILE:${target_name}>"
              --private-repo-root "${DEFOLD_XBONE_PRIVATE_REPO_ROOT}"
              ${_config_args}
            DEPENDS ${target_name}
            USES_TERMINAL
            COMMAND_EXPAND_LISTS
            COMMENT "Running ${target_name} on Xbox console")
        else()
          message(WARNING "defold_register_test_target: Xbox test runner not found for '${target_name}'. The run target will fail until the private Xbox build_xbone.py hook is available.")
          add_custom_target(${_run_target}
            COMMAND ${CMAKE_COMMAND} -E false
            DEPENDS ${target_name}
            USES_TERMINAL
            COMMENT "Xbox test runner missing for ${target_name}")
        endif()
      elseif(_RUN_DIR_NORM)
        add_custom_target(${_run_target}
          COMMAND ${_run_env} ${CMAKE_COMMAND} -E chdir "${_RUN_DIR_NORM}" ${_run_exe} ${_run_args}
          DEPENDS ${target_name}
          USES_TERMINAL
          COMMENT "Running ${target_name} in ${_RUN_DIR_NORM}")
      else()
        add_custom_target(${_run_target}
          COMMAND ${_run_env} ${_run_exe} ${_run_args}
          DEPENDS ${target_name}
          USES_TERMINAL
          COMMENT "Running ${target_name}")
      endif()
    endif()
    set(_sequential_dep ${target_name})
    if(CMAKE_GENERATOR STREQUAL "Xcode" AND NOT TARGET_PLATFORM MATCHES "arm64-android|armv7-android|x86_64-xbone")
      set(_prepare_target "prepare_${target_name}")
      if(NOT TARGET ${_prepare_target})
        add_custom_target(${_prepare_target} DEPENDS ${target_name})
      endif()
      set(_sequential_dep ${_prepare_target})
    endif()

    if(NOT TARGET_PLATFORM MATCHES "arm64-android|armv7-android|arm64-ios|x86_64-ios|x86_64-xbone")
      set(_sequential_command ${_run_env})
      if(_RUN_DIR_NORM)
        list(APPEND _sequential_command ${CMAKE_COMMAND} -E chdir "${_RUN_DIR_NORM}" ${_run_exe})
      else()
        list(APPEND _sequential_command ${_run_exe})
      endif()
      list(APPEND _sequential_command ${_run_args})
      defold_register_sequential_test_command(${target_name}
        COMMAND ${_sequential_command}
        DEPENDS ${_sequential_dep})
    endif()

    if(NOT CMAKE_GENERATOR STREQUAL "Xcode" OR TARGET_PLATFORM MATCHES "arm64-android|armv7-android|arm64-ios|x86_64-ios")
      if(NOT TARGET run_tests)
        add_custom_target(run_tests)
      endif()
    endif()
    if(TARGET_PLATFORM MATCHES "arm64-android|armv7-android")
      _defold_register_android_batch_target(_android_stop_target ${target_name} "${_RUN_DIR_NORM}" "${_TEST_CONFIGFILE}" ${DEFOLD_TEST_STAGE_FILES})
      add_dependencies(run_tests ${_android_stop_target})
    elseif(TARGET_PLATFORM MATCHES "^(arm64-ios|x86_64-ios)$")
      add_dependencies(run_tests ${_run_target})
    elseif(NOT CMAKE_GENERATOR STREQUAL "Xcode")
      add_dependencies(run_tests ${_run_target})
    endif()
  endif()
endfunction()
