# Gamesys' contribution to dmengine_shared. The runtime gamesys library keeps
# compiling the same geometry source without any Java/JNI dependencies.
find_package(JNI REQUIRED)
find_program(DEFOLD_PYTHON NAMES python3 python REQUIRED)
defold_find_clang_cpp(DEFOLD_CLANGPP)
if(NOT DEFOLD_CLANGPP)
  message(FATAL_ERROR "Could not find clang++. It is required to generate gamesys Java bindings.")
endif()

set(GS_JAVA_DIR "${CMAKE_CURRENT_BINARY_DIR}/java")
set(GS_JNI_DIR "${CMAKE_CURRENT_BINARY_DIR}/jni")
set(GS_GENERATED_JAVA "${GS_JAVA_DIR}/com/defold/engine/Geometry.java")
set(GS_GENERATED_CPP "${GS_JNI_DIR}/Geometry_jni.cpp")
set(GS_GENERATED_H "${GS_JNI_DIR}/Geometry_jni.h")
set(GS_GEOMETRY_JAVA "${GS_SRC_DIR}/java/com/defold/engine/GeometryJni.java")

add_custom_command(
  OUTPUT "${GS_GENERATED_JAVA}" "${GS_GENERATED_CPP}" "${GS_GENERATED_H}"
  COMMAND "${CMAKE_COMMAND}" -E env
          "DEFOLD_HOME=${DEFOLD_HOME}"
          "DYNAMO_HOME=${DEFOLD_SDK_ROOT}"
          "CLANGPP=${DEFOLD_CLANGPP}"
          "${DEFOLD_PYTHON}" "${DEFOLD_CMAKE_DIR}/gen_java_bindings.py"
          --gen-java "${DEFOLD_HOME}/engine/jni/scripts/gen_java.py"
          --header "${GS_SRC_DIR}/geometry.h"
          --namespace dmGameSystem
          --package com.defold.engine
          --java-outdir "${GS_JAVA_DIR}"
          --jni-outdir "${GS_JNI_DIR}"
          --include "${GS_SRC_DIR}"
  DEPENDS
    "${DEFOLD_CMAKE_DIR}/gen_java_bindings.py"
    "${DEFOLD_HOME}/engine/jni/scripts/gen_java.py"
    "${DEFOLD_HOME}/engine/jni/scripts/external/gen_ir.py"
    "${DEFOLD_HOME}/engine/jni/scripts/external/gen_util.py"
    "${GS_SRC_DIR}/geometry.h"
  COMMENT "Generating Java/JNI bindings for geometry.h"
  VERBATIM)

defold_add_library(gamesys_shared STATIC
  "${GS_SRC_DIR}/geometry.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/geometry_jni.cpp"
  "${GS_GENERATED_CPP}")
defold_target_skip_sanitizer(gamesys_shared)
set_target_properties(gamesys_shared PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  DEFOLD_JAVA_SOURCES "${GS_GENERATED_JAVA};${GS_GEOMETRY_JAVA}")
target_include_directories(gamesys_shared PUBLIC "${GS_DMSDK_SRC_DIR}"
  PRIVATE "${GS_SRC_DIR}" "${GS_JNI_DIR}" ${JNI_INCLUDE_DIRS})
defold_target_link_libraries(gamesys_shared "${TARGET_PLATFORM}" SCOPE PRIVATE jni_noasan)
