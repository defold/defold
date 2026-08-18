cmake_minimum_required(VERSION 3.20)

project(defold_protobuf_package NONE)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(functions)
include(platform_host)

set(TARGET_PLATFORM "${HOST_PLATFORM}" CACHE STRING "Defold desktop target platform")
set(PROTOBUF_BUILD_JOBS 8 CACHE STRING "Parallel jobs used for the nested Protobuf builds")
set(PROTOBUF_PACKAGE_DIR "${CMAKE_CURRENT_LIST_DIR}/package" CACHE PATH "Directory for generated package archives")

if(NOT TARGET_PLATFORM MATCHES "^(arm64-macos|x86_64-macos|arm64-linux|x86_64-linux|win32|x86_64-win32)$")
  message(FATAL_ERROR
    "Protobuf is packaged for Defold desktop targets only; got TARGET_PLATFORM=${TARGET_PLATFORM}")
endif()

find_program(BASH_EXECUTABLE bash REQUIRED)

set(PROTOBUF_VERSION 35.1)
set(PROTOBUF_ARCHIVE "${CMAKE_CURRENT_LIST_DIR}/protobuf-${PROTOBUF_VERSION}.tar.gz")
set(ABSEIL_ARCHIVE "${CMAKE_CURRENT_LIST_DIR}/abseil-cpp-20250512.1.tar.gz")
set(PROTOBUF_PLATFORM_PACKAGE
  "${PROTOBUF_PACKAGE_DIR}/protobuf-${PROTOBUF_VERSION}-${TARGET_PLATFORM}.tar.gz")
set(PROTOBUF_COMMON_PACKAGE
  "${PROTOBUF_PACKAGE_DIR}/protobuf-${PROTOBUF_VERSION}-common.tar.gz")

add_custom_command(
  OUTPUT
    "${PROTOBUF_PLATFORM_PACKAGE}"
    "${PROTOBUF_COMMON_PACKAGE}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROTOBUF_PACKAGE_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E env
    "PROTOBUF_BUILD_ROOT=${CMAKE_CURRENT_BINARY_DIR}/work"
    "PROTOBUF_PACKAGE_DIR=${PROTOBUF_PACKAGE_DIR}"
    "PROTOBUF_BUILD_JOBS=${PROTOBUF_BUILD_JOBS}"
    "${BASH_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/build_protobuf.sh" "${TARGET_PLATFORM}"
  DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/build_protobuf.sh"
    "${CMAKE_CURRENT_LIST_DIR}/../../share/ext/common.sh"
    "${PROTOBUF_ARCHIVE}"
    "${ABSEIL_ARCHIVE}"
  COMMENT "Building and packaging Protobuf ${PROTOBUF_VERSION} for ${TARGET_PLATFORM}"
  USES_TERMINAL
  VERBATIM)

add_custom_target(protobuf_package ALL
  DEPENDS
    "${PROTOBUF_PLATFORM_PACKAGE}"
    "${PROTOBUF_COMMON_PACKAGE}")

message(STATUS "Protobuf target platform: ${TARGET_PLATFORM}")
message(STATUS "Protobuf packages: ${PROTOBUF_PACKAGE_DIR}")
