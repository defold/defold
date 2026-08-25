defold_log("functions_testserver.cmake:")

# Helpers to run unit tests with the local HTTP test server used by Defold.
# Mirrors the logic from engine/gamesys/wscript:shutdown and
# build_tools/waf_dynamo.py:create_test_server_config.
#
# Public API:
#   defold_register_test_with_server(<target> <platform>
#     [PORT <port>] [WORKDIR <dir>] [CONFIG_NAME <name.cfg>]
#     [STAGE_FILES <source> <target> ...])
#
# Effect:
#   - Creates a run target named run_<target>_server that:
#       1) Writes a unittest config file with [server] ip+socket
#       2) Starts test_script_server.Server(ip,port)
#       3) Executes the test binary
#       4) Stops the server and removes the config file
#   - Adds run_<target>_server under the global run_tests aggregate

# Register a test run target that wraps execution with the test server lifecycle.
function(_defold_testserver_ios_runner_args out_var run_dir_abs cfg_path ios_runner_platform)
  set(_stage_files ${ARGN})
  _defold_find_python(_python)
  _defold_build_stage_file_args(_stage_args ${_stage_files})

  set(_runner_args
    "--runner-arg=${_python}"
    "--runner-arg=${DEFOLD_HOME}/build_tools/build_ios.py"
    "--runner-arg=run-test"
    "--runner-arg=--platform"
    "--runner-arg=${ios_runner_platform}"
    "--runner-arg=--cwd"
    "--runner-arg=${run_dir_abs}"
    "--runner-arg=--program"
    "--runner-arg={exe}"
    "--runner-arg=--config"
    "--runner-arg={config}")

  foreach(_stage_arg IN LISTS _stage_args)
    list(APPEND _runner_args "--runner-arg=${_stage_arg}")
  endforeach()

  set(${out_var} "${_runner_args}" PARENT_SCOPE)
endfunction()

function(_defold_testserver_server_dir_args out_var)
  set(_server_dir_args)
  foreach(_server_dir IN LISTS ARGN)
    list(APPEND _server_dir_args --server-dir "${_server_dir}")
  endforeach()
  set(${out_var} "${_server_dir_args}" PARENT_SCOPE)
endfunction()

