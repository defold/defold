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

# Internal: Resolve the default IP used by tests for a platform tuple.
# For desktop targets we use "localhost" (mirroring gamesys/wscript:shutdown).
# For others, we still default to "localhost" for robustness.
function(_defold_testserver_default_ip OUT_VAR PLATFORM)
  # Desktop and simulator targets
  if("${PLATFORM}" MATCHES "(win32|x86_64-win32|x86_64-linux|x86_64-macos|arm64-macos)")
    set(_ip "localhost")
  else()
    set(_ip "localhost")
  endif()
  set(${OUT_VAR} "${_ip}" PARENT_SCOPE)
endfunction()

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

  # Resolve IP address
  _defold_testserver_default_ip(_DEFOLD_TS_IP "${platform}")

  # Locate Python to run the server wrapper
  find_package(Python3 COMPONENTS Interpreter REQUIRED)

  # Paths used by the wrapper
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

  # Create a run target that calls the shared Python wrapper
  set(_run_target "run_${target}_server")
  if(NOT TARGET ${_run_target})
    add_custom_target(${_run_target}
      COMMAND ${Python3_EXECUTABLE} "${_WRAP}" $<TARGET_FILE:${target}> "${_RUN_DIR_ABS}" "${_DEFOLD_TS_IP}" "${DTS_PORT}" "${_CFG_PATH}" ${_SERVER_DIRS}
      DEPENDS ${target}
      USES_TERMINAL
      COMMENT "Running ${target} with Defold test server on ${_DEFOLD_TS_IP}:${DTS_PORT}")
  endif()

  # Make sure global run_tests exists and include this run target
  if(NOT TARGET run_tests)
    add_custom_target(run_tests)
  endif()
  add_dependencies(run_tests ${_run_target})
endfunction()
