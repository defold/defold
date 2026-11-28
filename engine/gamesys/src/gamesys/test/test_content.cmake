function(gamesys_read_manifest out_var manifest)
  if(NOT EXISTS "${manifest}")
    message(FATAL_ERROR "Missing gamesys test manifest: ${manifest}")
  endif()

  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${manifest}")
  file(STRINGS "${manifest}" _manifest_lines)

  set(_entries)
  foreach(_line IN LISTS _manifest_lines)
    string(REGEX REPLACE "#.*$" "" _entry "${_line}")
    string(STRIP "${_entry}" _entry)
    if(NOT _entry STREQUAL "")
      list(APPEND _entries "${_entry}")
    endif()
  endforeach()

  set(${out_var} ${_entries} PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# Test content setup
# Build the runtime test content tree under the platform build root.
# ---------------------------------------------------------------------------
set(_GS_BOB_LIGHT "${DEFOLD_SDK_ROOT}/share/java/bob-light.jar")

set(_GS_BOB_PLUGIN_JARS)
if(COMMAND defold_append_platform_bob_plugin_jars)
  defold_append_platform_bob_plugin_jars(_GS_BOB_PLUGIN_JARS)
endif()
set(_GS_BOB_CLASSPATH_LIST "${_GS_BOB_LIGHT}" ${_GS_BOB_PLUGIN_JARS})
set(_GS_BUILTINS_ZIP "${DEFOLD_SDK_ROOT}/share/builtins.zip")
if(EXISTS "${_GS_BUILTINS_ZIP}")
  list(APPEND _GS_BOB_CLASSPATH_LIST "${_GS_BUILTINS_ZIP}")
endif()
set(_GS_SHADER_ENV)
if(COMMAND defold_get_platform_shader_env)
  defold_get_platform_shader_env(_GS_SHADER_ENV)
endif()

set(_GS_TEST_DATA_FOLDERS_FILE "${GS_TEST_ROOT}/test_data_folders.txt")
gamesys_read_manifest(_GS_TEST_DATA_FOLDERS "${_GS_TEST_DATA_FOLDERS_FILE}")
if(NOT _GS_TEST_DATA_FOLDERS)
  message(FATAL_ERROR "No gamesys test data folders listed in ${_GS_TEST_DATA_FOLDERS_FILE}")
endif()

set(_GS_SHARED_TEST_SOURCES)
foreach(_shared_dir IN ITEMS lua)
  if(EXISTS "${GS_TEST_ROOT}/${_shared_dir}")
    file(GLOB_RECURSE _shared_sources CONFIGURE_DEPENDS LIST_DIRECTORIES false "${GS_TEST_ROOT}/${_shared_dir}/*")
    list(APPEND _GS_SHARED_TEST_SOURCES ${_shared_sources})
  endif()
endforeach()

file(GLOB_RECURSE _GS_ALL_TEST_SOURCES CONFIGURE_DEPENDS LIST_DIRECTORIES false "${GS_TEST_ROOT}/*")
list(FILTER _GS_ALL_TEST_SOURCES EXCLUDE REGEX "/build/|/__pycache__/")
list(FILTER _GS_ALL_TEST_SOURCES EXCLUDE REGEX "\\.prebuilt_")
list(FILTER _GS_ALL_TEST_SOURCES EXCLUDE REGEX "\\.DS_Store$")
list(FILTER _GS_ALL_TEST_SOURCES EXCLUDE REGEX "/CMakeLists\\.txt$")
list(FILTER _GS_ALL_TEST_SOURCES EXCLUDE REGEX "/build_test_data\\.py$")
list(FILTER _GS_ALL_TEST_SOURCES EXCLUDE REGEX "\\.(cpp|h)$")

set(_GS_COMMON_INPUTS_FILE "${GS_TEST_ROOT}/common_build.inputs")
if(NOT EXISTS "${_GS_COMMON_INPUTS_FILE}")
  message(FATAL_ERROR "Missing gamesys common test data input list: ${_GS_COMMON_INPUTS_FILE}")
endif()

set(_GS_BUILTINS_GRAPHICS_DIR "${GS_ENGINE_CONTENT_DIR}/builtins/graphics")
if(NOT EXISTS "${_GS_BUILTINS_GRAPHICS_DIR}/default.texture_profiles")
  message(FATAL_ERROR "Missing gamesys builtins graphics test dependency: ${_GS_BUILTINS_GRAPHICS_DIR}/default.texture_profiles")
endif()
file(GLOB_RECURSE _GS_BUILTINS_GRAPHICS_SOURCES CONFIGURE_DEPENDS LIST_DIRECTORIES false "${_GS_BUILTINS_GRAPHICS_DIR}/*")

# Generated glTF materials need current shaders before builtins.zip is produced.
set(_GS_BUILTINS_MATERIALS_DIR "${GS_ENGINE_CONTENT_DIR}/builtins/materials")
file(GLOB_RECURSE _GS_BUILTINS_MATERIAL_SOURCES CONFIGURE_DEPENDS LIST_DIRECTORIES false "${_GS_BUILTINS_MATERIALS_DIR}/*")

# Font tests need current builtins before builtins.zip is produced on a clean build.
set(_GS_BUILTINS_FONTS_DIR "${GS_ENGINE_CONTENT_DIR}/builtins/fonts")
get_filename_component(_GS_FONT_SHADERS_DIR "${GS_ENGINE_CONTENT_DIR}/../../font/content/builtins/fonts" ABSOLUTE)
file(GLOB_RECURSE _GS_BUILTINS_FONT_SOURCES CONFIGURE_DEPENDS LIST_DIRECTORIES false
  "${_GS_BUILTINS_FONTS_DIR}/*"
  "${_GS_FONT_SHADERS_DIR}/*")

if(DEFINED ENV{DM_BOB_ROOTFOLDER} AND NOT "$ENV{DM_BOB_ROOTFOLDER}" STREQUAL "")
  set(_GS_BOB_ROOT_BASE "$ENV{DM_BOB_ROOTFOLDER}/gamesys")
else()
  set(_GS_BOB_ROOT_BASE "${GS_TEST_RUNTIME_DIR}/.bob/bob-root")
endif()

# Each Bob process owns its project metadata and extracted tools. Limit the
# number of JVMs competing with compilation for memory on CI runners.
set(_GS_BOB_THREAD_ARGS)
if(DEFINED ENV{GITHUB_WORKFLOW})
  set(DEFOLD_GAMESYS_BOB_THREADS 2 CACHE STRING "Maximum worker threads per gamesys test content Bob process in CI")
  if(NOT DEFOLD_GAMESYS_BOB_THREADS MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "DEFOLD_GAMESYS_BOB_THREADS must be a positive integer")
  endif()
  list(APPEND _GS_BOB_THREAD_ARGS --max-cpu-threads "${DEFOLD_GAMESYS_BOB_THREADS}")
endif()
set(_GS_CONTENT_JOB_POOL)
set(_GS_CONTENT_LANES 1)
if(CMAKE_GENERATOR MATCHES "^Ninja")
  set_property(GLOBAL APPEND PROPERTY JOB_POOLS gamesys_test_content=2)
  set(_GS_CONTENT_JOB_POOL JOB_POOL gamesys_test_content)
  set(_GS_CONTENT_LANES 2)
endif()

# Folders in each dependency chain run sequentially, so they can reuse one
# source copy. Build metadata is cleared before each Bob invocation below.
set(_GS_CONTENT_SOURCE_STAMPS)
math(EXPR _GS_LAST_CONTENT_LANE "${_GS_CONTENT_LANES} - 1")
foreach(_lane RANGE 0 ${_GS_LAST_CONTENT_LANE})
  set(_stage_root "${GS_TEST_RUNTIME_DIR}/.bob/roots/lane-${_lane}")
  set(_source_stamp "${CMAKE_CURRENT_BINARY_DIR}/.bob/source-${_lane}.stamp")
  add_custom_command(
    OUTPUT "${_source_stamp}"
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${_stage_root}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${GS_TEST_ROOT}" "${_stage_root}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_stage_root}/builtins"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${_GS_BUILTINS_GRAPHICS_DIR}" "${_stage_root}/builtins/graphics"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${_GS_BUILTINS_MATERIALS_DIR}" "${_stage_root}/builtins/materials"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${_GS_BUILTINS_FONTS_DIR}" "${_stage_root}/builtins/fonts"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${_GS_FONT_SHADERS_DIR}" "${_stage_root}/builtins/fonts"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/.bob"
    COMMAND "${CMAKE_COMMAND}" -E touch "${_source_stamp}"
    DEPENDS ${_GS_ALL_TEST_SOURCES} ${_GS_BUILTINS_GRAPHICS_SOURCES} ${_GS_BUILTINS_MATERIAL_SOURCES} ${_GS_BUILTINS_FONT_SOURCES}
    ${_GS_CONTENT_JOB_POOL}
    COMMENT "Staging gamesys test sources for content chain ${_lane}"
    VERBATIM)
  add_custom_target(gamesys_test_sources_${_lane} DEPENDS "${_source_stamp}")
  list(APPEND _GS_CONTENT_SOURCE_STAMPS "${_source_stamp}")
