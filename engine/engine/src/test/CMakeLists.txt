cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

set(ENGINE_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(ENGINE_TEST_ENGINE_ROOT "${ENGINE_TEST_ROOT}/../.." ABSOLUTE)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Java REQUIRED COMPONENTS Runtime)

set(ENGINE_PROFILEREXT_LUA_EXPORTS
  ProfilerExt
  ProfilerBasic
  GraphicsAdapterNull)
set(ENGINE_PROFILEREXT_LUA_EXPORTS_CPP "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols_test_profilerext_lua.cpp")
defold_create_exported_symbols_file("${ENGINE_PROFILEREXT_LUA_EXPORTS_CPP}" "${ENGINE_PROFILEREXT_LUA_EXPORTS}")

defold_add_executable(test_profilerext_lua
  "${ENGINE_TEST_ROOT}/test_profilerext_lua.cpp"
  "${ENGINE_PROFILEREXT_LUA_EXPORTS_CPP}")
target_include_directories(test_profilerext_lua PRIVATE
  "${ENGINE_TEST_ROOT}"
  "${ENGINE_TEST_ENGINE_ROOT}/src"
  "${ENGINE_TEST_ENGINE_ROOT}/proto")
defold_target_link_libraries(test_profilerext_lua "${TARGET_PLATFORM}"
  testmain
  resource
  ddf
  font
  graphics_null
  image
  graphics_transcoder_null
  platform_null
  render
  render_font_default
  script
  extension
  dlib
  zip
  mbedtls
  profile
  profilerext
  lua)
defold_target_link_lua(test_profilerext_lua "${TARGET_PLATFORM}")
defold_target_link_socket(test_profilerext_lua "${TARGET_PLATFORM}")
defold_target_link_app(test_profilerext_lua "${TARGET_PLATFORM}")
if(TARGET_PLATFORM MATCHES "win32")
  target_link_libraries(test_profilerext_lua PRIVATE Psapi.lib)
endif()

defold_register_test_target(test_profilerext_lua ON "${ENGINE_TEST_ENGINE_ROOT}")

set(ENGINE_TEST_RUNTIME_ROOT "${ENGINE_TEST_ENGINE_ROOT}/build/src/test")
set(ENGINE_TEST_CONTENT_ROOT "build/src/test/build/default")
set(ENGINE_TEST_CONTENT_STAMP "${ENGINE_TEST_RUNTIME_ROOT}/.cmake/test_content.stamp")
set(ENGINE_TEST_CONTENT_SCRIPT "${ENGINE_TEST_ROOT}/build_cmake_test_content.py")
set(ENGINE_TEST_CONTENT_SETTINGS
  "cross_script_messaging.ini"
  "render_script.ini"
  "camera_acquire_input_focus.ini")

set(ENGINE_TEST_CLASSPATH "${ENGINE_BOB_ENGINE_JAR}")
if(TARGET shaderc_jar)
  set(ENGINE_SHADERC_JAR "${DEFOLD_SHADERC_BINARY_DIR}/shaderc.jar")
elseif(EXISTS "${DEFOLD_SDK_ROOT}/share/java/shaderc.jar")
  set(ENGINE_SHADERC_JAR "${DEFOLD_SDK_ROOT}/share/java/shaderc.jar")
endif()
if(ENGINE_SHADERC_JAR)
  list(APPEND ENGINE_TEST_CLASSPATH "${ENGINE_SHADERC_JAR}")
endif()

set(ENGINE_TEST_CONTENT_SETTINGS_ARGS)
foreach(_engine_test_setting IN LISTS ENGINE_TEST_CONTENT_SETTINGS)
  list(APPEND ENGINE_TEST_CONTENT_SETTINGS_ARGS --settings "${_engine_test_setting}")
endforeach()
set(ENGINE_TEST_CLASSPATH_ARGS)
foreach(_engine_test_classpath_entry IN LISTS ENGINE_TEST_CLASSPATH)
  list(APPEND ENGINE_TEST_CLASSPATH_ARGS --classpath "${_engine_test_classpath_entry}")
