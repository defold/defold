cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)
project(defold_debugger LANGUAGES ${DEFOLD_LANGUAGE_LIST})

# TCP and blocking Lua stops require a native runtime. Only debug/headless
# engine targets link this library; DM_RELEASE also compiles empty objects.
if(TARGET_PLATFORM MATCHES "web$")
  return()
endif()

defold_add_library(debugger STATIC src/dap.cpp src/debugger.cpp src/debugger_values.cpp src/debugger_extension.cpp)
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
