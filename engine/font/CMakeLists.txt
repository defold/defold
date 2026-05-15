cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_font LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="FONT")

set(FONT_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(FONT_SRC_DIR "${FONT_ROOT}/src")

file(GLOB FONT_SOURCES CONFIGURE_DEPENDS "${FONT_SRC_DIR}/*.cpp")

defold_add_library(font STATIC ${FONT_SOURCES})
target_include_directories(font PUBLIC "${FONT_SRC_DIR}" "${FONT_ROOT}")
target_compile_definitions(font PRIVATE "DLIB_LOG_DOMAIN=\"FONT\"")

defold_add_library(font_skribidi STATIC ${FONT_SOURCES})
target_include_directories(font_skribidi PUBLIC "${FONT_SRC_DIR}" "${FONT_ROOT}")
target_compile_definitions(font_skribidi PRIVATE
  "DLIB_LOG_DOMAIN=\"FONT\""
  FONT_USE_HARFBUZZ
  FONT_USE_SKRIBIDI)
defold_target_link_libraries(font_skribidi "${TARGET_PLATFORM}"
  harfbuzz
  sheenbidi
  unibreak
  skribidi)

install(TARGETS font font_skribidi
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})

install(FILES
  "${FONT_SRC_DIR}/font.h"
  "${FONT_SRC_DIR}/fontcollection.h"
  "${FONT_SRC_DIR}/text_layout.h"
  DESTINATION include/font)

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
