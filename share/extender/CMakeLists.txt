cmake_minimum_required(VERSION 4.0)

project(defold_extender LANGUAGES NONE)

set(TARGET_PLATFORM "" CACHE STRING "Defold platform tuple")
set(BUILD_TESTS OFF CACHE BOOL "Build tests")
set(CMAKE_INSTALL_PREFIX "$ENV{DYNAMO_HOME}" CACHE PATH "Install prefix" FORCE)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(EXTENDER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(EXTENDER_MERGE_SCRIPT "${EXTENDER_ROOT}/scripts/merge_extender_config.py")

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
defold_extender_merge("${EXTENDER_BUILD_YML}"
  INPUTS "${EXTENDER_ROOT}/build_input.yml"
  OPTIONAL_INPUTS "${EXTENDER_ROOT}/build_vendor.yml")

set(EXTENDER_APPMANIFESTS)
foreach(_variant IN ITEMS debug release headless)
  set(_output "${CMAKE_CURRENT_BINARY_DIR}/variants/${_variant}.appmanifest")
  defold_extender_merge("${_output}"
    INPUTS "${EXTENDER_ROOT}/variants/${_variant}_input.appmanifest"
    OPTIONAL_INPUTS "${EXTENDER_ROOT}/variants/${_variant}_vendor.appmanifest")
  list(APPEND EXTENDER_APPMANIFESTS "${_output}")
endforeach()

add_custom_target(extender ALL
  DEPENDS "${EXTENDER_BUILD_YML}" ${EXTENDER_APPMANIFESTS})

install(FILES "${EXTENDER_BUILD_YML}" DESTINATION extender)
install(FILES ${EXTENDER_APPMANIFESTS} DESTINATION extender/variants)
