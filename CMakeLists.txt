cmake_minimum_required(VERSION 4.0)

# Top-level CMake for Defold libs

# Set module path and run SDK detection early so toolchains (e.g., Emscripten)
# can set CMAKE_TOOLCHAIN_FILE before compilers are enabled by project().
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/scripts/cmake")
include(defold)

# we rename this project in build.py when we generate the solution
project(defold_libraries LANGUAGES ${DEFOLD_LANGUAGE_LIST})

# Load full Defold CMake (platform flags, interface target, etc.)
include("${CMAKE_CURRENT_LIST_DIR}/scripts/cmake/defold.cmake")

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
  font
  graphics
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
  shaderc)
set(DEFOLD_ENGINE_LIB_SET "all" CACHE STRING "Defold engine lib set to configure: all or host")
set(DEFOLD_SELECTED_ENGINE_LIBS "" CACHE STRING "Optional semicolon or comma separated lib override")
if(DEFOLD_SELECTED_ENGINE_LIBS)
  string(REPLACE "," ";" DEFOLD_SELECTED_ENGINE_LIBS "${DEFOLD_SELECTED_ENGINE_LIBS}")
elseif(DEFOLD_ENGINE_LIB_SET STREQUAL "all")
  set(DEFOLD_SELECTED_ENGINE_LIBS ${DEFOLD_ENGINE_LIBS})
elseif(DEFOLD_ENGINE_LIB_SET STREQUAL "host")
  set(DEFOLD_SELECTED_ENGINE_LIBS ${DEFOLD_HOST_LIBS})
else()
  message(FATAL_ERROR "Unknown DEFOLD_ENGINE_LIB_SET: ${DEFOLD_ENGINE_LIB_SET}")
endif()

message(STATUS "Defold engine libs: ${DEFOLD_SELECTED_ENGINE_LIBS}")
foreach(_DEFOLD_ENGINE_LIB IN LISTS DEFOLD_SELECTED_ENGINE_LIBS)
  set(_DEFOLD_ENGINE_LIB_BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/engine/${_DEFOLD_ENGINE_LIB}/build/${TARGET_PLATFORM}")
  string(TOUPPER "${_DEFOLD_ENGINE_LIB}" _DEFOLD_ENGINE_LIB_UPPER)
  set("DEFOLD_${_DEFOLD_ENGINE_LIB_UPPER}_BINARY_DIR" "${_DEFOLD_ENGINE_LIB_BINARY_DIR}" CACHE INTERNAL "Binary directory for engine/${_DEFOLD_ENGINE_LIB}")
  add_subdirectory("engine/${_DEFOLD_ENGINE_LIB}" "${_DEFOLD_ENGINE_LIB_BINARY_DIR}")
endforeach()
unset(_DEFOLD_ENGINE_LIB)
unset(_DEFOLD_ENGINE_LIB_BINARY_DIR)
unset(_DEFOLD_ENGINE_LIB_UPPER)

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
