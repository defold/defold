cmake_minimum_required(VERSION 4.0)

# Top-level CMake for Defold libs

# Set module path and run SDK detection early so toolchains (e.g., Emscripten)
# can set CMAKE_TOOLCHAIN_FILE before compilers are enabled by project().
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/scripts/cmake")
include(defold)

set(DEFOLD_SOLUTION_NAME "defold_libraries" CACHE STRING "Name used for generated IDE solutions")
project(${DEFOLD_SOLUTION_NAME} LANGUAGES ${DEFOLD_LANGUAGE_LIST})

# Load full Defold CMake (platform flags, interface target, etc.)
include("${CMAKE_CURRENT_LIST_DIR}/scripts/cmake/defold.cmake")

set(DEFOLD_BOB_DIR "${CMAKE_CURRENT_LIST_DIR}/com.dynamo.cr/com.dynamo.cr.bob")
set(DEFOLD_BOB_LIGHT_JAR "${DEFOLD_SDK_ROOT}/share/java/bob-light.jar" CACHE FILEPATH "Path to generated bob-light.jar" FORCE)
if(CMAKE_HOST_WIN32)
  set(DEFOLD_GRADLEW "${DEFOLD_BOB_DIR}/gradlew.bat")
else()
  set(DEFOLD_GRADLEW "${DEFOLD_BOB_DIR}/gradlew")
endif()
get_filename_component(_DEFOLD_JAVA_BIN_DIR "${Java_JAVA_EXECUTABLE}" DIRECTORY)
get_filename_component(DEFOLD_JAVA_HOME "${_DEFOLD_JAVA_BIN_DIR}/.." ABSOLUTE)
unset(_DEFOLD_JAVA_BIN_DIR)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

option(DEFOLD_SKIP_BOB_LIGHT "Use an existing bob-light.jar instead of building it" OFF)
if(NOT DEFOLD_SKIP_BOB_LIGHT)
  add_custom_target(bob_light DEPENDS "${DEFOLD_BOB_LIGHT_JAR}")
endif()
if(TARGET build_tests AND TARGET bob_light)
  add_dependencies(build_tests bob_light)
endif()