function(defold_register_test_with_server target platform)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "defold_register_test_with_server: target '${target}' does not exist")
  endif()

  set(options)
  set(oneValueArgs PORT WORKDIR CONFIG_NAME)
  set(multiValueArgs STAGE_FILES)
  cmake_parse_arguments(DTS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT DTS_PORT)
    set(DTS_PORT 9001)
  endif()
  if(NOT DTS_CONFIG_NAME)
    set(DTS_CONFIG_NAME "unittest.cfg")
  endif()

  if(NOT DEFINED DEFOLD_TESTSERVER_PYTHON3_EXECUTABLE)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(DEFOLD_TESTSERVER_PYTHON3_EXECUTABLE "${Python3_EXECUTABLE}" CACHE INTERNAL "Python interpreter used by Defold test server targets")
  endif()

  set(_RUN_DIR "${DTS_WORKDIR}")
  if(_RUN_DIR)
    get_filename_component(_RUN_DIR_ABS "${_RUN_DIR}" ABSOLUTE)
  else()
    set(_RUN_DIR_ABS "")
  endif()

  set(_SERVER_DIRS "${DEFOLD_SDK_ROOT}/lib/python")
  if(EXISTS "${DEFOLD_HOME}/engine/script/test_script_server.py")
    list(APPEND _SERVER_DIRS "${DEFOLD_HOME}/engine/script")
  endif()
  if(_RUN_DIR_ABS AND EXISTS "${_RUN_DIR_ABS}/test_script_server.py")
    list(APPEND _SERVER_DIRS "${_RUN_DIR_ABS}")
  endif()
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_script_server_plugin.py")
    list(APPEND _SERVER_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()
  list(REMOVE_DUPLICATES _SERVER_DIRS)

  set(_SERVER_DIR_ARGS)
  foreach(_SERVER_DIR IN LISTS _SERVER_DIRS)
    list(APPEND _SERVER_DIR_ARGS --server-dir "${_SERVER_DIR}")
  endforeach()

  set(_CFG_PATH "${CMAKE_CURRENT_BINARY_DIR}/${DTS_CONFIG_NAME}")
  set(_WRAP "${DEFOLD_CMAKE_DIR}/testserver.py")
  if(platform STREQUAL "arm64-ios")
    set(_SERVER_IP "auto")
  else()
    set(_SERVER_IP "localhost")
  endif()
  _defold_testserver_server_dir_args(_SERVER_DIR_ARGS ${_SERVER_DIRS})

  set(_ANDROID_ARGS)
  if(platform MATCHES "arm64-android|armv7-android|x86_64-android")
    list(APPEND _ANDROID_ARGS
      --android-runner "${DEFOLD_HOME}/build_tools/build_android.py"
      --android-cwd "${_RUN_DIR_ABS}")
    set(_STAGE_FILES ${DTS_STAGE_FILES})
    list(LENGTH _STAGE_FILES _STAGE_LEN)
    math(EXPR _STAGE_REMAINDER "${_STAGE_LEN} % 2")
    if(NOT _STAGE_REMAINDER EQUAL 0)
      message(FATAL_ERROR "defold_register_test_with_server: STAGE_FILES requires SOURCE TARGET pairs")
    endif()
    set(_STAGE_IDX 0)
    while(_STAGE_IDX LESS _STAGE_LEN)
      list(GET _STAGE_FILES ${_STAGE_IDX} _STAGE_SOURCE)
      math(EXPR _STAGE_TARGET_IDX "${_STAGE_IDX} + 1")
      list(GET _STAGE_FILES ${_STAGE_TARGET_IDX} _STAGE_TARGET)
      list(APPEND _ANDROID_ARGS --android-stage "${_STAGE_SOURCE}" "${_STAGE_TARGET}")
      math(EXPR _STAGE_IDX "${_STAGE_IDX} + 2")
    endwhile()
  endif()

  set(_IOS_RUNNER_ARGS)
  if(platform MATCHES "^(arm64-ios|x86_64-ios)$")
    if(NOT _RUN_DIR_ABS)
      message(FATAL_ERROR "defold_register_test_with_server: iOS test '${target}' requires WORKDIR")
    endif()
    if(platform STREQUAL "x86_64-ios")
      set(_IOS_RUNNER_PLATFORM "simulator")
    else()
      set(_IOS_RUNNER_PLATFORM "device")
    endif()
    _defold_testserver_ios_runner_args(_IOS_RUNNER_ARGS "${_RUN_DIR_ABS}" "${_CFG_PATH}" "${_IOS_RUNNER_PLATFORM}" ${DTS_STAGE_FILES})
  endif()

  set(_run_target "run_${target}_server")
  if(NOT TARGET ${_run_target})
    add_custom_target(${_run_target}
      COMMAND "${DEFOLD_TESTSERVER_PYTHON3_EXECUTABLE}" "${_WRAP}"
        --workdir "${_RUN_DIR_ABS}"
        --ip "${_SERVER_IP}"
        --port "${DTS_PORT}"
        --config "${_CFG_PATH}"
        ${_ANDROID_ARGS}
        ${_SERVER_DIR_ARGS}
        ${_IOS_RUNNER_ARGS}
        -- "$<TARGET_FILE:${target}>"
      DEPENDS ${target}
      USES_TERMINAL
      COMMAND_EXPAND_LISTS
      COMMENT "Running ${target} with Defold test server on ${_SERVER_IP}:${DTS_PORT}")
  endif()

  set(_sequential_dep ${target})
  if(CMAKE_GENERATOR STREQUAL "Xcode" AND NOT platform MATCHES "arm64-android|armv7-android|x86_64-android|arm64-ios|x86_64-ios")
    set(_prepare_target "prepare_${_run_target}")
    if(NOT TARGET ${_prepare_target})
      add_custom_target(${_prepare_target} DEPENDS ${target})
    endif()
    set(_sequential_dep ${_prepare_target})
  endif()
  if(NOT platform MATCHES "arm64-android|armv7-android|x86_64-android|arm64-ios|x86_64-ios")
    defold_register_sequential_test_command(${_run_target}
      COMMAND "${DEFOLD_TESTSERVER_PYTHON3_EXECUTABLE}" "${_WRAP}"
        --workdir "${_RUN_DIR_ABS}"
        --ip "${_SERVER_IP}"
        --port "${DTS_PORT}"
        --config "${_CFG_PATH}"
        ${_SERVER_DIR_ARGS}
        -- "$<TARGET_FILE:${target}>"
      DEPENDS ${_sequential_dep})
  endif()
  if(NOT CMAKE_GENERATOR STREQUAL "Xcode" OR platform MATCHES "arm64-ios|x86_64-ios")
    if(NOT TARGET run_tests)
      add_custom_target(run_tests)
    endif()
    add_dependencies(run_tests ${_run_target})
  endif()
endfunction()

function(defold_register_tests_with_server group platform)
  set(options)
  set(oneValueArgs PORT WORKDIR CONFIG_NAME)
  set(multiValueArgs TARGETS STAGE_FILES)
  cmake_parse_arguments(DTS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT DTS_TARGETS)
    message(FATAL_ERROR "defold_register_tests_with_server: TARGETS is required")
  endif()

  if(NOT DTS_PORT)
    set(DTS_PORT 9001)
  endif()
  if(NOT DTS_CONFIG_NAME)
    set(DTS_CONFIG_NAME "unittest.cfg")
  endif()

  if(NOT DEFINED DEFOLD_TESTSERVER_PYTHON3_EXECUTABLE)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(DEFOLD_TESTSERVER_PYTHON3_EXECUTABLE "${Python3_EXECUTABLE}" CACHE INTERNAL "Python interpreter used by Defold test server targets")
  endif()

  set(_RUN_DIR "${DTS_WORKDIR}")
  if(_RUN_DIR)
    get_filename_component(_RUN_DIR_ABS "${_RUN_DIR}" ABSOLUTE)
  else()
    set(_RUN_DIR_ABS "")
  endif()

  set(_SERVER_DIRS "${DEFOLD_SDK_ROOT}/lib/python")
  if(EXISTS "${DEFOLD_HOME}/engine/script/test_script_server.py")
    list(APPEND _SERVER_DIRS "${DEFOLD_HOME}/engine/script")
  endif()
  if(_RUN_DIR_ABS AND EXISTS "${_RUN_DIR_ABS}/test_script_server.py")
    list(APPEND _SERVER_DIRS "${_RUN_DIR_ABS}")
  endif()
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_script_server_plugin.py")
    list(APPEND _SERVER_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()
  list(REMOVE_DUPLICATES _SERVER_DIRS)

  _defold_testserver_server_dir_args(_SERVER_DIR_ARGS ${_SERVER_DIRS})

  set(_TEST_EXES)
  foreach(_TARGET IN LISTS DTS_TARGETS)
    if(NOT TARGET ${_TARGET})
      message(FATAL_ERROR "defold_register_tests_with_server: target '${_TARGET}' does not exist")
    endif()
    list(APPEND _TEST_EXES "$<TARGET_FILE:${_TARGET}>")
  endforeach()

  set(_CFG_PATH "${CMAKE_CURRENT_BINARY_DIR}/${DTS_CONFIG_NAME}")
  set(_WRAP "${DEFOLD_CMAKE_DIR}/testserver.py")
  if(platform STREQUAL "arm64-ios")
    set(_SERVER_IP "auto")
  else()
    set(_SERVER_IP "localhost")
  endif()

  set(_ANDROID_ARGS)
  if(platform MATCHES "arm64-android|armv7-android|x86_64-android")
    list(APPEND _ANDROID_ARGS
      --android-runner "${DEFOLD_HOME}/build_tools/build_android.py"
      --android-cwd "${_RUN_DIR_ABS}")
    set(_STAGE_FILES ${DTS_STAGE_FILES})
    list(LENGTH _STAGE_FILES _STAGE_LEN)
    math(EXPR _STAGE_REMAINDER "${_STAGE_LEN} % 2")
    if(NOT _STAGE_REMAINDER EQUAL 0)
      message(FATAL_ERROR "defold_register_tests_with_server: STAGE_FILES requires SOURCE TARGET pairs")
    endif()
    set(_STAGE_IDX 0)
    while(_STAGE_IDX LESS _STAGE_LEN)
      list(GET _STAGE_FILES ${_STAGE_IDX} _STAGE_SOURCE)
      math(EXPR _STAGE_TARGET_IDX "${_STAGE_IDX} + 1")
      list(GET _STAGE_FILES ${_STAGE_TARGET_IDX} _STAGE_TARGET)
      list(APPEND _ANDROID_ARGS --android-stage "${_STAGE_SOURCE}" "${_STAGE_TARGET}")
      math(EXPR _STAGE_IDX "${_STAGE_IDX} + 2")
    endwhile()
  endif()
  set(_run_target "run_${group}_server")

  set(_IOS_RUNNER_ARGS)
  if(platform MATCHES "^(arm64-ios|x86_64-ios)$")
    if(NOT _RUN_DIR_ABS)
      message(FATAL_ERROR "defold_register_tests_with_server: iOS test group '${group}' requires WORKDIR")
    endif()
    if(platform STREQUAL "x86_64-ios")
      set(_IOS_RUNNER_PLATFORM "simulator")
    else()
      set(_IOS_RUNNER_PLATFORM "device")
    endif()
    _defold_testserver_ios_runner_args(_IOS_RUNNER_ARGS "${_RUN_DIR_ABS}" "${_CFG_PATH}" "${_IOS_RUNNER_PLATFORM}" ${DTS_STAGE_FILES})
  endif()

  if(NOT TARGET ${_run_target})
    add_custom_target(${_run_target}
      COMMAND "${DEFOLD_TESTSERVER_PYTHON3_EXECUTABLE}" "${_WRAP}"
        --workdir "${_RUN_DIR_ABS}"
        --ip "${_SERVER_IP}"
        --port "${DTS_PORT}"
        --config "${_CFG_PATH}"
        ${_ANDROID_ARGS}
        ${_SERVER_DIR_ARGS}
        ${_IOS_RUNNER_ARGS}
        -- ${_TEST_EXES}
      DEPENDS ${DTS_TARGETS}
      USES_TERMINAL
      COMMAND_EXPAND_LISTS
      COMMENT "Running ${group} with shared Defold test server on ${_SERVER_IP}:${DTS_PORT}")
  endif()

  set(_sequential_dep ${DTS_TARGETS})
  if(CMAKE_GENERATOR STREQUAL "Xcode" AND NOT platform MATCHES "arm64-android|armv7-android|x86_64-android|arm64-ios|x86_64-ios")
    set(_prepare_target "prepare_${_run_target}")
    if(NOT TARGET ${_prepare_target})
      add_custom_target(${_prepare_target} DEPENDS ${DTS_TARGETS})
    endif()
    set(_sequential_dep ${_prepare_target})
  endif()
  if(NOT platform MATCHES "arm64-android|armv7-android|x86_64-android|arm64-ios|x86_64-ios")
    defold_register_sequential_test_command(${_run_target}
      COMMAND "${DEFOLD_TESTSERVER_PYTHON3_EXECUTABLE}" "${_WRAP}"
        --workdir "${_RUN_DIR_ABS}"
        --ip "${_SERVER_IP}"
        --port "${DTS_PORT}"
        --config "${_CFG_PATH}"
        ${_SERVER_DIR_ARGS}
        -- ${_TEST_EXES}
      DEPENDS ${_sequential_dep})
  endif()
  if(NOT CMAKE_GENERATOR STREQUAL "Xcode" OR platform MATCHES "arm64-ios|x86_64-ios")
    if(NOT TARGET run_tests)
      add_custom_target(run_tests)
    endif()
    add_dependencies(run_tests ${_run_target})
  endif()
endfunction()
