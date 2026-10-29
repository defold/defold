find_package(JNI REQUIRED)

# Each contributing module owns its native registration, Java sources and API tests.
# Contributors must be configured before engine/engine so their targets and
# DEFOLD_JAVA_* properties exist when this directory is processed.
set(ENGINE_SHARED_MODULES gamesys_shared)
set(ENGINE_SHARED_JAVA_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/java/com/defold/engine/EngineJni.java")
set(ENGINE_SHARED_JAVA_TEST_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/test/DispatchTestCommands.java")
foreach(_module IN LISTS ENGINE_SHARED_MODULES)
  if(NOT TARGET ${_module})
    message(FATAL_ERROR "Configure contributor ${_module} before engine/engine shared bindings.")
  endif()
  get_target_property(_java_sources ${_module} DEFOLD_JAVA_SOURCES)
  list(APPEND ENGINE_SHARED_JAVA_SOURCES ${_java_sources})
  if(BUILD_TESTS)
    get_target_property(_test_sources ${_module} DEFOLD_JAVA_TEST_SOURCES)
    get_target_property(_test_classes ${_module} DEFOLD_JAVA_TEST_CLASSES)
    if(_test_sources AND _test_classes)
      list(APPEND ENGINE_SHARED_JAVA_TEST_SOURCES ${_test_sources})
      list(APPEND ENGINE_SHARED_JAVA_TEST_CLASSES ${_test_classes})
    endif()
    get_target_property(_cli_test_scripts ${_module} DEFOLD_JAVA_CLI_TEST_SCRIPTS)
    if(_cli_test_scripts)
      list(APPEND ENGINE_SHARED_JAVA_CLI_TEST_SCRIPTS ${_cli_test_scripts})
    endif()
  endif()
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
  set(ENGINE_SHARED_TEST_CLASS "${ENGINE_SHARED_TEST_CLASSES}/compiled.stamp")
  add_custom_command(
    OUTPUT "${ENGINE_SHARED_TEST_CLASS}"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${ENGINE_SHARED_TEST_CLASSES}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${ENGINE_SHARED_TEST_CLASSES}"
    COMMAND "${DEFOLD_JAVAC_EXECUTABLE}" -g --release 8 -Xlint:-options
            -classpath "${ENGINE_SHARED_JAR}" -d "${ENGINE_SHARED_TEST_CLASSES}"
            ${ENGINE_SHARED_JAVA_TEST_SOURCES}
    COMMAND "${CMAKE_COMMAND}" -E touch "${ENGINE_SHARED_TEST_CLASS}"
    DEPENDS dmengine_jar "${ENGINE_SHARED_JAR}" ${ENGINE_SHARED_JAVA_TEST_SOURCES}
    VERBATIM)
  add_custom_target(test_engine_shared_java DEPENDS "${ENGINE_SHARED_TEST_CLASS}")
  add_dependencies(build_tests test_engine_shared_java dmengine_shared)
endif()

# The host JVM cannot load a shared library built for a different architecture.
if(BUILD_TESTS AND TARGET_PLATFORM STREQUAL HOST_PLATFORM)
  cmake_path(CONVERT "${ENGINE_SHARED_JAR};${ENGINE_SHARED_TEST_CLASSES}" TO_NATIVE_PATH_LIST _test_classpath)
  # Preserve Windows classpath separators when expanding the command as a CMake list.
  string(REPLACE ";" "$<SEMICOLON>" _test_classpath "${_test_classpath}")
  set(_java_test_command "${DEFOLD_JAVA_EXECUTABLE}" ${DEFOLD_JAVA_RUNTIME_FLAGS_LIST}
    -Xcheck:jni "-Djava.library.path=$<TARGET_FILE_DIR:dmengine_shared>"
    -classpath "${_test_classpath}")
  set(_java_main_command "${DEFOLD_JAVA_EXECUTABLE}" ${DEFOLD_JAVA_RUNTIME_FLAGS_LIST}
    -Xcheck:jni "-Ddmengine.library=$<TARGET_FILE:dmengine_shared>" -jar "${ENGINE_SHARED_JAR}")
  _defold_find_python(_test_python)
  set(_main_test_command "${_test_python}" "${CMAKE_CURRENT_SOURCE_DIR}/test/test_engine_shared.py"
    --test-classes "${ENGINE_SHARED_TEST_CLASSES}" ${_java_main_command})
  add_custom_target(run_test_engine_shared_api)
  foreach(_test_class IN LISTS ENGINE_SHARED_JAVA_TEST_CLASSES)
    set(_test_target "run_test_${_test_class}")
    add_custom_target(${_test_target}
      COMMAND ${_java_test_command} ${_test_class}
      DEPENDS test_engine_shared_java dmengine_shared
      VERBATIM)
    add_dependencies(run_test_engine_shared_api ${_test_target})
    defold_add_to_run_tests(${_test_target} RUN_GROUP engine
      COMMAND ${_java_test_command} ${_test_class}
      WORKING_DIRECTORY "${ENGINE_ROOT}"
      DEPENDS test_engine_shared_java dmengine_shared)
  endforeach()
  add_custom_target(run_test_engine_shared_main
    COMMAND ${_main_test_command}
    DEPENDS test_engine_shared_java dmengine_shared
    VERBATIM)
  defold_add_to_run_tests(run_test_engine_shared_main RUN_GROUP engine
    COMMAND ${_main_test_command}
    WORKING_DIRECTORY "${ENGINE_ROOT}"
    DEPENDS test_engine_shared_java dmengine_shared)
  add_custom_target(run_test_engine_shared_cli)
  foreach(_test_script IN LISTS ENGINE_SHARED_JAVA_CLI_TEST_SCRIPTS)
    get_filename_component(_test_name "${_test_script}" NAME_WE)
    set(_test_target "run_${_test_name}")
    set(_test_command "${_test_python}" "${_test_script}" ${_java_main_command})
    add_custom_target(${_test_target}
      COMMAND ${_test_command}
      DEPENDS dmengine_jar dmengine_shared
      VERBATIM)
    add_dependencies(run_test_engine_shared_cli ${_test_target})
    defold_add_to_run_tests(${_test_target} RUN_GROUP engine
      COMMAND ${_test_command}
      WORKING_DIRECTORY "${ENGINE_ROOT}"
      DEPENDS dmengine_jar dmengine_shared)
  endforeach()
  add_custom_target(run_test_engine_shared
    DEPENDS run_test_engine_shared_api run_test_engine_shared_main run_test_engine_shared_cli)
endif()
