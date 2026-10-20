find_package(JNI REQUIRED)

# Each contributing module owns its native registration and Java sources.
set(ENGINE_SHARED_MODULES gamesys_shared)
set(ENGINE_SHARED_JAVA_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/java/com/defold/engine/EngineJni.java")
foreach(_module IN LISTS ENGINE_SHARED_MODULES)
  get_target_property(_java_sources ${_module} DEFOLD_JAVA_SOURCES)
  list(APPEND ENGINE_SHARED_JAVA_SOURCES ${_java_sources})
endforeach()

defold_add_library(dmengine_shared SHARED "${CMAKE_CURRENT_SOURCE_DIR}/engine_jni.cpp")
defold_target_skip_sanitizer(dmengine_shared)
target_include_directories(dmengine_shared PRIVATE ${JNI_INCLUDE_DIRS})
defold_target_link_libraries(dmengine_shared "${TARGET_PLATFORM}"
  ${ENGINE_SHARED_MODULES} jni_noasan dlib_noasan zip_noasan mbedtls_noasan profile_null_noasan)
defold_target_link_app(dmengine_shared "${TARGET_PLATFORM}" SCOPE PRIVATE)
defold_codesign_target(dmengine_shared "${ENGINE_ROOT}/darwin/entitlements-macos-debug.plist")
defold_install_targets(TARGETS dmengine_shared
  ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
  LIBRARY DESTINATION lib/${TARGET_PLATFORM}
  RUNTIME DESTINATION lib/${TARGET_PLATFORM})

set(ENGINE_SHARED_JAR "${CMAKE_CURRENT_BINARY_DIR}/dmengine.jar")
set(ENGINE_SHARED_CLASSES_DIR "${CMAKE_CURRENT_BINARY_DIR}/classes")
add_custom_command(
  OUTPUT "${ENGINE_SHARED_JAR}"
  COMMAND "${CMAKE_COMMAND}" -E rm -rf "${ENGINE_SHARED_CLASSES_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${ENGINE_SHARED_CLASSES_DIR}"
  COMMAND "${DEFOLD_JAVAC_EXECUTABLE}" -g --release 8 -Xlint:-options
          -d "${ENGINE_SHARED_CLASSES_DIR}" ${ENGINE_SHARED_JAVA_SOURCES}
  COMMAND "${Java_JAR_EXECUTABLE}" cfe "${ENGINE_SHARED_JAR}"
          com.defold.engine.EngineJni -C "${ENGINE_SHARED_CLASSES_DIR}" .
  DEPENDS ${ENGINE_SHARED_MODULES} ${ENGINE_SHARED_JAVA_SOURCES}
  COMMENT "Packaging dmengine.jar"
  VERBATIM)
add_custom_target(dmengine_jar ALL DEPENDS "${ENGINE_SHARED_JAR}")
install(FILES "${ENGINE_SHARED_JAR}" DESTINATION share/java)

if(BUILD_TESTS)
  set(ENGINE_SHARED_TEST_CLASSES "${CMAKE_CURRENT_BINARY_DIR}/test-classes")
  set(ENGINE_SHARED_TEST_CLASS "${ENGINE_SHARED_TEST_CLASSES}/com/defold/engine/EngineJniTest.class")
  add_custom_command(
    OUTPUT "${ENGINE_SHARED_TEST_CLASS}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${ENGINE_SHARED_TEST_CLASSES}"
    COMMAND "${DEFOLD_JAVAC_EXECUTABLE}" -g --release 8 -Xlint:-options
            -classpath "${ENGINE_SHARED_JAR}" -d "${ENGINE_SHARED_TEST_CLASSES}"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/EngineJniTest.java"
    DEPENDS dmengine_jar "${ENGINE_SHARED_JAR}" "${CMAKE_CURRENT_SOURCE_DIR}/test/EngineJniTest.java"
    VERBATIM)
  add_custom_target(test_engine_shared_java DEPENDS "${ENGINE_SHARED_TEST_CLASS}")
  add_dependencies(build_tests test_engine_shared_java dmengine_shared)
  cmake_path(CONVERT "${ENGINE_SHARED_JAR};${ENGINE_SHARED_TEST_CLASSES}" TO_NATIVE_PATH_LIST _test_classpath)
  set(_java_test_command "${DEFOLD_JAVA_EXECUTABLE}" ${DEFOLD_JAVA_RUNTIME_FLAGS_LIST}
    -Xcheck:jni "-Djava.library.path=$<TARGET_FILE_DIR:dmengine_shared>"
    -classpath "${_test_classpath}" com.defold.engine.EngineJniTest)
  set(_java_main_command "${DEFOLD_JAVA_EXECUTABLE}" ${DEFOLD_JAVA_RUNTIME_FLAGS_LIST}
    -Xcheck:jni "-Ddmengine.library=$<TARGET_FILE:dmengine_shared>" -jar "${ENGINE_SHARED_JAR}")
  _defold_find_python(_test_python)
  set(_main_test_command "${_test_python}" "${CMAKE_CURRENT_SOURCE_DIR}/test/test_engine_shared.py" ${_java_main_command})
  add_custom_target(run_test_engine_shared_api
    COMMAND ${_java_test_command}
    DEPENDS test_engine_shared_java dmengine_shared
    VERBATIM)
  defold_add_to_run_tests(run_test_engine_shared_api RUN_GROUP engine
    COMMAND ${_java_test_command}
    WORKING_DIRECTORY "${ENGINE_ROOT}"
    DEPENDS test_engine_shared_java dmengine_shared)
  add_custom_target(run_test_engine_shared_main
    COMMAND ${_main_test_command}
    DEPENDS dmengine_jar dmengine_shared
    VERBATIM)
  defold_add_to_run_tests(run_test_engine_shared_main RUN_GROUP engine
    COMMAND ${_main_test_command}
    WORKING_DIRECTORY "${ENGINE_ROOT}"
    DEPENDS dmengine_jar dmengine_shared)
  add_custom_target(run_test_engine_shared
    DEPENDS run_test_engine_shared_api run_test_engine_shared_main)
endif()
