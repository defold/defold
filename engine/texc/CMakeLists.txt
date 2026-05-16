cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_texc LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="TEXC")

set(TEXC_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(TEXC_SRC_DIR "${TEXC_ROOT}/src")

set(TEXC_IS_DESKTOP OFF)
if(TARGET_PLATFORM MATCHES "macos|linux|win32$")
  set(TEXC_IS_DESKTOP ON)
endif()

if(TEXC_IS_DESKTOP)
  find_package(JNI REQUIRED)
  find_program(DEFOLD_PYTHON NAMES python3 python REQUIRED)

  set(TEXC_CORE_SOURCES
    "${TEXC_SRC_DIR}/texc.cpp"
    "${TEXC_SRC_DIR}/texc_buffer.cpp"
    "${TEXC_SRC_DIR}/texc_enc_astc.cpp"
    "${TEXC_SRC_DIR}/texc_enc_basis.cpp"
    "${TEXC_SRC_DIR}/texc_enc_default.cpp"
    "${TEXC_SRC_DIR}/texc_private.cpp")

  set(TEXC_JAVA_DIR "${CMAKE_CURRENT_BINARY_DIR}/java")
  set(TEXC_JNI_DIR "${CMAKE_CURRENT_BINARY_DIR}/jni")
  set(TEXC_GENERATED_JAVA "${TEXC_JAVA_DIR}/com/dynamo/bob/pipeline/Texc.java")
  set(TEXC_GENERATED_CPP "${TEXC_JNI_DIR}/Texc_jni.cpp")
  set(TEXC_GENERATED_H "${TEXC_JNI_DIR}/Texc_jni.h")

  add_custom_command(
    OUTPUT "${TEXC_GENERATED_JAVA}" "${TEXC_GENERATED_CPP}" "${TEXC_GENERATED_H}"
    COMMAND "${CMAKE_COMMAND}" -E env
            "DEFOLD_HOME=${DEFOLD_HOME}"
            "DYNAMO_HOME=${DEFOLD_SDK_ROOT}"
            "${DEFOLD_PYTHON}" "${DEFOLD_CMAKE_DIR}/gen_java_bindings.py"
            --gen-java "${DEFOLD_HOME}/engine/jni/scripts/gen_java.py"
            --header "${TEXC_SRC_DIR}/texc.h"
            --namespace dmTexc
            --package com.dynamo.bob.pipeline
            --java-outdir "${TEXC_JAVA_DIR}"
            --jni-outdir "${TEXC_JNI_DIR}"
            --include "${TEXC_SRC_DIR}"
            --include "${DEFOLD_DMSDK_INCLUDE_DIR}"
            --include "${DEFOLD_INCLUDE_DIR}"
            --include "${DEFOLD_EXT_INCLUDE_DIR}"
    DEPENDS
      "${DEFOLD_HOME}/engine/jni/scripts/gen_java.py"
      "${TEXC_SRC_DIR}/texc.h"
    COMMENT "Generating Java/JNI bindings for texc.h")

  defold_add_library(texc STATIC ${TEXC_CORE_SOURCES})
  target_include_directories(texc PUBLIC "${TEXC_SRC_DIR}")
  defold_target_link_libraries(texc "${TARGET_PLATFORM}" SCOPE PUBLIC dlib zip mbedtls basis_encoder basis_transcoder astcenc)

  add_library(texc_shared SHARED
    ${TEXC_CORE_SOURCES}
    "${TEXC_SRC_DIR}/texclibrary_jni.cpp"
    "${TEXC_GENERATED_CPP}")
  target_include_directories(texc_shared PRIVATE "${TEXC_SRC_DIR}" "${CMAKE_CURRENT_BINARY_DIR}" "${TEXC_JNI_DIR}" ${JNI_INCLUDE_DIRS})
  defold_target_link_libraries(texc_shared "${TARGET_PLATFORM}"
    dlib_noasan
    zip_noasan
    mbedtls_noasan
    profile_null_noasan
    jni_noasan
    basis_encoder_noasan
    basis_transcoder
    astcenc)
  defold_target_link_app(texc_shared "${TARGET_PLATFORM}" SCOPE PRIVATE)

  set(TEXC_JAR "${CMAKE_CURRENT_BINARY_DIR}/texturecompiler.jar")
  set(TEXC_CLASSES_DIR "${CMAKE_CURRENT_BINARY_DIR}/classes")
  add_custom_command(
    OUTPUT "${TEXC_JAR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${TEXC_CLASSES_DIR}"
    COMMAND "${DEFOLD_JAVAC_EXECUTABLE}" -g --release 8 -Xlint:-options
            -d "${TEXC_CLASSES_DIR}"
            "${TEXC_GENERATED_JAVA}"
            "${TEXC_SRC_DIR}/java/com/dynamo/bob/pipeline/TexcLibraryJni.java"
    COMMAND "${Java_JAR_EXECUTABLE}" cf "${TEXC_JAR}" -C "${TEXC_CLASSES_DIR}" .
    DEPENDS "${TEXC_GENERATED_JAVA}" "${TEXC_SRC_DIR}/java/com/dynamo/bob/pipeline/TexcLibraryJni.java"
    COMMENT "Packaging texturecompiler.jar")
  add_custom_target(texturecompiler_jar ALL DEPENDS "${TEXC_JAR}")

  install(TARGETS texc texc_shared
          ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
          LIBRARY DESTINATION lib/${TARGET_PLATFORM}
          RUNTIME DESTINATION lib/${TARGET_PLATFORM})
  install(FILES "${TEXC_SRC_DIR}/texc.h" DESTINATION include/texc)
  install(FILES "${TEXC_JAR}" DESTINATION share/java)

  if(BUILD_TESTS)
    add_subdirectory(src/test)
  endif()
elseif(BUILD_TESTS)
  add_custom_target(build_tests)
endif()
