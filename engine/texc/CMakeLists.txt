cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)
include(simd)

project(defold_texc LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="TEXC")

set(TEXC_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(TEXC_SRC_DIR "${TEXC_ROOT}/src")
set(TEXC_DEPENDENCIES dlib zip mbedtls basis_encoder astcenc)
set(TEXC_BINDINGS_DEPENDENCIES jni_noasan)
set(TEXC_SHARED_DEPENDENCIES
  dlib_noasan
  image_noasan
  zip_noasan
  mbedtls_noasan
  profile_null_noasan
  jni_noasan
  basis_encoder
  astcenc)

set(TEXC_IS_DESKTOP OFF)
if(TARGET_PLATFORM MATCHES "macos|linux|win32$")
  set(TEXC_IS_DESKTOP ON)
endif()

if(TEXC_IS_DESKTOP)
  # Prevent IPO from merging the baseline dispatcher and separately compiled ISAs.
  function(texc_disable_lto target)
    set_target_properties(${target} PROPERTIES
      INTERPROCEDURAL_OPTIMIZATION FALSE
      INTERPROCEDURAL_OPTIMIZATION_RELEASE FALSE
      INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO FALSE)
  endfunction()

  set(TEXC_SIMD_OBJECTS)
  set(TEXC_SIMD_SHARED_OBJECTS)
  set(TEXC_SIMD_DEFINES)
  set(_texc_simd_variants scalar)
  if(TARGET_PLATFORM MATCHES "^x86_64-")
    list(APPEND _texc_simd_variants sse3 sse42 avx2 avx512)
    set(TEXC_SIMD_DEFINES DM_TEXC_HAS_SSE3 DM_TEXC_HAS_SSE42 DM_TEXC_HAS_AVX2 DM_TEXC_HAS_AVX512)
  elseif(TARGET_PLATFORM MATCHES "^arm64-")
    list(APPEND _texc_simd_variants neon)
    set(TEXC_SIMD_DEFINES DM_TEXC_HAS_NEON)
  endif()
  foreach(_variant IN LISTS _texc_simd_variants)
    string(TOUPPER "${_variant}" _isa)
    if(_variant STREQUAL "scalar")
      set(_source texc_simd_scalar.cpp)
      set(_isa SSE2)
    elseif(_variant MATCHES "^sse")
      set(_source texc_simd_sse.cpp)
      if(_variant STREQUAL "sse42")
        set(_isa SSE4.2)
      endif()
    elseif(_variant MATCHES "^avx")
      set(_source texc_simd_avx.cpp)
    else()
      set(_source texc_simd_neon.cpp)
    endif()
    set(_target texc_simd_${_variant})
    set(_sources "${TEXC_SRC_DIR}/${_source}")
    if(NOT _variant STREQUAL "scalar")
      list(APPEND _sources "${TEXC_SRC_DIR}/texc_simd_resize.cpp")
    endif()
    set(_shared_target ${_target})
    set(_targets ${_target})
    if(WITH_ASAN OR WITH_HWASAN OR WITH_UBSAN OR WITH_TSAN)
      set(_shared_target ${_target}_noasan)
      list(APPEND _targets ${_shared_target})
    endif()
    foreach(_kernel_target IN LISTS _targets)
      add_library(${_kernel_target} OBJECT ${_sources})
      target_link_libraries(${_kernel_target} PRIVATE defold_sdk)
      target_include_directories(${_kernel_target} PRIVATE "${DEFOLD_HOME}/engine/dlib/src")
      set_target_properties(${_kernel_target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
      texc_disable_lto(${_kernel_target})
      defold_target_simd_x86_isa(${_kernel_target} "${_isa}")
    endforeach()
    if(NOT _shared_target STREQUAL _target)
      defold_target_skip_sanitizer(${_shared_target})
    endif()
    list(APPEND TEXC_SIMD_OBJECTS $<TARGET_OBJECTS:${_target}>)
    list(APPEND TEXC_SIMD_SHARED_OBJECTS $<TARGET_OBJECTS:${_shared_target}>)
  endforeach()

  find_package(JNI REQUIRED)
  find_program(DEFOLD_PYTHON NAMES python3 python REQUIRED)
  defold_find_clang_cpp(DEFOLD_CLANGPP)
  if(NOT DEFOLD_CLANGPP)
    message(FATAL_ERROR "Could not find clang++. It is required to generate texc Java bindings.")
  endif()

  set(TEXC_CORE_SOURCES
    "${TEXC_SRC_DIR}/texc.cpp"
    "${TEXC_SRC_DIR}/texc_buffer.cpp"
    "${TEXC_SRC_DIR}/texc_enc_astc.cpp"
    "${TEXC_SRC_DIR}/texc_enc_basis.cpp"
    "${TEXC_SRC_DIR}/texc_enc_default.cpp"
    "${TEXC_SRC_DIR}/texc_private.cpp"
    "${TEXC_SRC_DIR}/texc_ktx2.cpp"
    "${TEXC_SRC_DIR}/texc_resize.cpp")

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
            "CLANGPP=${DEFOLD_CLANGPP}"
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

  defold_add_library(texc STATIC ${TEXC_CORE_SOURCES} ${TEXC_SIMD_OBJECTS})
  target_compile_definitions(texc PRIVATE ${TEXC_SIMD_DEFINES})
  defold_target_simd_x86_isa(texc SSE2)
  texc_disable_lto(texc)
  target_include_directories(texc PUBLIC "${TEXC_SRC_DIR}" PRIVATE "${DEFOLD_HOME}/engine/dlib/src")
  defold_target_link_libraries(texc "${TARGET_PLATFORM}" SCOPE PUBLIC ${TEXC_DEPENDENCIES})

  add_library(texc_bindings OBJECT "${TEXC_GENERATED_CPP}")
  texc_disable_lto(texc_bindings)
  defold_target_simd_x86_isa(texc_bindings SSE2)
  defold_target_skip_sanitizer(texc_bindings)
  target_include_directories(texc_bindings PRIVATE "${TEXC_SRC_DIR}" "${CMAKE_CURRENT_BINARY_DIR}" "${TEXC_JNI_DIR}" ${JNI_INCLUDE_DIRS})
  defold_target_link_libraries(texc_bindings "${TARGET_PLATFORM}" SCOPE PRIVATE ${TEXC_BINDINGS_DEPENDENCIES})
  set_target_properties(texc_bindings PROPERTIES POSITION_INDEPENDENT_CODE ON)
  add_dependencies(texc_bindings texc_generated_bindings)

  add_library(texc_shared SHARED
    ${TEXC_CORE_SOURCES}
    ${TEXC_SIMD_SHARED_OBJECTS}
    "${TEXC_SRC_DIR}/texclibrary_jni.cpp"
    $<TARGET_OBJECTS:texc_bindings>)
  defold_target_skip_sanitizer(texc_shared)
  target_compile_definitions(texc_shared PRIVATE ${TEXC_SIMD_DEFINES})
  defold_target_simd_x86_isa(texc_shared SSE2)
  texc_disable_lto(texc_shared)
  target_include_directories(texc_shared PRIVATE "${DEFOLD_HOME}/engine/dlib/src" "${TEXC_SRC_DIR}" "${CMAKE_CURRENT_BINARY_DIR}" "${TEXC_JNI_DIR}" ${JNI_INCLUDE_DIRS})
  add_dependencies(texc_shared texc_bindings)
  defold_target_link_libraries(texc_shared "${TARGET_PLATFORM}"
    ${TEXC_SHARED_DEPENDENCIES})
  defold_target_link_app(texc_shared "${TARGET_PLATFORM}" SCOPE PRIVATE)
  defold_codesign_target(texc_shared "../engine/darwin/entitlements-macos-debug.plist")

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
endif()
