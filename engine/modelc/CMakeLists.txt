cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_modelc LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="MODELC")

set(MODELC_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(MODELC_SRC_DIR "${MODELC_ROOT}/src")

set(MODELC_IS_DESKTOP OFF)
if(TARGET_PLATFORM MATCHES "macos|linux|win32$")
  set(MODELC_IS_DESKTOP ON)
endif()

if(MODELC_IS_DESKTOP)
  find_package(JNI REQUIRED)
  find_program(DEFOLD_PYTHON NAMES python3 python REQUIRED)

  set(MODELC_CORE_SOURCES
    "${MODELC_SRC_DIR}/modelimporter.cpp"
    "${MODELC_SRC_DIR}/modelimporter_debug.cpp"
    "${MODELC_SRC_DIR}/modelimporter_gltf.cpp")

  set(MODELC_JAVA_DIR "${CMAKE_CURRENT_BINARY_DIR}/java")
  set(MODELC_JNI_DIR "${CMAKE_CURRENT_BINARY_DIR}/jni")
  set(MODELC_GENERATED_JAVA "${MODELC_JAVA_DIR}/com/dynamo/bob/pipeline/Modelimporter.java")
  set(MODELC_GENERATED_CPP "${MODELC_JNI_DIR}/Modelimporter_jni.cpp")
  set(MODELC_GENERATED_H "${MODELC_JNI_DIR}/Modelimporter_jni.h")

  add_custom_command(
    OUTPUT "${MODELC_GENERATED_JAVA}" "${MODELC_GENERATED_CPP}" "${MODELC_GENERATED_H}"
    COMMAND "${CMAKE_COMMAND}" -E env
            "DEFOLD_HOME=${DEFOLD_HOME}"
            "DYNAMO_HOME=${DEFOLD_SDK_ROOT}"
            "${DEFOLD_PYTHON}" "${DEFOLD_CMAKE_DIR}/gen_java_bindings.py"
            --gen-java "${DEFOLD_HOME}/engine/jni/scripts/gen_java.py"
            --header "${MODELC_SRC_DIR}/modelimporter.h"
            --namespace dmModelImporter
            --package com.dynamo.bob.pipeline
            --java-outdir "${MODELC_JAVA_DIR}"
            --jni-outdir "${MODELC_JNI_DIR}"
            --include "${MODELC_SRC_DIR}"
            --include "${DEFOLD_DMSDK_INCLUDE_DIR}"
            --include "${DEFOLD_INCLUDE_DIR}"
            --include "${DEFOLD_EXT_INCLUDE_DIR}"
    DEPENDS
      "${DEFOLD_HOME}/engine/jni/scripts/gen_java.py"
      "${MODELC_SRC_DIR}/modelimporter.h"
    COMMENT "Generating Java/JNI bindings for modelimporter.h")

  defold_add_library(model STATIC ${MODELC_CORE_SOURCES})
  target_include_directories(model PUBLIC "${MODELC_SRC_DIR}" "${MODELC_SRC_DIR}/model")
  defold_target_link_libraries(model "${TARGET_PLATFORM}" SCOPE PUBLIC dlib zip mbedtls profile)

  add_library(modelc_shared SHARED
    ${MODELC_CORE_SOURCES}
    "${MODELC_SRC_DIR}/modelimporter_jni.cpp"
    "${MODELC_GENERATED_CPP}")
  target_include_directories(modelc_shared PRIVATE "${MODELC_SRC_DIR}" "${MODELC_SRC_DIR}/model" "${MODELC_JNI_DIR}" ${JNI_INCLUDE_DIRS})
  defold_target_link_libraries(modelc_shared "${TARGET_PLATFORM}"
    dlib_noasan
    zip_noasan
    mbedtls_noasan
    profile_null_noasan
    jni_noasan)
  defold_target_link_app(modelc_shared "${TARGET_PLATFORM}" SCOPE PRIVATE)

  set(MODELC_JAR "${CMAKE_CURRENT_BINARY_DIR}/modelimporter.jar")
  set(MODELC_CLASSES_DIR "${CMAKE_CURRENT_BINARY_DIR}/classes")
  add_custom_command(
    OUTPUT "${MODELC_JAR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${MODELC_CLASSES_DIR}"
    COMMAND "${DEFOLD_JAVAC_EXECUTABLE}" -g --release 8 -Xlint:-options
            -d "${MODELC_CLASSES_DIR}"
            "${MODELC_GENERATED_JAVA}"
            "${MODELC_SRC_DIR}/java/com/dynamo/bob/pipeline/ModelImporterJni.java"
    COMMAND "${Java_JAR_EXECUTABLE}" cf "${MODELC_JAR}" -C "${MODELC_CLASSES_DIR}" .
    DEPENDS "${MODELC_GENERATED_JAVA}" "${MODELC_SRC_DIR}/java/com/dynamo/bob/pipeline/ModelImporterJni.java"
    COMMENT "Packaging modelimporter.jar")
  add_custom_target(modelimporter_jar ALL DEPENDS "${MODELC_JAR}")

  defold_install_targets(TARGETS model modelc_shared
          ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
          LIBRARY DESTINATION lib/${TARGET_PLATFORM}
          RUNTIME DESTINATION lib/${TARGET_PLATFORM})
  install(FILES "${MODELC_JAR}" DESTINATION share/java)

  if(BUILD_TESTS)
    add_subdirectory(src/test)
  endif()
elseif(BUILD_TESTS)
  add_custom_target(build_tests)
endif()
