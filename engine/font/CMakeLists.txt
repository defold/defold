cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_font LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="FONT")

set(FONT_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(FONT_SRC_DIR "${FONT_ROOT}/src")
set(FONT_PUBLIC_HEADERS
  "${FONT_SRC_DIR}/font.h"
  "${FONT_SRC_DIR}/fontcollection.h"
  "${FONT_SRC_DIR}/text_layout.h")
set(FONT_DEPENDENCIES dlib)
set(FONT_SKRIBIDI_DEPENDENCIES
  dlib
  harfbuzz
  sheenbidi
  unibreak
  skribidi)
file(GLOB_RECURSE FONT_DMSDK_HEADERS CONFIGURE_DEPENDS
  "${FONT_SRC_DIR}/dmsdk/*.h"
  "${FONT_SRC_DIR}/dmsdk/*.hpp")

file(GLOB FONT_SOURCES CONFIGURE_DEPENDS "${FONT_SRC_DIR}/*.cpp")
list(FILTER FONT_SOURCES EXCLUDE REGEX "/font_(darwin|win32|null)\\.(cpp|m|mm)$")
if(TARGET_PLATFORM MATCHES "macos$")
  list(APPEND FONT_SOURCES "${FONT_SRC_DIR}/font_darwin.mm")
else()
  list(APPEND FONT_SOURCES "${FONT_SRC_DIR}/font_null.cpp")
endif()

defold_add_library(font STATIC ${FONT_SOURCES})
target_include_directories(font PUBLIC "${FONT_SRC_DIR}" "${FONT_ROOT}")
target_compile_definitions(font PRIVATE "DLIB_LOG_DOMAIN=\"FONT\"")
defold_target_link_libraries(font "${TARGET_PLATFORM}" SCOPE PUBLIC ${FONT_DEPENDENCIES})
if(TARGET_PLATFORM MATCHES "macos$")
  target_link_libraries(font PUBLIC "-framework CoreText")
endif()

defold_add_library(font_skribidi STATIC ${FONT_SOURCES})
target_include_directories(font_skribidi PUBLIC "${FONT_SRC_DIR}" "${FONT_ROOT}")
target_compile_definitions(font_skribidi PRIVATE
  "DLIB_LOG_DOMAIN=\"FONT\""
  FONT_USE_HARFBUZZ
  FONT_USE_SKRIBIDI)
defold_target_link_libraries(font_skribidi "${TARGET_PLATFORM}" SCOPE PUBLIC ${FONT_SKRIBIDI_DEPENDENCIES})
if(TARGET_PLATFORM MATCHES "macos$")
  target_link_libraries(font_skribidi PUBLIC "-framework CoreText")
endif()
set_target_properties(font_skribidi PROPERTIES DEFOLD_SDK_HEADERS_TARGET font_sdk_headers)

defold_install_targets(TARGETS font font_skribidi
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})

defold_add_sdk_headers_target(font_public_sdk_headers
  DESTINATION include/font
  SOURCE_ROOT "${FONT_SRC_DIR}"
  FILES ${FONT_PUBLIC_HEADERS}
  DEPENDS font)
defold_add_sdk_headers_target(font_dmsdk_sdk_headers
  DESTINATION sdk/include/dmsdk
  SOURCE_ROOT "${FONT_SRC_DIR}/dmsdk"
  FILES ${FONT_DMSDK_HEADERS}
  DEPENDS font)
set(FONT_SDK_HEADER_TARGETS
  font_public_sdk_headers
  font_dmsdk_sdk_headers
  dlib_sdk_headers)
add_custom_target(font_sdk_headers
  DEPENDS ${FONT_SDK_HEADER_TARGETS})

install(FILES
  "${FONT_SRC_DIR}/font.h"
  "${FONT_SRC_DIR}/fontcollection.h"
  "${FONT_SRC_DIR}/text_layout.h"
  DESTINATION include/font)
install(DIRECTORY "${FONT_SRC_DIR}/dmsdk/" DESTINATION sdk/include/dmsdk FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
