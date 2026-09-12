# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)
project(defold_debugger LANGUAGES ${DEFOLD_LANGUAGE_LIST})

# TCP and blocking Lua stops require a native runtime. Only debug/headless
# engine targets link this library; DM_RELEASE also compiles empty objects.
if(TARGET_PLATFORM MATCHES "web$")
  return()
endif()

defold_add_library(debugger STATIC src/dap.cpp src/debugger.cpp src/debugger_values.cpp src/debugger_inspect.cpp src/debugger_extension.cpp)
target_include_directories(debugger PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
target_include_directories(debugger PRIVATE "${DEFOLD_HOME}/engine/script/src" "${DEFOLD_HOME}/engine")
target_compile_definitions(debugger PRIVATE DLIB_LOG_DOMAIN="DEBUGGER")
defold_target_link_libraries(debugger "${TARGET_PLATFORM}" SCOPE PUBLIC dlib)
defold_target_link_lua(debugger "${TARGET_PLATFORM}" SCOPE PUBLIC)
defold_target_link_socket(debugger "${TARGET_PLATFORM}" PUBLIC)

defold_install_targets(TARGETS debugger ARCHIVE DESTINATION lib/${TARGET_PLATFORM})
install(FILES src/debugger.h DESTINATION include/debugger)

if(BUILD_TESTS AND TARGET_PLATFORM STREQUAL HOST_PLATFORM)
  add_subdirectory(src/test)
endif()
