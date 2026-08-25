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

# Selects the platform-specific source files for a feature directory.
#
# The feature source layout follows the legacy Waf selection rules: all feature
# implementation files under BASE_DIR are returned in OUT_ALL, while OUT_SELECTED
# contains the files that should be built for FEATURE on PLATFORM. The selection
# is delegated to select_feature_sources.py so the same filename/tag matching can
# be shared across CMake modules and private repositories.
#
# Usage:
#   defold_select_feature_sources(<out_selected> <out_all>
#     BASE_DIR <path>
#     FEATURE <name>
#     PLATFORM <target-platform>
#     [PRIVATE_ROOT <repo-root>]
#     [EXTRA_TAGS <tag>...]
#     [PREFERRED_TAGS <tag>...])
#
# Arguments:
#   OUT_SELECTED
#     Name of the variable that receives the selected source files.
#   OUT_ALL
#     Name of the variable that receives all known source files for the feature.
#   BASE_DIR
#     Directory containing the public feature implementation files.
#   FEATURE
#     Human-readable feature name used in diagnostics.
#   PLATFORM
#     Defold target platform tuple, e.g. x86_64-win32 or x86_64-xbone.
#   PRIVATE_ROOT
#     Optional private repository root. Matching private feature files are
#     considered together with the public files.
#   EXTRA_TAGS
#     Additional platform tags accepted by the selector.
#   PREFERRED_TAGS
#     Tags that should be preferred when more than one platform-specific source
#     could satisfy the feature.
function(defold_select_feature_sources OUT_SELECTED OUT_ALL)
  set(options)
  set(oneValueArgs BASE_DIR FEATURE PLATFORM PRIVATE_ROOT)
  set(multiValueArgs EXTRA_TAGS PREFERRED_TAGS)
  cmake_parse_arguments(DFS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT DFS_BASE_DIR)
    message(FATAL_ERROR "defold_select_feature_sources: BASE_DIR is required")
  endif()
  if(NOT DFS_FEATURE)
    message(FATAL_ERROR "defold_select_feature_sources: FEATURE is required")
  endif()
  if(NOT DFS_PLATFORM)
    message(FATAL_ERROR "defold_select_feature_sources: PLATFORM is required")
  endif()

  find_package(Python3 COMPONENTS Interpreter REQUIRED)

  set(_cmd
    "${Python3_EXECUTABLE}"
    "${DEFOLD_CMAKE_DIR}/select_feature_sources.py"
    "--repo-root" "${DEFOLD_HOME}"
    "--base-dir" "${DFS_BASE_DIR}"
    "--platform" "${DFS_PLATFORM}"
    "--feature" "${DFS_FEATURE}")

  if(DFS_PRIVATE_ROOT)
    list(APPEND _cmd "--private-root" "${DFS_PRIVATE_ROOT}")
  endif()
  foreach(_tag IN LISTS DFS_EXTRA_TAGS)
    list(APPEND _cmd "--extra-tag" "${_tag}")
  endforeach()
  foreach(_tag IN LISTS DFS_PREFERRED_TAGS)
    list(APPEND _cmd "--preferred-tag" "${_tag}")
  endforeach()

  execute_process(
    COMMAND ${_cmd}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _json
    ERROR_VARIABLE _error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)

  if(NOT _result EQUAL 0)
    message(FATAL_ERROR "Could not select source files for feature ${DFS_FEATURE}: ${_error}")
  endif()

  set(_selected)
  string(JSON _selected_count ERROR_VARIABLE _json_error LENGTH "${_json}" selected)
  if(_json_error)
    message(FATAL_ERROR "Could not parse feature selection for ${DFS_FEATURE}: ${_json_error}\n${_json}")
  endif()
  if(_selected_count GREATER 0)
    math(EXPR _selected_last "${_selected_count} - 1")
    foreach(_index RANGE 0 ${_selected_last})
      string(JSON _path GET "${_json}" selected ${_index})
      file(TO_CMAKE_PATH "${_path}" _path)
      list(APPEND _selected "${_path}")
    endforeach()
  endif()

  set(_all)
  string(JSON _all_count ERROR_VARIABLE _json_error LENGTH "${_json}" all)
  if(_json_error)
    message(FATAL_ERROR "Could not parse feature file list for ${DFS_FEATURE}: ${_json_error}\n${_json}")
  endif()
  if(_all_count GREATER 0)
    math(EXPR _all_last "${_all_count} - 1")
    foreach(_index RANGE 0 ${_all_last})
      string(JSON _path GET "${_json}" all ${_index})
      file(TO_CMAKE_PATH "${_path}" _path)
      list(APPEND _all "${_path}")
    endforeach()
  endif()

  if(NOT _all)
    message(FATAL_ERROR "Could not find any source files for feature ${DFS_FEATURE}")
  endif()
  if(NOT _selected)
    message(FATAL_ERROR "Could not find selected source files for feature ${DFS_FEATURE} on platform ${DFS_PLATFORM}")
  endif()

  set(${OUT_SELECTED} "${_selected}" PARENT_SCOPE)
  set(${OUT_ALL} "${_all}" PARENT_SCOPE)
endfunction()
