cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_font LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="FONT")

set(FONT_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(FONT_SRC_DIR "${FONT_ROOT}/src")
set(FONT_RENDERER_SRC_DIR "${FONT_SRC_DIR}/renderer")
set(FONT_PUBLIC_HEADERS
  "${FONT_SRC_DIR}/font.h"
  "${FONT_SRC_DIR}/fontcollection.h"
  "${FONT_SRC_DIR}/text_layout.h")
set(FONT_RENDERER_PUBLIC_HEADERS
  "${FONT_RENDERER_SRC_DIR}/font_renderer.h"
  "${FONT_RENDERER_SRC_DIR}/glyph_gen.h"
  "${FONT_RENDERER_SRC_DIR}/glyph_vertex.h")
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
list(APPEND FONT_SOURCES
  "${FONT_RENDERER_SRC_DIR}/glyph_gen.cpp"
  "${FONT_RENDERER_SRC_DIR}/glyph_vertex.cpp")

defold_add_library(font STATIC ${FONT_SOURCES})
target_include_directories(font PUBLIC "${FONT_SRC_DIR}" "${FONT_RENDERER_SRC_DIR}" "${FONT_ROOT}")
target_compile_definitions(font PRIVATE "DLIB_LOG_DOMAIN=\"FONT\"")
defold_target_link_libraries(font "${TARGET_PLATFORM}" SCOPE PUBLIC ${FONT_DEPENDENCIES})

defold_add_library(font_skribidi STATIC ${FONT_SOURCES})
target_include_directories(font_skribidi PUBLIC "${FONT_SRC_DIR}" "${FONT_RENDERER_SRC_DIR}" "${FONT_ROOT}")
target_compile_definitions(font_skribidi PRIVATE
  "DLIB_LOG_DOMAIN=\"FONT\""
  FONT_USE_HARFBUZZ
  FONT_USE_SKRIBIDI)
defold_target_link_libraries(font_skribidi "${TARGET_PLATFORM}" SCOPE PUBLIC ${FONT_SKRIBIDI_DEPENDENCIES})
set_target_properties(font_skribidi PROPERTIES DEFOLD_SDK_HEADERS_TARGET font_sdk_headers)

set(FONT_RENDERER_IS_DESKTOP OFF)
if(TARGET_PLATFORM MATCHES "macos|linux|win32$")
  set(FONT_RENDERER_IS_DESKTOP ON)
endif()

if(FONT_RENDERER_IS_DESKTOP)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)

  add_library(fontc_shared SHARED "${FONT_RENDERER_SRC_DIR}/font_renderer.cpp")
  defold_target_skip_sanitizer(fontc_shared)
  target_include_directories(fontc_shared PRIVATE "${FONT_SRC_DIR}" "${FONT_RENDERER_SRC_DIR}")
  target_compile_definitions(fontc_shared PRIVATE FONT_USE_HARFBUZZ FONT_USE_SKRIBIDI)
  defold_target_link_libraries(fontc_shared "${TARGET_PLATFORM}" font_skribidi profile_null)
  defold_codesign_target(fontc_shared "../engine/darwin/entitlements-macos-debug.plist")

  set(FONT_RENDERER_JAR "${CMAKE_CURRENT_BINARY_DIR}/fontrenderer.jar")
  set(FONT_RENDERER_CLASSES_DIR "${CMAKE_CURRENT_BINARY_DIR}/font-renderer-classes")
  set(FONT_RENDERER_JAVA_ROOT "${FONT_SRC_DIR}/java")
  set(FONT_RENDERER_GENERATED_JAVA_ROOT "${FONT_RENDERER_JAVA_ROOT}")
  set(FONT_RENDERER_GENERATED_JAVA_PACKAGE "${FONT_RENDERER_GENERATED_JAVA_ROOT}/com/dynamo/bob/font/generated")
  set(FONT_RENDERER_BINDING_GENERATOR "${FONT_ROOT}/scripts/generate_font_renderer_bindings.py")
  set(DEFOLD_JEXTRACT_EXECUTABLE "jextract" CACHE FILEPATH "Java 25 jextract executable")
  set(FONT_RENDERER_JAVA
    "${FONT_RENDERER_JAVA_ROOT}/com/dynamo/bob/font/BMFont.java"
    "${FONT_RENDERER_JAVA_ROOT}/com/dynamo/bob/font/FontRenderer.java"
    "${FONT_RENDERER_GENERATED_JAVA_PACKAGE}/FontRendererFFM.java"
    "${FONT_RENDERER_GENERATED_JAVA_PACKAGE}/FontRendererSymbols.java"
    "${FONT_RENDERER_GENERATED_JAVA_PACKAGE}/FontRendererGlyph.java"
    "${FONT_RENDERER_GENERATED_JAVA_PACKAGE}/FontRendererLayout.java"
    "${FONT_RENDERER_GENERATED_JAVA_PACKAGE}/FontRendererParams.java"
    "${FONT_RENDERER_GENERATED_JAVA_PACKAGE}/FontRendererProperties.java"
    "${FONT_RENDERER_GENERATED_JAVA_PACKAGE}/FontTexture.java")
  add_custom_target(generate_font_renderer_ffm
    COMMAND "${Python3_EXECUTABLE}" "${FONT_RENDERER_BINDING_GENERATOR}"
            --jextract "${DEFOLD_JEXTRACT_EXECUTABLE}"
            --header "${FONT_RENDERER_SRC_DIR}/font_renderer.h"
            --include-dir "${FONT_ROOT}/../dlib/src"
            --output "${FONT_RENDERER_GENERATED_JAVA_ROOT}"
    DEPENDS "${FONT_RENDERER_SRC_DIR}/font_renderer.h" "${FONT_RENDERER_BINDING_GENERATOR}"
    COMMENT "Generating Java 25 FFM bindings for font_renderer.h")
  add_custom_command(
    OUTPUT "${FONT_RENDERER_JAR}"
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${FONT_RENDERER_CLASSES_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${FONT_RENDERER_CLASSES_DIR}"
    COMMAND "${DEFOLD_JAVAC_EXECUTABLE}" -g --release 25 -Xlint:-options
            -d "${FONT_RENDERER_CLASSES_DIR}" ${FONT_RENDERER_JAVA}
    COMMAND "${Java_JAR_EXECUTABLE}" cf "${FONT_RENDERER_JAR}" -C "${FONT_RENDERER_CLASSES_DIR}" .
    DEPENDS "${FONT_RENDERER_JAVA}"
    COMMENT "Packaging fontrenderer.jar")
  add_custom_target(font_renderer_jar ALL DEPENDS "${FONT_RENDERER_JAR}")

  defold_install_targets(TARGETS fontc_shared
          LIBRARY DESTINATION lib/${TARGET_PLATFORM}
          RUNTIME DESTINATION lib/${TARGET_PLATFORM})
  install(FILES "${FONT_RENDERER_JAR}" DESTINATION share/java)
endif()

defold_install_targets(TARGETS font font_skribidi
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})

