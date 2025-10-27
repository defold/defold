cmake_minimum_required(VERSION 4.0)

# Top-level CMake for Defold components (hid, input)

# Set module path and run SDK detection early so toolchains (e.g., Emscripten)
# can set CMAKE_TOOLCHAIN_FILE before compilers are enabled by project().
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/scripts/cmake")
include(defold)

# we rename this project in build.py when we generate the solution
project(defold_libraries LANGUAGES ${DEFOLD_LANGUAGE_LIST})

# Load full Defold CMake (platform flags, interface target, etc.)
include("${CMAKE_CURRENT_LIST_DIR}/scripts/cmake/defold.cmake")

# Add subprojects
add_subdirectory(engine/platform)
add_subdirectory(engine/hid)
add_subdirectory(engine/input)

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
