cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

set(FONT_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(FONT_ROOT "${FONT_TEST_ROOT}/../..")
set(FONT_SRC_DIR "${FONT_ROOT}/src")

function(font_configure_test target font_target)
  target_include_directories(${target} PRIVATE
    "${FONT_TEST_ROOT}"
    "${FONT_SRC_DIR}"
    "${FONT_ROOT}")
  defold_target_link_libraries(${target} "${TARGET_PLATFORM}"
    testmain
    dlib
    profile_null
    ${font_target})
  defold_target_link_socket(${target} "${TARGET_PLATFORM}" PRIVATE)
  defold_register_test_target(${target} ON "${FONT_ROOT}"
    STAGE_FILES
      "src/test/data" "src/test/data")
endfunction()

defold_add_executable(test_font "${FONT_TEST_ROOT}/test_font.cpp")
font_configure_test(test_font font)

defold_add_executable(test_font_skribidi "${FONT_TEST_ROOT}/test_font.cpp")
target_compile_definitions(test_font_skribidi PRIVATE FONT_USE_HARFBUZZ FONT_USE_SKRIBIDI)
font_configure_test(test_font_skribidi font_skribidi)

if(FONT_RENDERER_IS_DESKTOP)
  set(FONT_RENDERER_TEST_JAVA "${FONT_TEST_ROOT}/java/com/dynamo/bob/font/FontRendererTest.java")
  set(FONT_RENDERER_TEST_CLASSES_DIR "${CMAKE_CURRENT_BINARY_DIR}/font-renderer-test-classes")
  set(FONT_RENDERER_TEST_JAR "${CMAKE_CURRENT_BINARY_DIR}/font-renderer-tests.jar")
  set(FONT_RENDERER_TEST_JUNIT "${DEFOLD_SDK_ROOT}/ext/share/java/junit-4.6.jar")
  if(TARGET_PLATFORM MATCHES "win32$")
    set(FONT_RENDERER_TEST_CLASSPATH_SEPARATOR "\\;")
  else()
    set(FONT_RENDERER_TEST_CLASSPATH_SEPARATOR ":")
  endif()
  set(FONT_RENDERER_TEST_CLASSPATH
    "${FONT_RENDERER_TEST_JAR}${FONT_RENDERER_TEST_CLASSPATH_SEPARATOR}${FONT_RENDERER_JAR}${FONT_RENDERER_TEST_CLASSPATH_SEPARATOR}${FONT_RENDERER_TEST_JUNIT}")
  add_custom_command(
    OUTPUT "${FONT_RENDERER_TEST_JAR}"
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${FONT_RENDERER_TEST_CLASSES_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${FONT_RENDERER_TEST_CLASSES_DIR}"
    COMMAND "${DEFOLD_JAVAC_EXECUTABLE}" -g --release 25 -Xlint:-options
            -classpath "${FONT_RENDERER_JAR}${FONT_RENDERER_TEST_CLASSPATH_SEPARATOR}${FONT_RENDERER_TEST_JUNIT}"
            -d "${FONT_RENDERER_TEST_CLASSES_DIR}" "${FONT_RENDERER_TEST_JAVA}"
    COMMAND "${Java_JAR_EXECUTABLE}" cfe "${FONT_RENDERER_TEST_JAR}"
            com.dynamo.bob.font.FontRendererTest
            -C "${FONT_RENDERER_TEST_CLASSES_DIR}" .
            -C "${FONT_TEST_ROOT}/data" NotoSans-Regular.ttf
    DEPENDS font_renderer_jar
            "${FONT_RENDERER_TEST_JAVA}"
            "${FONT_TEST_ROOT}/data/NotoSans-Regular.ttf"
    COMMENT "Packaging font-renderer-tests.jar")
  add_custom_target(test_font_renderer_java_jar DEPENDS "${FONT_RENDERER_TEST_JAR}")
  add_dependencies(test_font_renderer_java_jar fontc_shared font_renderer_jar)
  add_dependencies(build_tests test_font_renderer_java_jar)
  add_custom_target(run_test_font_renderer_java
    COMMAND "${Java_JAVA_EXECUTABLE}"
            --enable-native-access=ALL-UNNAMED
            "-Djava.library.path=$<TARGET_FILE_DIR:fontc_shared>"
            -classpath "${FONT_RENDERER_TEST_CLASSPATH}"
            com.dynamo.bob.font.FontRendererTest
    DEPENDS test_font_renderer_java_jar fontc_shared
    WORKING_DIRECTORY "${FONT_ROOT}"
    USES_TERMINAL
    COMMENT "Running font-renderer Java tests")
  add_dependencies(run_tests run_test_font_renderer_java)
endif()

if(TARGET_PLATFORM MATCHES "^(arm64-macos|x86_64-macos|x86_64-linux|x86_64-win32)$")
  set(FONTVIEWER_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/fontviewer_generated")
  set(FONTVIEWER_ENGLISH_FONT_HEADER "${FONTVIEWER_GENERATED_DIR}/NotoSans-Regular.ttf.embed.h")
  set(FONTVIEWER_ENGLISH_FONT_CPP "${FONTVIEWER_GENERATED_DIR}/NotoSans-Regular.ttf.embed.cpp")
  set(FONTVIEWER_ARABIC_FONT_HEADER "${FONTVIEWER_GENERATED_DIR}/NotoSansArabic-Regular.ttf.embed.h")
  set(FONTVIEWER_ARABIC_FONT_CPP "${FONTVIEWER_GENERATED_DIR}/NotoSansArabic-Regular.ttf.embed.cpp")
  set(FONTVIEWER_VERTEX_SHADER_HEADER "${FONTVIEWER_GENERATED_DIR}/font-df.vp.embed.h")
  set(FONTVIEWER_VERTEX_SHADER_CPP "${FONTVIEWER_GENERATED_DIR}/font-df.vp.embed.cpp")
  set(FONTVIEWER_FRAGMENT_SHADER_HEADER "${FONTVIEWER_GENERATED_DIR}/font-df.fp.embed.h")
  set(FONTVIEWER_FRAGMENT_SHADER_CPP "${FONTVIEWER_GENERATED_DIR}/font-df.fp.embed.cpp")
  defold_embed_to_header(
    INPUT "${FONT_TEST_ROOT}/data/NotoSans-Regular.ttf"
    OUTPUT_HEADER "${FONTVIEWER_ENGLISH_FONT_HEADER}"
    OUTPUT_CPP "${FONTVIEWER_ENGLISH_FONT_CPP}"
    SYMBOL FONTVIEWER_NOTO_SANS)
  defold_embed_to_header(
    INPUT "${FONT_TEST_ROOT}/data/NotoSansArabic-Regular.ttf"
    OUTPUT_HEADER "${FONTVIEWER_ARABIC_FONT_HEADER}"
    OUTPUT_CPP "${FONTVIEWER_ARABIC_FONT_CPP}"
    SYMBOL FONTVIEWER_NOTO_SANS_ARABIC)
  defold_embed_to_header(
    INPUT "${FONT_ROOT}/content/builtins/fonts/font-df.vp"
    OUTPUT_HEADER "${FONTVIEWER_VERTEX_SHADER_HEADER}"
    OUTPUT_CPP "${FONTVIEWER_VERTEX_SHADER_CPP}"
    SYMBOL FONTVIEWER_FONT_DF_VP)
  defold_embed_to_header(
    INPUT "${FONT_ROOT}/content/builtins/fonts/font-df.fp"
    OUTPUT_HEADER "${FONTVIEWER_FRAGMENT_SHADER_HEADER}"
    OUTPUT_CPP "${FONTVIEWER_FRAGMENT_SHADER_CPP}"
    SYMBOL FONTVIEWER_FONT_DF_FP)
  defold_create_exported_symbols_file(
    "${FONTVIEWER_GENERATED_DIR}/__exported_symbols_fontviewer.cpp"
    "GraphicsAdapterOpenGL")
  defold_add_executable(fontviewer
    "${FONT_TEST_ROOT}/fontviewer/fontviewer.cpp"
    "${FONT_TEST_ROOT}/fontviewer/fontviewer_nuklear.cpp"
    "${FONT_TEST_ROOT}/fontviewer/fontviewer_macos.cpp"
    "${FONTVIEWER_GENERATED_DIR}/__exported_symbols_fontviewer.cpp"
    "${FONTVIEWER_ENGLISH_FONT_CPP}"
    "${FONTVIEWER_ARABIC_FONT_CPP}"
    "${FONTVIEWER_VERTEX_SHADER_CPP}"
    "${FONTVIEWER_FRAGMENT_SHADER_CPP}")
  set_target_properties(fontviewer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
  target_include_directories(fontviewer PRIVATE
    "${FONT_SRC_DIR}"
    "${FONT_RENDERER_SRC_DIR}"
    "${FONT_ROOT}"
    "${FONTVIEWER_GENERATED_DIR}"
    "${GRAPHICS_SRC_DIR}"
    "${GRAPHICS_PROTO_ROOT}")
  target_compile_definitions(fontviewer PRIVATE FONT_USE_HARFBUZZ FONT_USE_SKRIBIDI)
  defold_target_link_libraries(fontviewer "${TARGET_PLATFORM}"
    testmain
    ddf
    dlib
    profile_null
    font_skribidi)
  set(WITH_OPENGL ON)
  defold_target_link_graphics(fontviewer "${TARGET_PLATFORM}" SCOPE PRIVATE)
  defold_target_link_platform(fontviewer "${TARGET_PLATFORM}" SCOPE PRIVATE)
  defold_target_link_socket(fontviewer "${TARGET_PLATFORM}" PRIVATE)
  if(TARGET_PLATFORM MATCHES "^(arm64-macos|x86_64-macos)$")
    target_link_libraries(fontviewer PRIVATE "-framework IOKit")
  endif()
  if(TARGET_PLATFORM MATCHES "x86_64-linux")
    find_package(Threads REQUIRED)
    target_link_libraries(fontviewer PRIVATE Threads::Threads dl)
  endif()
endif()
