cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_sdk_module LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN=\"SDK\")

set(SDK_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(SDK_SRC_DIR "${SDK_ROOT}/src")

install(FILES "${SDK_SRC_DIR}/dmsdk/sdk.h" DESTINATION sdk/include/dmsdk)
install(FILES "${SDK_SRC_DIR}/cs/dmsdk/dmsdk.csproj" DESTINATION sdk/cs/dmsdk)

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
