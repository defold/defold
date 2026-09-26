cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(libunibreak_external VERSION 6.1 LANGUAGES C)

set(UNIBREAK_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/package/libunibreak-6.1/src")
file(GLOB UNIBREAK_SOURCES CONFIGURE_DEPENDS "${UNIBREAK_SOURCE_DIR}/*.c")
list(FILTER UNIBREAK_SOURCES EXCLUDE REGEX "tests\\.c$")

add_library(unibreak STATIC ${UNIBREAK_SOURCES})
target_include_directories(unibreak PRIVATE "${UNIBREAK_SOURCE_DIR}")
target_compile_definitions(unibreak PRIVATE NDEBUG)
# Source options come after the shared toolchain's optimization flags.
set_source_files_properties(${UNIBREAK_SOURCES}
  PROPERTIES COMPILE_OPTIONS "$<IF:$<BOOL:${MSVC}>,/O2,-O3>")
target_link_libraries(unibreak PRIVATE defold_sdk)
set_target_properties(unibreak PROPERTIES PREFIX "lib")

if(DEFOLD_EXTERNAL_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${DEFOLD_EXTERNAL_INSTALL_PREFIX}" CACHE PATH "External package install prefix" FORCE)
endif()

install(TARGETS unibreak ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")
install(DIRECTORY "${UNIBREAK_SOURCE_DIR}/" DESTINATION include/libunibreak
  FILES_MATCHING PATTERN "*.h")
