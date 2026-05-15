if(DEFINED DEFOLD_CMAKE_INCLUDED)
  return()
endif()
set(DEFOLD_CMAKE_INCLUDED ON)

message(DEBUG "defold.cmake:")

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

#include helper functions (e.g. defold_log)
include(functions)

# Disable RPATH in build and install to avoid embedding search paths
set(CMAKE_SKIP_BUILD_RPATH ON)
set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)
set(CMAKE_INSTALL_RPATH "")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH OFF)
if(TARGET_PLATFORM MATCHES "macos|ios")
  set(CMAKE_MACOSX_RPATH OFF)
endif()
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)

# Force -std=c++11 (no GNU extensions like gnu++11)
set(CMAKE_CXX_EXTENSIONS OFF)

# Determine which language families we need across the toolchain logic
set(DEFOLD_LANGUAGE_LIST C CXX)
if(TARGET_PLATFORM MATCHES "macos|ios")
  list(APPEND DEFOLD_LANGUAGE_LIST OBJC OBJCXX)
endif()

# Normalise optimisation flags for single-config generators. We strip any
# existing -O* flag before applying the desired level to avoid duplicates when
# re-configuring.
function(_defold_set_opt_flag cfg optflag debugflag)
  string(TOUPPER "${cfg}" _CFG_UP)
  foreach(_lang IN LISTS DEFOLD_LANGUAGE_LIST)
    foreach(_suffix "" "_INIT")
      set(_var "CMAKE_${_lang}_FLAGS_${_CFG_UP}${_suffix}")
      if(DEFINED ${_var})
        set(_flags "${${_var}}")
      else()
        set(_flags "")
      endif()
      string(REGEX REPLACE "-O[0-9s]" "" _flags "${_flags}")
      if(NOT "${debugflag}" STREQUAL "")
        string(REGEX REPLACE "-g([0-9]?)" "" _flags "${_flags}")
      endif()
      string(REGEX REPLACE "  +" " " _flags "${_flags}")
      string(STRIP "${_flags}" _flags)
      set(_final_flags "")
      if(NOT "${optflag}" STREQUAL "")
        set(_final_flags "${optflag}")
      endif()
      if(NOT "${debugflag}" STREQUAL "")
        if(_final_flags STREQUAL "")
          set(_final_flags "${debugflag}")
        else()
          set(_final_flags "${_final_flags} ${debugflag}")
        endif()
      endif()
      if("${CMAKE_${_lang}_COMPILER_ID}" STREQUAL "Clang")
        if(_final_flags STREQUAL "")
          set(_final_flags "-fno-omit-frame-pointer")
        else()
          set(_final_flags "${_final_flags} -fno-omit-frame-pointer")
        endif()
      endif()

      if(NOT _flags STREQUAL "")
        if(_final_flags STREQUAL "")
          set(_final_flags "${_flags}")
        else()
          set(_final_flags "${_flags} ${_final_flags}")
        endif()
      endif()

      if(_suffix STREQUAL "")
        set(${_var} "${_final_flags}" CACHE STRING "${_var}" FORCE)
      else()
        set(${_var} "${_final_flags}")
      endif()
    endforeach()
  endforeach()
endfunction()

