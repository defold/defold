set(JNI_TEST_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(JNI_TEST_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(JNI_TEST_JAVA_DIR "${JNI_TEST_BINARY_DIR}/java")
set(JNI_TEST_JNI_DIR "${JNI_TEST_BINARY_DIR}/jni")
set(JNI_TEST_GENERATED_JAVA "${JNI_TEST_JAVA_DIR}/com/dynamo/bob/pipeline/Testapi.java")
set(JNI_TEST_GENERATED_CPP "${JNI_TEST_JNI_DIR}/Testapi_jni.cpp")
set(JNI_TEST_GENERATED_H "${JNI_TEST_JNI_DIR}/Testapi_jni.h")

find_program(DEFOLD_PYTHON NAMES python3 python REQUIRED)

add_custom_command(
  OUTPUT "${JNI_TEST_GENERATED_JAVA}" "${JNI_TEST_GENERATED_CPP}" "${JNI_TEST_GENERATED_H}"
  COMMAND "${CMAKE_COMMAND}" -E env
          "DEFOLD_HOME=${DEFOLD_HOME}"
          "DYNAMO_HOME=${DEFOLD_SDK_ROOT}"
          "${DEFOLD_PYTHON}" "${DEFOLD_CMAKE_DIR}/gen_java_bindings.py"
          --gen-java "${JNI_ROOT}/scripts/gen_java.py"
          --header "${JNI_TEST_DIR}/testapi.h"
          --namespace dmJniTest
          --package com.dynamo.bob.pipeline
          --java-outdir "${JNI_TEST_JAVA_DIR}"
          --jni-outdir "${JNI_TEST_JNI_DIR}"
          --include "${JNI_TEST_DIR}"
          --include "${JNI_SRC_DIR}"
          --include "${DEFOLD_DMSDK_INCLUDE_DIR}"
          --include "${DEFOLD_INCLUDE_DIR}"
          --include "${DEFOLD_EXT_INCLUDE_DIR}"
  DEPENDS
    "${JNI_ROOT}/scripts/gen_java.py"
    "${JNI_TEST_DIR}/testapi.h"
  COMMENT "Generating Java/JNI bindings for testapi.h")

defold_add_executable(test_jni
  "${JNI_TEST_DIR}/test_jni.cpp")
target_include_directories(test_jni PRIVATE "${JNI_TEST_DIR}" "${JNI_SRC_DIR}" "${JNI_TEST_JNI_DIR}" ${JNI_INCLUDE_DIRS})
defold_target_link_libraries(test_jni "${TARGET_PLATFORM}" jni dlib zip mbedtls profile_null)
if(TARGET_PLATFORM MATCHES "win32$")
  target_link_libraries(test_jni PRIVATE dbghelp.lib)
endif()
defold_register_test_target(test_jni ON "${JNI_ROOT}")

add_library(test_jni_shared SHARED
  "${JNI_TEST_DIR}/testapi.cpp"
  "${JNI_TEST_GENERATED_CPP}")
target_include_directories(test_jni_shared PRIVATE "${JNI_TEST_DIR}" "${JNI_SRC_DIR}" "${JNI_TEST_JNI_DIR}" ${JNI_INCLUDE_DIRS})
defold_target_link_libraries(test_jni_shared "${TARGET_PLATFORM}" jni_noasan dlib_noasan zip_noasan mbedtls_noasan profile_null_noasan)
if(TARGET_PLATFORM MATCHES "win32$")
  target_link_libraries(test_jni_shared PRIVATE dbghelp.lib)
endif()
set_target_properties(test_jni_shared PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
add_dependencies(build_tests test_jni_shared)

set(JNI_TEST_JAR "${CMAKE_CURRENT_BINARY_DIR}/test_jni.jar")
set(JNI_TEST_CLASSES_DIR "${CMAKE_CURRENT_BINARY_DIR}/classes")
set(JNI_TEST_JUNIT "${DEFOLD_SDK_ROOT}/ext/share/java/junit-4.6.jar")
add_custom_command(
  OUTPUT "${JNI_TEST_JAR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${JNI_TEST_CLASSES_DIR}"
  COMMAND "${DEFOLD_JAVAC_EXECUTABLE}" -g --release 8 -Xlint:-options
          -classpath "${JNI_TEST_JUNIT}"
          -d "${JNI_TEST_CLASSES_DIR}"
          "${JNI_TEST_GENERATED_JAVA}"
          "${JNI_TEST_DIR}/java/com/dynamo/bob/pipeline/JniTest.java"
  COMMAND "${Java_JAR_EXECUTABLE}" cfe "${JNI_TEST_JAR}" com.dynamo.bob.pipeline.JniTest -C "${JNI_TEST_CLASSES_DIR}" .
  DEPENDS "${JNI_TEST_GENERATED_JAVA}" "${JNI_TEST_DIR}/java/com/dynamo/bob/pipeline/JniTest.java"
  COMMENT "Packaging test_jni.jar")
add_custom_target(test_jni_jar DEPENDS "${JNI_TEST_JAR}")
add_dependencies(build_tests test_jni_jar)
