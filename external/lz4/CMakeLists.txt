cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(lz4 VERSION 1.11.0 LANGUAGES C)

set(CMAKE_INSTALL_PREFIX "${DEFOLD_SDK_ROOT}/ext" CACHE PATH "External library install prefix" FORCE)
if(DEFOLD_EXTERNAL_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${DEFOLD_EXTERNAL_INSTALL_PREFIX}" CACHE PATH "External library install prefix" FORCE)
endif()

# Patch a build copy so the vendored upstream sources remain unchanged.
find_package(Git REQUIRED)
set(LZ4_PATCHED_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/patched")
set(LZ4_PATCHED_HC_SOURCE "${LZ4_PATCHED_SOURCE_DIR}/src/lz4hc.c")
add_custom_command(OUTPUT "${LZ4_PATCHED_HC_SOURCE}"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
    "${CMAKE_CURRENT_LIST_DIR}/src" "${LZ4_PATCHED_SOURCE_DIR}/src"
  COMMAND "${GIT_EXECUTABLE}" apply --unsafe-paths
    "--directory=${LZ4_PATCHED_SOURCE_DIR}" "${CMAKE_CURRENT_LIST_DIR}/defold.patch"
  DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/src/lz4.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/lz4.h"
    "${CMAKE_CURRENT_LIST_DIR}/src/lz4hc.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/lz4hc.h"
    "${CMAKE_CURRENT_LIST_DIR}/defold.patch"
  WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../.."
  COMMENT "Applying LZ4 inlining patch"
  VERBATIM)

add_library(lz4 STATIC src/lz4.c "${LZ4_PATCHED_HC_SOURCE}")
target_link_libraries(lz4 PRIVATE defold_sdk)
set_target_properties(lz4 PROPERTIES PREFIX "lib")

install(TARGETS lz4 ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")
install(FILES src/lz4.h src/lz4hc.h DESTINATION include/lz4)
install(FILES LICENSE DESTINATION share/licenses/lz4)