set(DEFOLD_HOST_TOOLS_TARGET "")
if(NOT TARGET_PLATFORM STREQUAL HOST_PLATFORM)
  set(DEFOLD_HOST_TOOLS_TARGET host_tools)
  set(DEFOLD_HOST_TOOLS_BUILD_DIR "${CMAKE_BINARY_DIR}/host-tools")
  set(_DEFOLD_HOST_TOOLS_CONFIGURE_ARGS
    -S "${DEFOLD_HOME}"
    -B "${DEFOLD_HOST_TOOLS_BUILD_DIR}"
    -D "TARGET_PLATFORM=${HOST_PLATFORM}"
    -D "BUILD_TESTS=OFF"
    -D "DEFOLD_ENGINE_LIB_SET=host"
    -D "DEFOLD_SKIP_BOB_LIGHT=ON"
    -D "DEFOLD_BUILD_HOME:PATH=${DEFOLD_BUILD_HOME}"
    -D "DEFOLD_SDK_ROOT:PATH=${DEFOLD_SDK_ROOT}"
    -D "CMAKE_INSTALL_PREFIX:PATH=${DEFOLD_SDK_ROOT}"
    -D "DEFOLD_SOLUTION_NAME:STRING=Defold-host-tools-${HOST_PLATFORM}")
  if(CMAKE_GENERATOR)
    list(APPEND _DEFOLD_HOST_TOOLS_CONFIGURE_ARGS -G "${CMAKE_GENERATOR}")
  endif()
  if(CMAKE_GENERATOR MATCHES "Visual Studio" AND CMAKE_HOST_WIN32)
    if(HOST_PLATFORM STREQUAL "x86_64-win32")
      list(APPEND _DEFOLD_HOST_TOOLS_CONFIGURE_ARGS -A x64)
    elseif(HOST_PLATFORM STREQUAL "x86-win32")
      list(APPEND _DEFOLD_HOST_TOOLS_CONFIGURE_ARGS -A Win32)
    elseif(HOST_PLATFORM STREQUAL "arm64-win32")
      list(APPEND _DEFOLD_HOST_TOOLS_CONFIGURE_ARGS -A ARM64)
    endif()
  elseif(CMAKE_GENERATOR MATCHES "Visual Studio" AND CMAKE_GENERATOR_PLATFORM)
    list(APPEND _DEFOLD_HOST_TOOLS_CONFIGURE_ARGS -A "${CMAKE_GENERATOR_PLATFORM}")
  endif()
  if(CMAKE_GENERATOR_TOOLSET)
    list(APPEND _DEFOLD_HOST_TOOLS_CONFIGURE_ARGS -T "${CMAKE_GENERATOR_TOOLSET}")
  endif()
  if(CMAKE_GENERATOR_INSTANCE)
    list(APPEND _DEFOLD_HOST_TOOLS_CONFIGURE_ARGS -D "CMAKE_GENERATOR_INSTANCE:PATH=${CMAKE_GENERATOR_INSTANCE}")
  endif()
  if(DEFOLD_VISUAL_STUDIO_ROOT)
    list(APPEND _DEFOLD_HOST_TOOLS_CONFIGURE_ARGS -D "DEFOLD_VISUAL_STUDIO_ROOT:PATH=${DEFOLD_VISUAL_STUDIO_ROOT}")
  endif()
  if(DEFOLD_WINDOWS_SDK_VERSION)
    list(APPEND _DEFOLD_HOST_TOOLS_CONFIGURE_ARGS -D "DEFOLD_WINDOWS_SDK_VERSION=${DEFOLD_WINDOWS_SDK_VERSION}")
  endif()
  set(_DEFOLD_HOST_TOOLS_BUILD_ARGS)
  if(CMAKE_GENERATOR MATCHES "Visual Studio")
    list(APPEND _DEFOLD_HOST_TOOLS_BUILD_ARGS -- /nodeReuse:false /v:minimal)
  endif()
  set(_DEFOLD_HOST_TOOLS_PATH "$ENV{Path}")
  if(NOT _DEFOLD_HOST_TOOLS_PATH)
    set(_DEFOLD_HOST_TOOLS_PATH "$ENV{PATH}")
  endif()

  function(_defold_make_script_args out_var)
    set(_script_args)
    foreach(_script_arg IN LISTS ARGN)
      string(REPLACE "\\" "\\\\" _script_arg "${_script_arg}")
      string(REPLACE "\"" "\\\"" _script_arg "${_script_arg}")
      list(APPEND _script_args "\"${_script_arg}\"")
    endforeach()
    string(JOIN " " _script_args ${_script_args})
    set(${out_var} "${_script_args}" PARENT_SCOPE)
  endfunction()
  _defold_make_script_args(DEFOLD_HOST_TOOLS_CONFIGURE_SCRIPT_ARGS ${_DEFOLD_HOST_TOOLS_CONFIGURE_ARGS})
  _defold_make_script_args(DEFOLD_HOST_TOOLS_BUILD_SCRIPT_ARGS ${_DEFOLD_HOST_TOOLS_BUILD_ARGS})
  set(DEFOLD_HOST_TOOLS_ENV_PATH "${_DEFOLD_HOST_TOOLS_PATH}")
  set(DEFOLD_HOST_TOOLS_NORMALIZE_WINDOWS_PATH OFF)
  if(CMAKE_HOST_WIN32)
    set(DEFOLD_HOST_TOOLS_NORMALIZE_WINDOWS_PATH ON)
  endif()
  set(_DEFOLD_HOST_TOOLS_SCRIPT "${CMAKE_BINARY_DIR}/run_host_tools.cmake")
  configure_file("${CMAKE_CURRENT_LIST_DIR}/scripts/cmake/host_tools.cmake.in" "${_DEFOLD_HOST_TOOLS_SCRIPT}" @ONLY)
  file(GLOB_RECURSE _DEFOLD_HOST_TOOLS_INPUTS CONFIGURE_DEPENDS
    "${DEFOLD_HOME}/CMakeLists.txt"
    "${DEFOLD_HOME}/scripts/cmake/*.cmake"
    "${DEFOLD_HOME}/engine/dlib/CMakeLists.txt"
    "${DEFOLD_HOME}/engine/dlib/src/*"
    "${DEFOLD_HOME}/engine/jni/CMakeLists.txt"
    "${DEFOLD_HOME}/engine/jni/src/*"
    "${DEFOLD_HOME}/engine/texc/CMakeLists.txt"
    "${DEFOLD_HOME}/engine/texc/src/*"
    "${DEFOLD_HOME}/engine/modelc/CMakeLists.txt"
    "${DEFOLD_HOME}/engine/modelc/src/*"
    "${DEFOLD_HOME}/engine/font/CMakeLists.txt"
    "${DEFOLD_HOME}/engine/font/src/*"
    "${DEFOLD_HOME}/engine/shaderc/CMakeLists.txt"
    "${DEFOLD_HOME}/engine/shaderc/src/*")
  set(_DEFOLD_HOST_TOOLS_STAMP "${CMAKE_BINARY_DIR}/CMakeFiles/host_tools-$<CONFIG>.stamp")

  add_custom_command(
    OUTPUT "${_DEFOLD_HOST_TOOLS_STAMP}"
    COMMAND "${CMAKE_COMMAND}" -D "DEFOLD_HOST_TOOLS_CONFIG=$<CONFIG>" -P "${_DEFOLD_HOST_TOOLS_SCRIPT}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${_DEFOLD_HOST_TOOLS_STAMP}"
    DEPENDS "${_DEFOLD_HOST_TOOLS_SCRIPT}" ${_DEFOLD_HOST_TOOLS_INPUTS}
    COMMENT "Building Defold host tools for ${HOST_PLATFORM}"
    VERBATIM)
  add_custom_target(host_tools DEPENDS "${_DEFOLD_HOST_TOOLS_STAMP}")
  unset(_DEFOLD_HOST_TOOLS_CONFIGURE_ARGS)
  unset(_DEFOLD_HOST_TOOLS_BUILD_ARGS)
  unset(_DEFOLD_HOST_TOOLS_INPUTS)
  unset(_DEFOLD_HOST_TOOLS_SCRIPT)
  unset(_DEFOLD_HOST_TOOLS_STAMP)
  unset(_DEFOLD_HOST_TOOLS_PATH)
  unset(DEFOLD_HOST_TOOLS_CONFIGURE_SCRIPT_ARGS)
  unset(DEFOLD_HOST_TOOLS_BUILD_SCRIPT_ARGS)
  unset(DEFOLD_HOST_TOOLS_ENV_PATH)
  unset(DEFOLD_HOST_TOOLS_NORMALIZE_WINDOWS_PATH)
