cmake_minimum_required(VERSION 4.0)

project(defold_extender LANGUAGES NONE)

set(TARGET_PLATFORM "" CACHE STRING "Defold platform tuple")
set(BUILD_TESTS OFF CACHE BOOL "Build tests")
set(CMAKE_INSTALL_PREFIX "$ENV{DYNAMO_HOME}" CACHE PATH "Install prefix" FORCE)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(EXTENDER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(EXTENDER_MERGE_SCRIPT "${EXTENDER_ROOT}/scripts/merge_extender_config.py")
set(EXTENDER_PRIVATE_ROOT "")
if(DEFOLD_XBONE_PRIVATE_REPO_ROOT AND EXISTS "${DEFOLD_XBONE_PRIVATE_REPO_ROOT}/share/extender")
  set(EXTENDER_PRIVATE_ROOT "${DEFOLD_XBONE_PRIVATE_REPO_ROOT}/share/extender")
elseif(DEFOLD_PRIVATE_REPO_ROOT AND EXISTS "${DEFOLD_PRIVATE_REPO_ROOT}/share/extender")
  set(EXTENDER_PRIVATE_ROOT "${DEFOLD_PRIVATE_REPO_ROOT}/share/extender")
elseif(COMMAND defold_get_private_repo_root AND TARGET_PLATFORM)
  defold_get_private_repo_root(_extender_private_repo_root "${TARGET_PLATFORM}")
  if(_extender_private_repo_root AND EXISTS "${_extender_private_repo_root}/share/extender")
    set(EXTENDER_PRIVATE_ROOT "${_extender_private_repo_root}/share/extender")
  endif()
endif()

set(EXTENDER_PLATFORM_TAG "")
if(TARGET_PLATFORM MATCHES "-([^-]+)$")
  set(EXTENDER_PLATFORM_TAG "${CMAKE_MATCH_1}")
elseif(TARGET_PLATFORM)
  set(EXTENDER_PLATFORM_TAG "${TARGET_PLATFORM}")
endif()

set(EXTENDER_SOURCE_GROUP "share\\extender")
set(EXTENDER_VARIANTS_SOURCE_GROUP "share\\extender\\variants")
set(EXTENDER_SOURCE_FILES)

function(defold_extender_add_source_group group)
  foreach(_file IN LISTS ARGN)
    if(EXISTS "${_file}" OR IS_ABSOLUTE "${_file}")
      list(APPEND EXTENDER_SOURCE_FILES "${_file}")
      source_group("${group}" FILES "${_file}")
    endif()
  endforeach()
  set(EXTENDER_SOURCE_FILES "${EXTENDER_SOURCE_FILES}" PARENT_SCOPE)
endfunction()

function(defold_extender_optional_inputs out_var group)
  set(_inputs)
  set(_source_files ${EXTENDER_SOURCE_FILES})
  foreach(_file IN LISTS ARGN)
    if(EXISTS "${_file}")
      list(APPEND _inputs "${_file}")
      list(APPEND _source_files "${_file}")
      source_group("${group}" FILES "${_file}")
    endif()
  endforeach()
  set(EXTENDER_SOURCE_FILES "${_source_files}" PARENT_SCOPE)
  set(${out_var} "${_inputs}" PARENT_SCOPE)
endfunction()

function(defold_extender_merge output)
  set(options)
  set(oneValueArgs)
  set(multiValueArgs INPUTS OPTIONAL_INPUTS)
  cmake_parse_arguments(DEM "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT DEM_INPUTS)
    message(FATAL_ERROR "defold_extender_merge requires INPUTS")
  endif()

  set(_args --output "${output}")
  set(_existing_optional_inputs)
  foreach(_input IN LISTS DEM_INPUTS)
    list(APPEND _args --input "${_input}")
  endforeach()
  foreach(_input IN LISTS DEM_OPTIONAL_INPUTS)
    list(APPEND _args --optional-input "${_input}")
    if(EXISTS "${_input}")
      list(APPEND _existing_optional_inputs "${_input}")
    endif()
  endforeach()

  add_custom_command(
    OUTPUT "${output}"
    COMMAND "${Python3_EXECUTABLE}" "${EXTENDER_MERGE_SCRIPT}" ${_args}
    DEPENDS "${EXTENDER_MERGE_SCRIPT}" ${DEM_INPUTS} ${_existing_optional_inputs}
    COMMENT "Generating ${output}"
    VERBATIM)
endfunction()

set(EXTENDER_BUILD_YML "${CMAKE_CURRENT_BINARY_DIR}/build.yml")
defold_extender_add_source_group("${EXTENDER_SOURCE_GROUP}" "${EXTENDER_ROOT}/build_input.yml")
defold_extender_optional_inputs(EXTENDER_BUILD_OPTIONAL_INPUTS "${EXTENDER_SOURCE_GROUP}"
  "${EXTENDER_ROOT}/build_vendor.yml"
  "${EXTENDER_PRIVATE_ROOT}/build_${EXTENDER_PLATFORM_TAG}.yml")

defold_extender_merge("${EXTENDER_BUILD_YML}"
  INPUTS "${EXTENDER_ROOT}/build_input.yml"
  OPTIONAL_INPUTS ${EXTENDER_BUILD_OPTIONAL_INPUTS})

set(EXTENDER_APPMANIFESTS)
foreach(_variant IN ITEMS debug release headless)
  set(_output "${CMAKE_CURRENT_BINARY_DIR}/variants/${_variant}.appmanifest")
  defold_extender_add_source_group("${EXTENDER_VARIANTS_SOURCE_GROUP}" "${EXTENDER_ROOT}/variants/${_variant}_input.appmanifest")
  defold_extender_optional_inputs(_variant_optional_inputs "${EXTENDER_VARIANTS_SOURCE_GROUP}"
    "${EXTENDER_ROOT}/variants/${_variant}_vendor.appmanifest"
    "${EXTENDER_PRIVATE_ROOT}/variants/${_variant}_${EXTENDER_PLATFORM_TAG}.appmanifest")
  defold_extender_merge("${_output}"
    INPUTS "${EXTENDER_ROOT}/variants/${_variant}_input.appmanifest"
    OPTIONAL_INPUTS ${_variant_optional_inputs})
  list(APPEND EXTENDER_APPMANIFESTS "${_output}")
endforeach()

add_custom_target(extender ALL
  DEPENDS "${EXTENDER_BUILD_YML}" ${EXTENDER_APPMANIFESTS}
  SOURCES ${EXTENDER_SOURCE_FILES})

install(FILES "${EXTENDER_BUILD_YML}" DESTINATION extender)
install(FILES ${EXTENDER_APPMANIFESTS} DESTINATION extender/variants)