endforeach()

include(functions_bob)
defold_bob_worker(_GS_WORKER_CLASSES _GS_WORKER_CLASS)
set(_GS_BATCH_SCRIPT "${GS_TEST_ROOT}/build_content_batch.py")
set(_GS_WORKER_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/../../../../../scripts/cmake/bob_worker.py")
set(gamesys_content_outputs)
set(gamesys_content_targets)
set(_GS_CONTENT_LANE 0)
foreach(_folder IN LISTS _GS_TEST_DATA_FOLDERS)
  set(_inputs_file "${GS_TEST_ROOT}/${_folder}/build.inputs")
  if(NOT EXISTS "${_inputs_file}")
    message(FATAL_ERROR "Missing gamesys test data input list: ${_inputs_file}")
  endif()
  file(GLOB_RECURSE _folder_sources CONFIGURE_DEPENDS LIST_DIRECTORIES false "${GS_TEST_ROOT}/${_folder}/*")
  list(FILTER _folder_sources EXCLUDE REGEX "/build/")
  list(FILTER _folder_sources EXCLUDE REGEX "\\.DS_Store$")
  set(_stamp "${CMAKE_CURRENT_BINARY_DIR}/.bob/${_folder}.stamp")
  set(_dependencies "${CMAKE_CURRENT_LIST_FILE}" "${_inputs_file}" "${_GS_COMMON_INPUTS_FILE}" ${_folder_sources} ${_GS_SHARED_TEST_SOURCES})
  defold_bob_job(_job "${_folder}" "${_stamp}" ${_dependencies})
  list(APPEND _GS_LANE_${_GS_CONTENT_LANE}_JOBS "${_job}")
  list(APPEND _GS_LANE_${_GS_CONTENT_LANE}_STAMPS "${_stamp}")
  list(APPEND _GS_LANE_${_GS_CONTENT_LANE}_DEPENDENCIES ${_dependencies})
  list(APPEND gamesys_content_outputs "${_stamp}")
  set(_target "gamesys_test_data_${_folder}")
  add_custom_target(${_target})
  add_dependencies(${_target} gamesys_test_content_lane_${_GS_CONTENT_LANE})
  list(APPEND gamesys_content_targets ${_target})
  math(EXPR _GS_CONTENT_LANE "(${_GS_CONTENT_LANE} + 1) % ${_GS_CONTENT_LANES}")
endforeach()

foreach(_lane RANGE 0 ${_GS_LAST_CONTENT_LANE})
  set(_stage_root "${GS_TEST_RUNTIME_DIR}/.bob/roots/lane-${_lane}")
  set(_bob_root "${_GS_BOB_ROOT_BASE}/lane-${_lane}")
  set(_classpath_entries "${_stage_root}" ${_GS_BOB_CLASSPATH_LIST})
  cmake_path(CONVERT "${_classpath_entries}" TO_NATIVE_PATH_LIST _classpath)
  list(GET _GS_CONTENT_SOURCE_STAMPS ${_lane} _source_stamp)
  set(_manifest "${CMAKE_CURRENT_BINARY_DIR}/.bob/lane-${_lane}.json")
  list(JOIN _GS_LANE_${_lane}_JOBS ",\n" _jobs)
  file(GENERATE OUTPUT "${_manifest}" CONTENT "[${_jobs}]\n")
  add_custom_command(
    OUTPUT ${_GS_LANE_${_lane}_STAMPS}
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_bob_root}"
    # Imported helpers must not add __pycache__ files to the CONFIGURE_DEPENDS glob.
    COMMAND "${CMAKE_COMMAND}" -E env
      "DM_BOB_ROOTFOLDER=${_bob_root}" "DM_JAVA_RUNTIME_FLAGS=${DEFOLD_JAVA_RUNTIME_FLAGS}" ${_GS_SHADER_ENV}
      "${Python3_EXECUTABLE}" -B "${_GS_BATCH_SCRIPT}"
      --manifest "${_manifest}" --root "${GS_TEST_ROOT}" --stage "${_stage_root}"
      --output-root "${GS_TEST_RUNTIME_DIR}" --source-stamp "${_source_stamp}"
      --bob-worker "${_GS_WORKER_CLASSES}" --classpath "${_classpath}"
      --java "${Java_JAVA_EXECUTABLE}" --platform "${TARGET_PLATFORM}" ${_GS_BOB_THREAD_ARGS}
    DEPENDS "${_manifest}" "${_GS_BATCH_SCRIPT}" "${GS_TEST_ROOT}/build_test_data.py" "${_GS_WORKER_SCRIPT}"
      "${_GS_WORKER_CLASS}" "${_source_stamp}" ${_GS_BOB_CLASSPATH_LIST} ${_GS_LANE_${_lane}_DEPENDENCIES}
    ${_GS_CONTENT_JOB_POOL}
    COMMENT "Building gamesys test content lane ${_lane}"
    VERBATIM)
  add_custom_target(gamesys_test_content_lane_${_lane} DEPENDS ${_GS_LANE_${_lane}_STAMPS})
  add_dependencies(gamesys_test_content_lane_${_lane} defold_bob_worker gamesys_test_sources_${_lane})
endforeach()