endif()

# Engine libs.
set(DEFOLD_ENGINE_LIBS
  testmain
  dlib
  jni
  texc
  modelc
  shaderc
  ddf
  platform
  graphics
  font
  particle
  lua
  hid
  input
  physics
  resource
  extension
  script
  render
  rig
  gameobject
  gui
  sound
  liveupdate
  crash
  gamesys
  tools
  record
  profiler
  engine
  sdk)
set(DEFOLD_HOST_LIBS
  testmain
  dlib
  jni
  texc
  modelc
  font
  shaderc)
set(DEFOLD_TARGET_LIBS ${DEFOLD_ENGINE_LIBS})
# Bob archives ship compiler JNI libraries for these desktop targets, even when
# the target is cross-built on a different host.
if(NOT TARGET_PLATFORM MATCHES "^(x86_64|arm64)-(macos|linux)$|^x86_64-win32$")
  list(REMOVE_ITEM DEFOLD_TARGET_LIBS jni texc modelc shaderc)
endif()
set(DEFOLD_ENGINE_LIB_SET "all" CACHE STRING "Defold engine lib set to configure: all, host or target")
set(DEFOLD_SELECTED_ENGINE_LIBS "" CACHE STRING "Optional semicolon or comma separated lib override")
if(DEFOLD_SELECTED_ENGINE_LIBS)
  string(REPLACE "," ";" DEFOLD_SELECTED_ENGINE_LIBS "${DEFOLD_SELECTED_ENGINE_LIBS}")
elseif(DEFOLD_ENGINE_LIB_SET STREQUAL "all")
  set(DEFOLD_SELECTED_ENGINE_LIBS ${DEFOLD_ENGINE_LIBS})
elseif(DEFOLD_ENGINE_LIB_SET STREQUAL "host")
  set(DEFOLD_SELECTED_ENGINE_LIBS ${DEFOLD_HOST_LIBS})
elseif(DEFOLD_ENGINE_LIB_SET STREQUAL "target")
  set(DEFOLD_SELECTED_ENGINE_LIBS ${DEFOLD_TARGET_LIBS})
else()
  message(FATAL_ERROR "Unknown DEFOLD_ENGINE_LIB_SET: ${DEFOLD_ENGINE_LIB_SET}")
endif()

