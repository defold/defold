defold_log("functions_testserver.cmake:")

# Helpers to run unit tests with the local HTTP test server used by Defold.
# Mirrors the logic from engine/gamesys/wscript:shutdown and
# build_tools/waf_dynamo.py:create_test_server_config.
#
# Public API:
#   defold_register_test_with_server(<target> <platform>
#     [PORT <port>] [WORKDIR <dir>] [CONFIG_NAME <name.cfg>])
#
# Effect:
#   - Creates a run target named run_<target>_server that:
#       1) Writes a unittest config file with [server] ip+socket
#       2) Starts test_script_server.Server(ip,port)
#       3) Executes the test binary
#       4) Stops the server and removes the config file
#   - Adds run_<target>_server under the global run_tests aggregate

# Register a test run target that wraps execution with the test server lifecycle.
function(defold_register_test_with_server target platform)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "defold_register_test_with_server: target '${target}' does not exist")
  endif()

  set(options)
  set(oneValueArgs PORT WORKDIR CONFIG_NAME)
  set(multiValueArgs)
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
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_script_server_plugin.py")
    list(APPEND _SERVER_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()
  set(_CFG_PATH "${CMAKE_CURRENT_BINARY_DIR}/${DTS_CONFIG_NAME}")
  set(_WRAP "${DEFOLD_CMAKE_DIR}/testserver.py")
  set(_SERVER_IP "localhost")

  set(_run_target "run_${target}_server")
  if(NOT TARGET ${_run_target})
    add_custom_target(${_run_target}
      COMMAND "${DEFOLD_TESTSERVER_PYTHON3_EXECUTABLE}" "${_WRAP}" $<TARGET_FILE:${target}> "${_RUN_DIR_ABS}" "${_SERVER_IP}" "${DTS_PORT}" "${_CFG_PATH}" ${_SERVER_DIRS}
      DEPENDS ${target}
      USES_TERMINAL
      COMMENT "Running ${target} with Defold test server on ${_SERVER_IP}:${DTS_PORT}")
  endif()

  if(NOT TARGET run_tests)
    add_custom_target(run_tests)
  endif()
  add_dependencies(run_tests ${_run_target})
endfunction()

function(defold_register_tests_with_server group platform)
  set(options)
  set(oneValueArgs PORT WORKDIR CONFIG_NAME)
  set(multiValueArgs TARGETS)
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
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_script_server_plugin.py")
    list(APPEND _SERVER_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()

  set(_SERVER_DIR_ARGS)
  foreach(_SERVER_DIR IN LISTS _SERVER_DIRS)
    list(APPEND _SERVER_DIR_ARGS --server-dir "${_SERVER_DIR}")
  endforeach()

  set(_TEST_EXES)
  foreach(_TARGET IN LISTS DTS_TARGETS)
    if(NOT TARGET ${_TARGET})
      message(FATAL_ERROR "defold_register_tests_with_server: target '${_TARGET}' does not exist")
    endif()
    list(APPEND _TEST_EXES "$<TARGET_FILE:${_TARGET}>")
  endforeach()

  set(_CFG_PATH "${CMAKE_CURRENT_BINARY_DIR}/${DTS_CONFIG_NAME}")
  set(_WRAP "${DEFOLD_CMAKE_DIR}/testserver.py")
  set(_SERVER_IP "localhost")
  set(_run_target "run_${group}_server")

  if(NOT TARGET ${_run_target})
    add_custom_target(${_run_target}
      COMMAND "${DEFOLD_TESTSERVER_PYTHON3_EXECUTABLE}" "${_WRAP}"
        --workdir "${_RUN_DIR_ABS}"
        --ip "${_SERVER_IP}"
        --port "${DTS_PORT}"
        --config "${_CFG_PATH}"
        ${_SERVER_DIR_ARGS}
        -- ${_TEST_EXES}
      DEPENDS ${DTS_TARGETS}
      USES_TERMINAL
      COMMAND_EXPAND_LISTS
      COMMENT "Running ${group} with shared Defold test server on ${_SERVER_IP}:${DTS_PORT}")
  endif()

  if(NOT TARGET run_tests)
    add_custom_target(run_tests)
  endif()
  add_dependencies(run_tests ${_run_target})
endfunction()
