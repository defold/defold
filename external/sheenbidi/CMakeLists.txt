cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(sheenbidi_external VERSION 2.9.0 LANGUAGES C)

set(SHEENBIDI_ROOT "${CMAKE_CURRENT_LIST_DIR}/package/SheenBidi-2.9.0")

add_library(sheenbidi STATIC "${SHEENBIDI_ROOT}/Source/SheenBidi.c")
target_include_directories(sheenbidi PRIVATE "${SHEENBIDI_ROOT}/Headers")
target_compile_definitions(sheenbidi PRIVATE NDEBUG SB_CONFIG_UNITY)
# Source options come after the shared toolchain's optimization flags.
set_source_files_properties("${SHEENBIDI_ROOT}/Source/SheenBidi.c"
  PROPERTIES COMPILE_OPTIONS "$<IF:$<BOOL:${MSVC}>,/O2,-O3>")
target_link_libraries(sheenbidi PRIVATE defold_sdk)
set_target_properties(sheenbidi PROPERTIES PREFIX "lib")

if(DEFOLD_EXTERNAL_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${DEFOLD_EXTERNAL_INSTALL_PREFIX}" CACHE PATH "External package install prefix" FORCE)
endif()

install(TARGETS sheenbidi ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")
install(DIRECTORY "${SHEENBIDI_ROOT}/Headers/" DESTINATION include
  FILES_MATCHING PATTERN "*.h")