endforeach()

file(GLOB_RECURSE ENGINE_TEST_CONTENT_SOURCES CONFIGURE_DEPENDS LIST_DIRECTORIES false
  "${ENGINE_TEST_ROOT}/*")
list(FILTER ENGINE_TEST_CONTENT_SOURCES EXCLUDE REGEX "/build/")
list(FILTER ENGINE_TEST_CONTENT_SOURCES EXCLUDE REGEX "/__pycache__/")
list(FILTER ENGINE_TEST_CONTENT_SOURCES EXCLUDE REGEX "/CMakeLists\\.txt$")
list(FILTER ENGINE_TEST_CONTENT_SOURCES EXCLUDE REGEX "/wscript$")
list(FILTER ENGINE_TEST_CONTENT_SOURCES EXCLUDE REGEX "\\.(cpp|h)$")

add_custom_command(
  OUTPUT "${ENGINE_TEST_CONTENT_STAMP}"
  COMMAND "${Python3_EXECUTABLE}" "${ENGINE_TEST_CONTENT_SCRIPT}"
    --source-root "${ENGINE_TEST_ROOT}"
    --stage-root "${ENGINE_TEST_RUNTIME_ROOT}"
    --builtins-root "${ENGINE_BUILTINS_COMPILED_DIR}"
    --stamp "${ENGINE_TEST_CONTENT_STAMP}"
    --platform "${TARGET_PLATFORM}"
    --java "${Java_JAVA_EXECUTABLE}"
    ${ENGINE_TEST_CLASSPATH_ARGS}
    ${ENGINE_TEST_CONTENT_SETTINGS_ARGS}
  DEPENDS
    "${ENGINE_TEST_CONTENT_SCRIPT}"
    "${ENGINE_BOB_ENGINE_JAR}"
    ${ENGINE_SHADERC_JAR}
    ${ENGINE_TEST_CONTENT_SOURCES}
  WORKING_DIRECTORY "${ENGINE_TEST_ENGINE_ROOT}"
  COMMENT "Building engine test content"
  VERBATIM)
add_custom_target(engine_test_content DEPENDS "${ENGINE_TEST_CONTENT_STAMP}")
add_dependencies(engine_test_content engine_bob_engine_jar engine_builtins)
if(TARGET shaderc_jar)
  add_dependencies(engine_test_content shaderc_jar)
endif()

set(ENGINE_TEST_SYMBOLS
  ProfilerExt
  GraphicsAdapterNull
  ScriptModelExt
  ${ENGINE_RESOURCE_SYMBOLS}
  ${ENGINE_COMPONENT_SYMBOLS})
set(ENGINE_TEST_EXPORTS_CPP "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols_test_engine.cpp")
defold_create_exported_symbols_file("${ENGINE_TEST_EXPORTS_CPP}" "${ENGINE_TEST_SYMBOLS}")

defold_add_executable(test_engine
  "${ENGINE_TEST_ROOT}/test_engine.cpp"
  "${ENGINE_TEST_ROOT}/test_engine_service.cpp"
  "${ENGINE_TEST_EXPORTS_CPP}")
add_dependencies(test_engine engine_test_content)
target_include_directories(test_engine PRIVATE
  "${ENGINE_TEST_ROOT}"
  "${ENGINE_SRC_DIR}"
  "${ENGINE_ROOT}"
  "${ENGINE_PROTO_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(test_engine PRIVATE CONTENT_ROOT="${ENGINE_TEST_CONTENT_ROOT}")
defold_target_link_libraries(test_engine "${TARGET_PLATFORM}" testmain)
engine_configure_executable(test_engine test engine engine_service null "profile_null;profilerext_null" "ProfilerExt")
defold_register_test_target(test_engine ON "${ENGINE_TEST_ENGINE_ROOT}"
  STAGE_FILES
    "build/src/test" "build/src/test")