set(_DEFOLD_RELWITHDEBINFO_OPT "-O2")
if(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
  set(_DEFOLD_RELWITHDEBINFO_OPT "-O3")
endif()

_defold_set_opt_flag(Debug "-O0" "-g")
_defold_set_opt_flag(Release "-O2" "-g")
_defold_set_opt_flag(RelWithDebInfo "${_DEFOLD_RELWITHDEBINFO_OPT}" "-g")
_defold_set_opt_flag(MinSizeRel "-Os" "-g")

# Prime Release-like flag variables before CMake enables the toolchains so
# /DNDEBUG or -DNDEBUG never sneak into optimised builds. We update both the
# cached values and their *_INIT counterparts; the post-project hook keeps a
# second layer of defence after project() runs.
set(_DEFOLD_OPT_CONFIGS Release RelWithDebInfo MinSizeRel)
foreach(_cfg IN LISTS _DEFOLD_OPT_CONFIGS)
  string(TOUPPER "${_cfg}" _CFG_UP)
  foreach(_lang IN LISTS DEFOLD_LANGUAGE_LIST)
    set(_cache_var "CMAKE_${_lang}_FLAGS_${_CFG_UP}")
    set(_init_var  "CMAKE_${_lang}_FLAGS_${_CFG_UP}_INIT")

    set(_flags "")
    if(DEFINED ${_cache_var})
      set(_flags "${${_cache_var}}")
    elseif(DEFINED ${_init_var})
      set(_flags "${${_init_var}}")
    endif()

    set(${_cache_var} "${_flags}" CACHE STRING "${_cache_var}" FORCE)
    set(${_init_var} "${_flags}")
  endforeach()
endforeach()

# Arrange for a post-project hook that scrubs /DNDEBUG/-DNDEBUG from the
# optimised configuration flags after CMake has enabled the language
# toolchains. This keeps asserts enabled in Release/RelWithDebInfo/MinSizeRel
# builds.
if(NOT DEFINED CMAKE_PROJECT_TOP_LEVEL_INCLUDES)
  set(CMAKE_PROJECT_TOP_LEVEL_INCLUDES "")
endif()
list(APPEND CMAKE_PROJECT_TOP_LEVEL_INCLUDES "${DEFOLD_CMAKE_DIR}/defold_post_project.cmake")

defold_log("DEFOLD_HOME: ${DEFOLD_HOME}")
defold_log("DEFOLD_SDK_ROOT: ${DEFOLD_SDK_ROOT}")

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

# Prefer colored diagnostics from compilers that support it
set(CMAKE_COLOR_DIAGNOSTICS ON)

# Export compilation database for tooling (clangd, IDEs)
# -- currently set to off, as it currently interferes with the Waf option
set(CMAKE_EXPORT_COMPILE_COMMANDS OFF)

# Common paths
set(DEFOLD_INCLUDE_DIR "${DEFOLD_SDK_ROOT}/include")
set(DEFOLD_EXT_INCLUDE_DIR "${DEFOLD_SDK_ROOT}/ext/include")
set(DEFOLD_EXT_PLATFORM_INCLUDE_DIR "${DEFOLD_SDK_ROOT}/ext/include/${TARGET_PLATFORM}")
set(DEFOLD_DMSDK_INCLUDE_DIR "${DEFOLD_SDK_ROOT}/sdk/include")

file(GLOB _DEFOLD_DMSDK_DIRS CONFIGURE_DEPENDS "${DEFOLD_HOME}/engine/*/src/dmsdk")
set(DEFOLD_DMSDK_SOURCE_INCLUDE_DIRS)
foreach(_DEFOLD_DMSDK_DIR IN LISTS _DEFOLD_DMSDK_DIRS)
  get_filename_component(_DEFOLD_DMSDK_SOURCE_INCLUDE_DIR "${_DEFOLD_DMSDK_DIR}/.." ABSOLUTE)
  list(APPEND DEFOLD_DMSDK_SOURCE_INCLUDE_DIRS "${_DEFOLD_DMSDK_SOURCE_INCLUDE_DIR}")
endforeach()
list(REMOVE_DUPLICATES DEFOLD_DMSDK_SOURCE_INCLUDE_DIRS)

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
  "$<BUILD_INTERFACE:${DEFOLD_INCLUDE_DIR}>"
  "$<BUILD_INTERFACE:${DEFOLD_DMSDK_INCLUDE_DIR}>"
  "$<INSTALL_INTERFACE:include>"
  "$<INSTALL_INTERFACE:sdk/include>")
foreach(_DEFOLD_DMSDK_SOURCE_INCLUDE_DIR IN LISTS DEFOLD_DMSDK_SOURCE_INCLUDE_DIRS)
  target_include_directories(defold_sdk INTERFACE
    "$<BUILD_INTERFACE:${_DEFOLD_DMSDK_SOURCE_INCLUDE_DIR}>")
