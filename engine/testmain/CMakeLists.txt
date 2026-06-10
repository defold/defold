cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_testmain LANGUAGES ${DEFOLD_LANGUAGE_LIST})

set(TESTMAIN_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(TESTMAIN_SRC_DIR "${TESTMAIN_ROOT}/src")
set(TESTMAIN_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")

configure_file(
  "${TESTMAIN_SRC_DIR}/common/testmain.h"
  "${TESTMAIN_INCLUDE_DIR}/testmain/testmain.h"
  COPYONLY)

file(GLOB TESTMAIN_COMMON_SOURCES CONFIGURE_DEPENDS "${TESTMAIN_SRC_DIR}/common/*.cpp")
file(GLOB TESTMAIN_PLATFORM_SOURCES CONFIGURE_DEPENDS "${TESTMAIN_SRC_DIR}/${TARGET_PLATFORM_OS}/*.cpp")

defold_add_library(testmain STATIC ${TESTMAIN_COMMON_SOURCES} ${TESTMAIN_PLATFORM_SOURCES})
target_include_directories(testmain PUBLIC
  "${TESTMAIN_INCLUDE_DIR}"
  "${TESTMAIN_SRC_DIR}/common")

defold_install_targets(TARGETS testmain
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})

install(FILES "${TESTMAIN_SRC_DIR}/common/testmain.h" DESTINATION include/testmain)

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
