cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_shaderc LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="SHADERC")

set(SHADERC_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(SHADERC_SRC_DIR "${SHADERC_ROOT}/src")

set(SHADERC_IS_DESKTOP OFF)
if(TARGET_PLATFORM MATCHES "macos|linux|win32$")
  set(SHADERC_IS_DESKTOP ON)
endif()

if(SHADERC_IS_DESKTOP)
  find_package(JNI REQUIRED)
  find_program(DEFOLD_PYTHON NAMES python3 python REQUIRED)

  set(SHADERC_CORE_SOURCES
    "${SHADERC_SRC_DIR}/shaderc.cpp"
    "${SHADERC_SRC_DIR}/shaderc_spirv.cpp"
    "${SHADERC_SRC_DIR}/shaderc_spvc.cpp")

  set(SHADERC_DEFINES)
  set(SHADERC_EXTRA_INCLUDES)
  set(SHADERC_EXTRA_LIBS)
  if(TARGET_PLATFORM MATCHES "win32$")
    list(APPEND SHADERC_DEFINES DM_BINARY_HLSL_SUPPORTED)
    list(APPEND SHADERC_CORE_SOURCES "${SHADERC_SRC_DIR}/shaderc_hlsl.cpp")
    list(APPEND SHADERC_EXTRA_INCLUDES "${DEFOLD_EXT_INCLUDE_DIR}/directx")
    list(APPEND SHADERC_EXTRA_LIBS d3dcompiler.lib uuid.lib dxcompiler.lib D3D12.lib DXGI.lib)
  endif()

  set(SHADERC_JAVA_DIR "${CMAKE_CURRENT_BINARY_DIR}/java")
  set(SHADERC_JNI_DIR "${CMAKE_CURRENT_BINARY_DIR}/jni")
  set(SHADERC_GENERATED_JAVA "${SHADERC_JAVA_DIR}/com/dynamo/bob/pipeline/Shaderc.java")
  set(SHADERC_GENERATED_CPP "${SHADERC_JNI_DIR}/Shaderc_jni.cpp")
  set(SHADERC_GENERATED_H "${SHADERC_JNI_DIR}/Shaderc_jni.h")

  add_custom_command(
    OUTPUT "${SHADERC_GENERATED_JAVA}" "${SHADERC_GENERATED_CPP}" "${SHADERC_GENERATED_H}"
    COMMAND "${CMAKE_COMMAND}" -E env
            "DEFOLD_HOME=${DEFOLD_HOME}"
            "DYNAMO_HOME=${DEFOLD_SDK_ROOT}"
            "${DEFOLD_PYTHON}" "${DEFOLD_CMAKE_DIR}/gen_java_bindings.py"
            --gen-java "${DEFOLD_HOME}/engine/jni/scripts/gen_java.py"
            --header "${SHADERC_SRC_DIR}/shaderc.h"
            --namespace dmShaderc
            --package com.dynamo.bob.pipeline
            --java-outdir "${SHADERC_JAVA_DIR}"
            --jni-outdir "${SHADERC_JNI_DIR}"
            --include "${SHADERC_SRC_DIR}"
            --include "${DEFOLD_DMSDK_INCLUDE_DIR}"
            --include "${DEFOLD_INCLUDE_DIR}"
            --include "${DEFOLD_EXT_INCLUDE_DIR}"
    DEPENDS
      "${DEFOLD_HOME}/engine/jni/scripts/gen_java.py"
      "${SHADERC_SRC_DIR}/shaderc.h"
    COMMENT "Generating Java/JNI bindings for shaderc.h")

  defold_add_library(shaderc STATIC ${SHADERC_CORE_SOURCES})
  target_include_directories(shaderc PUBLIC "${SHADERC_SRC_DIR}" "${SHADERC_SRC_DIR}/shader" ${SHADERC_EXTRA_INCLUDES})
  target_compile_definitions(shaderc PRIVATE ${SHADERC_DEFINES})
  defold_target_link_libraries(shaderc "${TARGET_PLATFORM}" SCOPE PUBLIC
    dlib
    zip
    mbedtls
    profile
    spirv-cross-c
    spirv-cross-core
    spirv-cross-glsl
    spirv-cross-hlsl
    spirv-cross-msl
    spirv-cross-reflect
    ${SHADERC_EXTRA_LIBS})

  add_library(shaderc_shared SHARED
    ${SHADERC_CORE_SOURCES}
    "${SHADERC_SRC_DIR}/shaderc_jni.cpp"
    "${SHADERC_GENERATED_CPP}")
  target_include_directories(shaderc_shared PRIVATE "${SHADERC_SRC_DIR}" "${SHADERC_SRC_DIR}/shader" "${SHADERC_JNI_DIR}" ${SHADERC_EXTRA_INCLUDES} ${JNI_INCLUDE_DIRS})
  target_compile_definitions(shaderc_shared PRIVATE ${SHADERC_DEFINES})
  defold_target_link_libraries(shaderc_shared "${TARGET_PLATFORM}"
    dlib_noasan
    zip_noasan
    mbedtls_noasan
    profile_null_noasan
    jni_noasan
    spirv-cross-c
    spirv-cross-core
    spirv-cross-glsl
    spirv-cross-hlsl
    spirv-cross-msl
    spirv-cross-reflect
    ${SHADERC_EXTRA_LIBS})
  defold_target_link_app(shaderc_shared "${TARGET_PLATFORM}" SCOPE PRIVATE)

  set(SHADERC_JAR "${CMAKE_CURRENT_BINARY_DIR}/shaderc.jar")
  set(SHADERC_CLASSES_DIR "${CMAKE_CURRENT_BINARY_DIR}/classes")
  add_custom_command(
    OUTPUT "${SHADERC_JAR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADERC_CLASSES_DIR}"
    COMMAND "${DEFOLD_JAVAC_EXECUTABLE}" -g --release 8 -Xlint:-options
            -d "${SHADERC_CLASSES_DIR}"
            "${SHADERC_GENERATED_JAVA}"
            "${SHADERC_SRC_DIR}/java/com/dynamo/bob/pipeline/ShadercJni.java"
    COMMAND "${Java_JAR_EXECUTABLE}" cf "${SHADERC_JAR}" -C "${SHADERC_CLASSES_DIR}" .
    DEPENDS "${SHADERC_GENERATED_JAVA}" "${SHADERC_SRC_DIR}/java/com/dynamo/bob/pipeline/ShadercJni.java"
    COMMENT "Packaging shaderc.jar")
  add_custom_target(shaderc_jar ALL DEPENDS "${SHADERC_JAR}")

  add_subdirectory(src/standalone)

  defold_install_targets(TARGETS shaderc shaderc_shared
          ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
          LIBRARY DESTINATION lib/${TARGET_PLATFORM}
          RUNTIME DESTINATION lib/${TARGET_PLATFORM})
  defold_install_targets(TARGETS shaderc_standalone
          RUNTIME DESTINATION bin/${TARGET_PLATFORM})
  install(FILES "${SHADERC_JAR}" DESTINATION share/java)

  if(BUILD_TESTS)
    add_subdirectory(src/test)
  endif()
elseif(BUILD_TESTS)
  add_custom_target(build_tests)
endif()