message(STATUS "Defold engine libs: ${DEFOLD_SELECTED_ENGINE_LIBS}")
foreach(_DEFOLD_ENGINE_LIB IN LISTS DEFOLD_SELECTED_ENGINE_LIBS)
  set(_DEFOLD_ENGINE_LIB_BINARY_DIR "${DEFOLD_BUILD_HOME}/engine/${_DEFOLD_ENGINE_LIB}/build/${TARGET_PLATFORM}")
  string(TOUPPER "${_DEFOLD_ENGINE_LIB}" _DEFOLD_ENGINE_LIB_UPPER)
  set("DEFOLD_${_DEFOLD_ENGINE_LIB_UPPER}_BINARY_DIR" "${_DEFOLD_ENGINE_LIB_BINARY_DIR}" CACHE INTERNAL "Binary directory for engine/${_DEFOLD_ENGINE_LIB}")
  add_subdirectory("engine/${_DEFOLD_ENGINE_LIB}" "${_DEFOLD_ENGINE_LIB_BINARY_DIR}")
endforeach()
unset(_DEFOLD_ENGINE_LIB)
unset(_DEFOLD_ENGINE_LIB_BINARY_DIR)
unset(_DEFOLD_ENGINE_LIB_UPPER)

if(DEFOLD_ENGINE_LIB_SET STREQUAL "all" OR DEFOLD_ENGINE_LIB_SET STREQUAL "target")
  add_subdirectory("share/extender" "${DEFOLD_BUILD_HOME}/share/extender/build/${TARGET_PLATFORM}")
endif()

# Subprojects call project(), which can reinitialize an unset install prefix.
# Reassert the SDK destination after configuring all subprojects so direct
# CMake configurations cannot generate absolute /sdk, /lib, or /share paths.
set(CMAKE_INSTALL_PREFIX "${DEFOLD_SDK_ROOT}" CACHE PATH "Install prefix" FORCE)

set(_DEFOLD_BOB_LIGHT_DEPENDS
  "${DEFOLD_BOB_DIR}/build.gradle"
  "${DEFOLD_GRADLEW}")
if(EXISTS "${DEFOLD_HOME}/.defold-platforms")
  list(APPEND _DEFOLD_BOB_LIGHT_DEPENDS "${DEFOLD_HOME}/.defold-platforms")
endif()
set(_DEFOLD_BOB_LIGHT_COPY_COMMANDS)
foreach(_DEFOLD_BOB_LIGHT_LIB IN ITEMS texc_shared modelc_shared shaderc_shared fontc_shared)
  if(TARGET ${_DEFOLD_BOB_LIGHT_LIB})
    list(APPEND _DEFOLD_BOB_LIGHT_DEPENDS ${_DEFOLD_BOB_LIGHT_LIB})
    list(APPEND _DEFOLD_BOB_LIGHT_COPY_COMMANDS
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:${_DEFOLD_BOB_LIGHT_LIB}>"
        "${DEFOLD_LIB_DIR}/$<TARGET_FILE_NAME:${_DEFOLD_BOB_LIGHT_LIB}>")
  endif()
endforeach()
set(_DEFOLD_BOB_LIGHT_JAR_COPY_COMMANDS)
if(TARGET dlib_java)
  list(APPEND _DEFOLD_BOB_LIGHT_DEPENDS
    dlib_java
    "${DEFOLD_DLIB_BINARY_DIR}/dlib.jar")
  list(APPEND _DEFOLD_BOB_LIGHT_JAR_COPY_COMMANDS
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${DEFOLD_DLIB_BINARY_DIR}/dlib.jar"
      "${DEFOLD_SDK_ROOT}/share/java/dlib.jar")
endif()
if(TARGET modelimporter_jar)
  list(APPEND _DEFOLD_BOB_LIGHT_DEPENDS
    modelimporter_jar
    "${DEFOLD_MODELC_BINARY_DIR}/modelimporter.jar")
  list(APPEND _DEFOLD_BOB_LIGHT_JAR_COPY_COMMANDS
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${DEFOLD_MODELC_BINARY_DIR}/modelimporter.jar"
      "${DEFOLD_SDK_ROOT}/share/java/modelimporter.jar")
endif()
if(TARGET font_renderer_jar)
  list(APPEND _DEFOLD_BOB_LIGHT_DEPENDS
    font_renderer_jar
    "${DEFOLD_FONT_BINARY_DIR}/fontrenderer.jar")
  list(APPEND _DEFOLD_BOB_LIGHT_JAR_COPY_COMMANDS
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${DEFOLD_FONT_BINARY_DIR}/fontrenderer.jar"
      "${DEFOLD_SDK_ROOT}/share/java/fontrenderer.jar")
