message("defold.cmake:")

# Require CMake 4.x in projects that include these modules
if(CMAKE_VERSION VERSION_LESS 4.0)
  message(FATAL_ERROR "Defold CMake scripts require CMake >= 4.0 (found ${CMAKE_VERSION})")
endif()

get_filename_component(DEFOLD_HOME "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# Ensure this directory (scripts/cmake) is on CMAKE_MODULE_PATH once
list(FIND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}" _defold_cmake_idx)
if(_defold_cmake_idx EQUAL -1)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
endif()

set(DEFOLD_SDK_ROOT "$ENV{DYNAMO_HOME}" CACHE PATH "Path to Defold SDK (e.g. defold/tmp/dynamo_home)")
if(NOT DEFOLD_SDK_ROOT)
  if(EXISTS "${DEFOLD_HOME}/tmp/dynamo_home")
    set(DEFOLD_SDK_ROOT "${DEFOLD_HOME}/tmp/dynamo_home" CACHE PATH "Path to Defold SDK (tmp/dynamo_home)" FORCE)
  endif()
endif()

# if(NOT DEFOLD_SDK_ROOT)
#   message(FATAL_ERROR "DEFOLD_SDK_ROOT not set. Run './scripts/build.py shell && ./scripts/build.py install_ext && ./scripts/build.py build_engine' first, or pass -DDEFOLD_SDK_ROOT=/path/to/tmp/dynamo_home")
# endif()

message(STATUS "DEFOLD_HOME: ${DEFOLD_HOME}")
message(STATUS "DEFOLD_SDK_ROOT: ${DEFOLD_SDK_ROOT}")

# Align try_compile configuration with active build configuration
if(CMAKE_CONFIGURATION_TYPES)
  if(NOT CMAKE_TRY_COMPILE_CONFIGURATION)
    if("RelWithDebInfo" IN_LIST CMAKE_CONFIGURATION_TYPES)
      set(CMAKE_TRY_COMPILE_CONFIGURATION RelWithDebInfo)
    elseif("Release" IN_LIST CMAKE_CONFIGURATION_TYPES)
      set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
    else()
      list(GET CMAKE_CONFIGURATION_TYPES 0 _DEFOLD_FIRST_CFG)
      set(CMAKE_TRY_COMPILE_CONFIGURATION "${_DEFOLD_FIRST_CFG}")
    endif()
  endif()
else()
  if(CMAKE_BUILD_TYPE)
    set(CMAKE_TRY_COMPILE_CONFIGURATION "${CMAKE_BUILD_TYPE}")
  endif()
endif()

# Ensure the INTERFACE target exists early so platform modules can attach options
if(NOT TARGET defold_sdk)
  add_library(defold_sdk INTERFACE)
endif()

# verify our list of tools (e.g. java, ninja etc)
include(tools)

# list of toggleable features
include(features)

# platform specific includes, lib paths, defines etc...
include(platform)

#include helper functions
include(functions)

# Common paths
set(DEFOLD_INCLUDE_DIR "${DEFOLD_SDK_ROOT}/include")
set(DEFOLD_EXT_INCLUDE_DIR "${DEFOLD_SDK_ROOT}/ext/include")
set(DEFOLD_EXT_PLATFORM_INCLUDE_DIR "${DEFOLD_SDK_ROOT}/ext/include/${TARGET_PLATFORM}")
set(DEFOLD_DMSDK_INCLUDE_DIR "${DEFOLD_SDK_ROOT}/sdk/include")

set(DEFOLD_LIB_DIR "${DEFOLD_SDK_ROOT}/lib/${TARGET_PLATFORM}")
set(DEFOLD_EXT_LIB_DIR "${DEFOLD_SDK_ROOT}/ext/lib/${TARGET_PLATFORM}")

# For 32-bit Windows, search both legacy 'win32' and tuple 'x86-win32' folders
set(_DEFOLD_PLATFORM_INCLUDE_DIRS "${DEFOLD_EXT_PLATFORM_INCLUDE_DIR}")
set(_DEFOLD_PLATFORM_LIB_DIRS     "${DEFOLD_LIB_DIR}" "${DEFOLD_EXT_LIB_DIR}")
if(TARGET_PLATFORM STREQUAL "x86-win32")
  list(APPEND _DEFOLD_PLATFORM_INCLUDE_DIRS "${DEFOLD_SDK_ROOT}/ext/include/win32")
  list(APPEND _DEFOLD_PLATFORM_LIB_DIRS "${DEFOLD_SDK_ROOT}/lib/win32" "${DEFOLD_SDK_ROOT}/ext/lib/win32")
endif()

if(NOT EXISTS "${DEFOLD_SDK_ROOT}")
  message(FATAL_ERROR "Missing folder ${DEFOLD_SDK_ROOT}")
endif()

# Attach SDK include/lib search paths to defold_sdk INTERFACE
# Core include dirs (non-system)
target_include_directories(defold_sdk INTERFACE
  "${DEFOLD_INCLUDE_DIR}"
  "${DEFOLD_DMSDK_INCLUDE_DIR}")
# External/platform include dirs as SYSTEM to reduce warnings
target_include_directories(defold_sdk SYSTEM INTERFACE
  "${DEFOLD_EXT_INCLUDE_DIR}"
  ${_DEFOLD_PLATFORM_INCLUDE_DIRS})
# Library search directories
target_link_directories(defold_sdk INTERFACE ${_DEFOLD_PLATFORM_LIB_DIRS})

# Enable IPO/LTO when supported
include(CheckIPOSupported)
check_ipo_supported(RESULT _DEFOLD_IPO_OK OUTPUT _DEFOLD_IPO_MSG)
if(_DEFOLD_IPO_OK)
  # Prefer IPO on Release-like configs for targets created after this point
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
  # Enable IPO on the defold_sdk usage target so consumers default to IPO
  # (actual effect applies to real targets compiled/linked)
  set_property(TARGET defold_sdk PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
  message(STATUS "IPO not enabled: ${_DEFOLD_IPO_MSG}")
endif()

# Apply defold_sdk to all targets in this directory and below
link_libraries(defold_sdk)

# Install into DEFOLD_SDK_ROOT
set(CMAKE_INSTALL_PREFIX "${DEFOLD_SDK_ROOT}" CACHE PATH "Install prefix" FORCE)
message(STATUS "Install prefix set to DEFOLD_SDK_ROOT: ${CMAKE_INSTALL_PREFIX}")
