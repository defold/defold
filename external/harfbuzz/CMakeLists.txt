cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(harfbuzz_external VERSION 13.2.1 LANGUAGES C CXX)

set(HARFBUZZ_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/package/harfbuzz-13.2.1/src")

add_library(harfbuzz STATIC "${HARFBUZZ_SOURCE_DIR}/harfbuzz.cc")
target_include_directories(harfbuzz PRIVATE "${HARFBUZZ_SOURCE_DIR}")
target_compile_definitions(harfbuzz PRIVATE
  NDEBUG HB_TINY HB_EXTERN=
  "HB_CONFIG_OVERRIDE_H=\"${CMAKE_CURRENT_LIST_DIR}/config-override.h\"")
# Source options come after the shared toolchain's optimization flags.
set_source_files_properties("${HARFBUZZ_SOURCE_DIR}/harfbuzz.cc"
  PROPERTIES COMPILE_OPTIONS "$<IF:$<BOOL:${MSVC}>,/O2,-O3>")
target_link_libraries(harfbuzz PRIVATE defold_sdk)
set_target_properties(harfbuzz PROPERTIES PREFIX "lib")

if(DEFOLD_EXTERNAL_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${DEFOLD_EXTERNAL_INSTALL_PREFIX}" CACHE PATH "External package install prefix" FORCE)
endif()

install(TARGETS harfbuzz ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")
file(GLOB HARFBUZZ_HEADERS CONFIGURE_DEPENDS "${HARFBUZZ_SOURCE_DIR}/*.h" "${HARFBUZZ_SOURCE_DIR}/*.hh")
list(FILTER HARFBUZZ_HEADERS EXCLUDE REGEX "/config-override\\.h$")
install(FILES ${HARFBUZZ_HEADERS} DESTINATION include/harfbuzz)
install(FILES config-override.h DESTINATION include/harfbuzz)