endif()
if(TARGET texturecompiler_jar)
  list(APPEND _DEFOLD_BOB_LIGHT_DEPENDS
    texturecompiler_jar
    "${DEFOLD_TEXC_BINARY_DIR}/texturecompiler.jar")
  list(APPEND _DEFOLD_BOB_LIGHT_JAR_COPY_COMMANDS
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${DEFOLD_TEXC_BINARY_DIR}/texturecompiler.jar"
      "${DEFOLD_SDK_ROOT}/share/java/texturecompiler.jar")
endif()
if(TARGET shaderc_jar)
  list(APPEND _DEFOLD_BOB_LIGHT_DEPENDS
    shaderc_jar
    "${DEFOLD_SHADERC_BINARY_DIR}/shaderc.jar")
  list(APPEND _DEFOLD_BOB_LIGHT_JAR_COPY_COMMANDS
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${DEFOLD_SHADERC_BINARY_DIR}/shaderc.jar"
      "${DEFOLD_SDK_ROOT}/share/java/shaderc.jar")
endif()
if(DEFOLD_HOST_TOOLS_TARGET)
  list(APPEND _DEFOLD_BOB_LIGHT_DEPENDS "${DEFOLD_HOST_TOOLS_TARGET}")
endif()

if(NOT DEFOLD_SKIP_BOB_LIGHT)
  add_custom_command(
    OUTPUT "${DEFOLD_BOB_LIGHT_JAR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${DEFOLD_LIB_DIR}"
    ${_DEFOLD_BOB_LIGHT_COPY_COMMANDS}
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${DEFOLD_SDK_ROOT}/share/java"
    ${_DEFOLD_BOB_LIGHT_JAR_COPY_COMMANDS}
    COMMAND "${CMAKE_COMMAND}" -E env
      "DEFOLD_HOME=${DEFOLD_HOME}"
      "DYNAMO_HOME=${DEFOLD_SDK_ROOT}"
      "JAVA_HOME=${DEFOLD_JAVA_HOME}"
      "GRADLE_OPTS=-Dorg.gradle.parallel=true --sun-misc-unsafe-memory-access=allow --enable-native-access=ALL-UNNAMED"
      "${DEFOLD_GRADLEW}" -Pkeep-bob-uncompressed "-Ptarget-platform=${TARGET_PLATFORM}" "-Ppython=${Python3_EXECUTABLE}" installBobLight
    WORKING_DIRECTORY "${DEFOLD_BOB_DIR}"
    DEPENDS ${_DEFOLD_BOB_LIGHT_DEPENDS}
    COMMENT "Building bob-light.jar"
    VERBATIM)
endif()
unset(_DEFOLD_BOB_LIGHT_DEPENDS)
unset(_DEFOLD_BOB_LIGHT_COPY_COMMANDS)
unset(_DEFOLD_BOB_LIGHT_JAR_COPY_COMMANDS)
unset(_DEFOLD_BOB_LIGHT_LIB)

defold_xcode_organize_project()
defold_finalize_sequential_run_tests()

# ===================== CMake package export =====================
# Export defold_sdk so external projects can find_package(Defold)
include(CMakePackageConfigHelpers)

# Determine Defold version from VERSION file if available
set(DEFOLD_VERSION "0.0.0")
if(EXISTS "${DEFOLD_HOME}/VERSION")
  file(READ "${DEFOLD_HOME}/VERSION" _DEFOLD_VER_RAW)
  string(STRIP "${_DEFOLD_VER_RAW}" DEFOLD_VERSION)
endif()

set(_DEFOLD_CONFIG_INSTALL_DIR "lib/cmake/Defold")

install(TARGETS defold_sdk EXPORT DefoldTargets)
install(EXPORT DefoldTargets
        NAMESPACE Defold::
        DESTINATION ${_DEFOLD_CONFIG_INSTALL_DIR})

configure_package_config_file(
  "${CMAKE_CURRENT_LIST_DIR}/scripts/cmake/DefoldConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/DefoldConfig.cmake"
  INSTALL_DESTINATION ${_DEFOLD_CONFIG_INSTALL_DIR}
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/DefoldConfigVersion.cmake"
  VERSION "${DEFOLD_VERSION}"
  COMPATIBILITY AnyNewerVersion
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/DefoldConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/DefoldConfigVersion.cmake"
  DESTINATION ${_DEFOLD_CONFIG_INSTALL_DIR}
)
