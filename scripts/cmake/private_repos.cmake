# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

if(DEFINED DEFOLD_PRIVATE_REPOS_CMAKE_INCLUDED)
  return()
endif()
set(DEFOLD_PRIVATE_REPOS_CMAKE_INCLUDED ON)

# Loads optional private repository paths from <defold>/.defold-platforms.
#
# Expected file format:
# {
#   "arm64-nx64": {
#     "root": "/path/to/defold-switch"
#   }
# }
#
# Exported variables:
#   DEFOLD_PRIVATE_REPO_ROOTS
#       All existing private repository paths across configured platforms.
#   DEFOLD_PRIVATE_REPO_ROOT_<PLATFORM>
#       Existing root for a platform, where <PLATFORM> is upper-case and
#       '-' is replaced by '_', e.g. DEFOLD_PRIVATE_REPO_ROOT_ARM64_NX64.
#
# Each existing <root>/scripts/cmake directory is appended to CMAKE_MODULE_PATH.

if(NOT DEFINED DEFOLD_HOME)
  get_filename_component(DEFOLD_HOME "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

function(_defold_private_platform_var OUT_VAR PLATFORM)
  string(TOUPPER "${PLATFORM}" _platform_var)
  string(REPLACE "-" "_" _platform_var "${_platform_var}")
  set(${OUT_VAR} "${_platform_var}" PARENT_SCOPE)
endfunction()

function(_defold_private_normalize_root OUT_VAR ROOT)
  if(NOT ROOT)
    set(${OUT_VAR} "" PARENT_SCOPE)
    return()
  endif()

  string(REPLACE "\\\\" "/" _root "${ROOT}")
  string(REPLACE "\\" "/" _root "${_root}")
  file(TO_CMAKE_PATH "${_root}" _root)

  if(EXISTS "${_root}")
    set(${OUT_VAR} "${_root}" PARENT_SCOPE)
  else()
    set(${OUT_VAR} "" PARENT_SCOPE)
  endif()
endfunction()

function(_defold_private_add_cmake_module_path ROOT)
  set(_cmake_dir "${ROOT}/scripts/cmake")
  if(EXISTS "${_cmake_dir}")
    list(FIND CMAKE_MODULE_PATH "${_cmake_dir}" _cmake_dir_index)
    if(_cmake_dir_index EQUAL -1)
      list(APPEND CMAKE_MODULE_PATH "${_cmake_dir}")
      set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)
    endif()
  endif()
endfunction()

function(defold_get_private_repo_root OUT_VAR PLATFORM)
  _defold_private_platform_var(_platform_var "${PLATFORM}")
  set(${OUT_VAR} "${DEFOLD_PRIVATE_REPO_ROOT_${_platform_var}}" PARENT_SCOPE)
endfunction()

function(_defold_private_clear_cached_roots)
  get_cmake_property(_cache_vars CACHE_VARIABLES)
  foreach(_cache_var IN LISTS _cache_vars)
    if(_cache_var MATCHES "^DEFOLD_PRIVATE_REPO_ROOT_[A-Z0-9_]+$")
      unset(${_cache_var} CACHE)
    endif()
  endforeach()
endfunction()

_defold_private_clear_cached_roots()

set(DEFOLD_PRIVATE_REPO_ROOTS "")
set(_DEFOLD_PRIVATE_PLATFORMS_CONFIG "${DEFOLD_HOME}/.defold-platforms")

if(EXISTS "${_DEFOLD_PRIVATE_PLATFORMS_CONFIG}")
  file(READ "${_DEFOLD_PRIVATE_PLATFORMS_CONFIG}" _DEFOLD_PRIVATE_PLATFORMS_JSON)
  string(JSON _private_platform_count ERROR_VARIABLE _json_error LENGTH "${_DEFOLD_PRIVATE_PLATFORMS_JSON}")

  if(_json_error)
    message(FATAL_ERROR "Could not parse ${_DEFOLD_PRIVATE_PLATFORMS_CONFIG}: ${_json_error}")
  endif()

  if(_private_platform_count GREATER 0)
    math(EXPR _private_platform_last "${_private_platform_count} - 1")
    foreach(_platform_index RANGE 0 ${_private_platform_last})
      string(JSON _private_platform MEMBER "${_DEFOLD_PRIVATE_PLATFORMS_JSON}" ${_platform_index})

      string(JSON _root_type ERROR_VARIABLE _json_error TYPE "${_DEFOLD_PRIVATE_PLATFORMS_JSON}" "${_private_platform}" root)
      if(NOT _json_error AND _root_type STREQUAL "STRING")
        string(JSON _private_root GET "${_DEFOLD_PRIVATE_PLATFORMS_JSON}" "${_private_platform}" root)
        _defold_private_normalize_root(_private_root "${_private_root}")
      else()
        set(_private_root "")
      endif()

      if(_private_root)
        list(APPEND DEFOLD_PRIVATE_REPO_ROOTS "${_private_root}")

        _defold_private_platform_var(_private_platform_var "${_private_platform}")
        set("DEFOLD_PRIVATE_REPO_ROOT_${_private_platform_var}" "${_private_root}" CACHE PATH "Private repository root for ${_private_platform}" FORCE)
        _defold_private_add_cmake_module_path("${_private_root}")
      endif()
    endforeach()
  endif()

  list(REMOVE_DUPLICATES DEFOLD_PRIVATE_REPO_ROOTS)
endif()
