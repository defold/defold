cmake_minimum_required(VERSION 4.0)

# Share SDK discovery between the generators and build.py's check_sdk command.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../scripts/cmake")
include(functions)
include(sdk_emscripten)
file(GLOB node_dirs "${_EMSCRIPTEN_DIR}/../../node/*/bin")
find_program(NODE_JS_EXECUTABLE NAMES node nodejs HINTS ${node_dirs} REQUIRED)

if(LUAJIT_CHECK_SDK)
  message(STATUS "Checking Emscripten and Node for LuaJIT's ARMv7 VM generator")
  execute_process(COMMAND "${_EMSCRIPTEN_DIR}/emcc" --version
    OUTPUT_VARIABLE compiler_version COMMAND_ERROR_IS_FATAL ANY)
  execute_process(COMMAND "${NODE_JS_EXECUTABLE}" --version
    OUTPUT_VARIABLE node_version OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
  string(REGEX MATCH "^[^\n\r]+" compiler_version "${compiler_version}")
  message(STATUS "Found LuaJIT generator compiler: ${_EMSCRIPTEN_DIR}/emcc (${compiler_version})")
  message(STATUS "Found LuaJIT generator runner: ${NODE_JS_EXECUTABLE} (${node_version})")
endif()