defold_add_sdk_headers_target(font_public_sdk_headers
  DESTINATION include/font
  SOURCE_ROOT "${FONT_SRC_DIR}"
  FILES ${FONT_PUBLIC_HEADERS}
  DEPENDS font)
defold_add_sdk_headers_target(font_renderer_sdk_headers
  DESTINATION include/font
  SOURCE_ROOT "${FONT_RENDERER_SRC_DIR}"
  FILES ${FONT_RENDERER_PUBLIC_HEADERS}
  DEPENDS font)
defold_add_sdk_headers_target(font_dmsdk_sdk_headers
  DESTINATION sdk/include/dmsdk
  SOURCE_ROOT "${FONT_SRC_DIR}/dmsdk"
  FILES ${FONT_DMSDK_HEADERS}
  DEPENDS font)
set(FONT_SDK_HEADER_TARGETS
  font_public_sdk_headers
  font_renderer_sdk_headers
  font_dmsdk_sdk_headers
  dlib_sdk_headers)
add_custom_target(font_sdk_headers
  DEPENDS ${FONT_SDK_HEADER_TARGETS})

install(FILES
  "${FONT_SRC_DIR}/font.h"
  "${FONT_SRC_DIR}/fontcollection.h"
  "${FONT_SRC_DIR}/text_layout.h"
  DESTINATION include/font)
install(FILES ${FONT_RENDERER_PUBLIC_HEADERS} DESTINATION include/font)
install(DIRECTORY "${FONT_SRC_DIR}/dmsdk/" DESTINATION sdk/include/dmsdk FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