endforeach()
# External/platform include dirs as SYSTEM to reduce warnings
target_include_directories(defold_sdk SYSTEM INTERFACE
  "$<BUILD_INTERFACE:${DEFOLD_EXT_INCLUDE_DIR}>"
  "$<INSTALL_INTERFACE:ext/include>")
foreach(_DEFOLD_PLATFORM_INCLUDE_DIR IN LISTS _DEFOLD_PLATFORM_INCLUDE_DIRS)
  target_include_directories(defold_sdk SYSTEM INTERFACE
    "$<BUILD_INTERFACE:${_DEFOLD_PLATFORM_INCLUDE_DIR}>")
endforeach()
target_include_directories(defold_sdk SYSTEM INTERFACE
  "$<INSTALL_INTERFACE:ext/include/${TARGET_PLATFORM}>")
if(TARGET_PLATFORM STREQUAL "x86-win32")
  target_include_directories(defold_sdk SYSTEM INTERFACE
    "$<INSTALL_INTERFACE:ext/include/win32>")
endif()
# Library search directories
foreach(_DEFOLD_PLATFORM_LIB_DIR IN LISTS _DEFOLD_PLATFORM_LIB_DIRS)
  target_link_directories(defold_sdk INTERFACE
    "$<BUILD_INTERFACE:${_DEFOLD_PLATFORM_LIB_DIR}>")
endforeach()
target_link_directories(defold_sdk INTERFACE
  "$<INSTALL_INTERFACE:lib/${TARGET_PLATFORM}>"
  "$<INSTALL_INTERFACE:ext/lib/${TARGET_PLATFORM}>")
if(TARGET_PLATFORM STREQUAL "x86-win32")
  target_link_directories(defold_sdk INTERFACE
    "$<INSTALL_INTERFACE:lib/win32>"
    "$<INSTALL_INTERFACE:ext/lib/win32>")
endif()

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
  defold_log("IPO not enabled: ${_DEFOLD_IPO_MSG}")
endif()

# Apply defold_sdk to all targets in this directory and below
link_libraries(defold_sdk)

# Install into DEFOLD_SDK_ROOT
set(CMAKE_INSTALL_PREFIX "${DEFOLD_SDK_ROOT}" CACHE PATH "Install prefix" FORCE)
defold_log("Install prefix set to DEFOLD_SDK_ROOT: ${CMAKE_INSTALL_PREFIX}")

# CMake libraries may be built before every Waf library has populated its
# dmsdk compatibility headers. Mirror Waf's dmsdk_add_files convention here.
foreach(_DEFOLD_DMSDK_DIR IN LISTS _DEFOLD_DMSDK_DIRS)
  file(GLOB_RECURSE _DEFOLD_DMSDK_HEADERS CONFIGURE_DEPENDS
       RELATIVE "${_DEFOLD_DMSDK_DIR}"
       "${_DEFOLD_DMSDK_DIR}/*.h"
       "${_DEFOLD_DMSDK_DIR}/*.hpp")
  foreach(_DEFOLD_DMSDK_HEADER IN LISTS _DEFOLD_DMSDK_HEADERS)
    get_filename_component(_DEFOLD_DMSDK_HEADER_DIR "${_DEFOLD_DMSDK_HEADER}" DIRECTORY)
    if(_DEFOLD_DMSDK_HEADER_DIR)
      install(FILES "${_DEFOLD_DMSDK_DIR}/${_DEFOLD_DMSDK_HEADER}"
              DESTINATION "sdk/include/dmsdk/${_DEFOLD_DMSDK_HEADER_DIR}")
    else()
      install(FILES "${_DEFOLD_DMSDK_DIR}/${_DEFOLD_DMSDK_HEADER}"
              DESTINATION sdk/include/dmsdk)
    endif()
  endforeach()
endforeach()
