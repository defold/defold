cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_texc LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="TEXC")

set(TEXC_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(TEXC_SRC_DIR "${TEXC_ROOT}/src")
set(TEXC_DEPENDENCIES dlib zip mbedtls basis_encoder astcenc)
set(TEXC_BINDINGS_DEPENDENCIES jni_noasan)
set(TEXC_SHARED_DEPENDENCIES
  dlib_noasan
  zip_noasan
  mbedtls_noasan
  profile_null_noasan
  jni_noasan
  basis_encoder_noasan
  astcenc)

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
  add_custom_target(texc_generated_bindings
    DEPENDS "${TEXC_GENERATED_JAVA}" "${TEXC_GENERATED_CPP}" "${TEXC_GENERATED_H}")

  defold_add_library(texc STATIC ${TEXC_CORE_SOURCES})
  target_include_directories(texc PUBLIC "${TEXC_SRC_DIR}")
  defold_target_link_libraries(texc "${TARGET_PLATFORM}" SCOPE PUBLIC ${TEXC_DEPENDENCIES})

  add_library(texc_bindings OBJECT "${TEXC_GENERATED_CPP}")
  defold_target_skip_sanitizer(texc_bindings)
  target_include_directories(texc_bindings PRIVATE "${TEXC_SRC_DIR}" "${CMAKE_CURRENT_BINARY_DIR}" "${TEXC_JNI_DIR}" ${JNI_INCLUDE_DIRS})
  defold_target_link_libraries(texc_bindings "${TARGET_PLATFORM}" SCOPE PRIVATE ${TEXC_BINDINGS_DEPENDENCIES})
  set_target_properties(texc_bindings PROPERTIES POSITION_INDEPENDENT_CODE ON)
  add_dependencies(texc_bindings texc_generated_bindings)

  add_library(texc_shared SHARED
    ${TEXC_CORE_SOURCES}
    "${TEXC_SRC_DIR}/texclibrary_jni.cpp"
    $<TARGET_OBJECTS:texc_bindings>)
  defold_target_skip_sanitizer(texc_shared)
  target_include_directories(texc_shared PRIVATE "${TEXC_SRC_DIR}" "${CMAKE_CURRENT_BINARY_DIR}" "${TEXC_JNI_DIR}" ${JNI_INCLUDE_DIRS})
  add_dependencies(texc_shared texc_bindings)
  defold_target_link_libraries(texc_shared "${TARGET_PLATFORM}"
    ${TEXC_SHARED_DEPENDENCIES})
  defold_target_link_app(texc_shared "${TARGET_PLATFORM}" SCOPE PRIVATE)
  if(NOT DEFOLD_SKIP_CODESIGN AND TARGET_PLATFORM MATCHES "macos|win32$")
    set(TEXC_CODESIGN_ARGS
      --platform "${TARGET_PLATFORM}"
      --file "$<TARGET_FILE:texc_shared>")
    if(DEFOLD_CODESIGNING_IDENTITY)
      list(APPEND TEXC_CODESIGN_ARGS --codesigning-identity "${DEFOLD_CODESIGNING_IDENTITY}")
    endif()
    if(DEFOLD_GCLOUD_PROJECTID)
      list(APPEND TEXC_CODESIGN_ARGS --gcloud-projectid "${DEFOLD_GCLOUD_PROJECTID}")
    endif()
    if(DEFOLD_GCLOUD_LOCATION)
      list(APPEND TEXC_CODESIGN_ARGS --gcloud-location "${DEFOLD_GCLOUD_LOCATION}")
    endif()
    if(DEFOLD_GCLOUD_KEYRINGNAME)
      list(APPEND TEXC_CODESIGN_ARGS --gcloud-keyringname "${DEFOLD_GCLOUD_KEYRINGNAME}")
    endif()
    if(DEFOLD_GCLOUD_KEYNAME)
      list(APPEND TEXC_CODESIGN_ARGS --gcloud-keyname "${DEFOLD_GCLOUD_KEYNAME}")
    endif()
    if(DEFOLD_GCLOUD_CERTFILE)
      list(APPEND TEXC_CODESIGN_ARGS --gcloud-certfile "${DEFOLD_GCLOUD_CERTFILE}")
    endif()
    if(DEFOLD_GCLOUD_KEYFILE)
      list(APPEND TEXC_CODESIGN_ARGS --gcloud-keyfile "${DEFOLD_GCLOUD_KEYFILE}")
    endif()
    add_custom_command(TARGET texc_shared POST_BUILD
      COMMAND "${DEFOLD_PYTHON}" "${DEFOLD_HOME}/build_tools/codesigning.py" ${TEXC_CODESIGN_ARGS}
      COMMENT "Signing texc_shared")
  endif()

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
    DEPENDS
      texc_generated_bindings
      texc_bindings
      "${TEXC_GENERATED_JAVA}"
      "${TEXC_GENERATED_CPP}"
      "${TEXC_GENERATED_H}"
      "${TEXC_SRC_DIR}/java/com/dynamo/bob/pipeline/TexcLibraryJni.java"
    COMMENT "Packaging texturecompiler.jar")
  add_custom_target(texturecompiler_jar ALL DEPENDS "${TEXC_JAR}")
  add_dependencies(texturecompiler_jar texc_generated_bindings texc_bindings)

  defold_install_targets(TARGETS texc texc_shared
          ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
          LIBRARY DESTINATION lib/${TARGET_PLATFORM}
          RUNTIME DESTINATION lib/${TARGET_PLATFORM})
  defold_add_sdk_headers_target(texc_sdk_headers
    DESTINATION include/texc
    FILES "${TEXC_SRC_DIR}/texc.h"
    DEPENDS texc dlib_sdk_headers)
  install(FILES "${TEXC_SRC_DIR}/texc.h" DESTINATION include/texc)
  install(FILES "${TEXC_JAR}" DESTINATION share/java)

  if(BUILD_TESTS)
    add_subdirectory(src/test)
  endif()
elseif(BUILD_TESTS)
  add_custom_target(build_tests)
endif()
