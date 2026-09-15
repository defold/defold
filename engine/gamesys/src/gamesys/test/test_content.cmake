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

function(gamesys_collect_raw_inputs out_var)
  set(_raw_inputs)
  foreach(_inputs_file IN LISTS ARGN)
    file(STRINGS "${_inputs_file}" _input_lines)
    foreach(_line IN LISTS _input_lines)
      string(REGEX REPLACE "#.*$" "" _entry "${_line}")
      string(STRIP "${_entry}" _entry)
      if(_entry MATCHES "^/(.*\\.raw)$")
        list(APPEND _raw_inputs "${CMAKE_MATCH_1}")
      endif()
    endforeach()
  endforeach()
  list(REMOVE_DUPLICATES _raw_inputs)
  set(${out_var} ${_raw_inputs} PARENT_SCOPE)
endfunction()

function(gamesys_collect_prebuilt_outputs out_commands out_sources source_root output_root)
  set(_commands)
  set(_sources)
  foreach(_source IN LISTS ARGN)
    get_filename_component(_name "${_source}" NAME)
    if(NOT _name MATCHES "^.+\\.prebuilt_.+$")
      continue()
    endif()

    string(REGEX REPLACE "\\.prebuilt_([^/]*)$" ".\\1" _output_source "${_source}")
    file(RELATIVE_PATH _output_rel "${source_root}" "${_output_source}")
    set(_output "${output_root}/${_output_rel}")
    get_filename_component(_output_dir "${_output}" DIRECTORY)
    list(APPEND _commands
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${_output_dir}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_source}" "${_output}")
    list(APPEND _sources "${_source}")
  endforeach()

  set(${out_commands} ${_commands} PARENT_SCOPE)
  set(${out_sources} ${_sources} PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# Test content setup
# Build the runtime test content tree under the platform build root.
# ---------------------------------------------------------------------------
set(_GS_BOB_LIGHT "${DEFOLD_SDK_ROOT}/share/java/bob-light.jar")
if(NOT EXISTS "${_GS_BOB_LIGHT}")
  if(NOT TARGET bob_light)
    message(FATAL_ERROR "bob-light.jar not found and bob_light target is unavailable: ${_GS_BOB_LIGHT}")
  endif()
endif()

set(_GS_BOB_PLUGIN_JARS)
if(COMMAND defold_append_platform_bob_plugin_jars)
  defold_append_platform_bob_plugin_jars(_GS_BOB_PLUGIN_JARS)
endif()
set(_GS_BOB_CLASSPATH_LIST "${_GS_BOB_LIGHT}" ${_GS_BOB_PLUGIN_JARS})
set(_GS_BUILTINS_ZIP "${DEFOLD_SDK_ROOT}/share/builtins.zip")
if(EXISTS "${_GS_BUILTINS_ZIP}")
  list(APPEND _GS_BOB_CLASSPATH_LIST "${_GS_BUILTINS_ZIP}")
endif()
cmake_path(CONVERT "${_GS_BOB_CLASSPATH_LIST}" TO_NATIVE_PATH_LIST _GS_BOB_CLASSPATH)
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
list(FILTER _GS_ALL_TEST_SOURCES EXCLUDE REGEX "/build/")
list(FILTER _GS_ALL_TEST_SOURCES EXCLUDE REGEX "\\.prebuilt_")
list(FILTER _GS_ALL_TEST_SOURCES EXCLUDE REGEX "\\.DS_Store$")
list(FILTER _GS_ALL_TEST_SOURCES EXCLUDE REGEX "/CMakeLists\\.txt$")
list(FILTER _GS_ALL_TEST_SOURCES EXCLUDE REGEX "/wscript$")
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

if(DEFINED ENV{DM_BOB_ROOTFOLDER} AND NOT "$ENV{DM_BOB_ROOTFOLDER}" STREQUAL "")
  set(_GS_BOB_ROOT_BASE "$ENV{DM_BOB_ROOTFOLDER}/gamesys")
else()
  set(_GS_BOB_ROOT_BASE "${GS_TEST_RUNTIME_DIR}/.bob/bob-root")
endif()

# Each Bob process owns its project metadata and extracted tools. Limit the
# number of JVMs competing with compilation for memory on CI runners.
set(_GS_CONTENT_JOB_POOL)
if(CMAKE_GENERATOR MATCHES "^Ninja")
  set_property(GLOBAL APPEND PROPERTY JOB_POOLS gamesys_test_content=2)
  set(_GS_CONTENT_JOB_POOL JOB_POOL gamesys_test_content)
endif()
set(gamesys_content_outputs)
set(gamesys_content_targets)
set(_GS_PREVIOUS_CONTENT_TARGET)
set(_GS_CONTENT_QUEUE)
foreach(_folder IN LISTS _GS_TEST_DATA_FOLDERS)
  if(CMAKE_GENERATOR MATCHES "^Ninja")
    set(_GS_PREVIOUS_CONTENT_TARGET)
    list(LENGTH _GS_CONTENT_QUEUE _queue_size)
    if(_queue_size EQUAL 2)
      list(POP_FRONT _GS_CONTENT_QUEUE _GS_PREVIOUS_CONTENT_TARGET)
    endif()
  endif()
  set(_inputs_file "${GS_TEST_ROOT}/${_folder}/build.inputs")
  if(NOT EXISTS "${_inputs_file}")
    message(FATAL_ERROR "Missing gamesys test data input list: ${_inputs_file}")
  endif()

  file(GLOB_RECURSE _folder_sources CONFIGURE_DEPENDS LIST_DIRECTORIES false "${GS_TEST_ROOT}/${_folder}/*")
  list(FILTER _folder_sources EXCLUDE REGEX "/build/")
  list(FILTER _folder_sources EXCLUDE REGEX "\\.DS_Store$")

  set(_GS_BOB_STAGE_ROOT "${GS_TEST_RUNTIME_DIR}/.bob/roots/${_folder}")
  set(_settings_args)
  if(EXISTS "${GS_TEST_ROOT}/${_folder}/game.project")
    list(APPEND _settings_args --settings "${_GS_BOB_STAGE_ROOT}/${_folder}/game.project")
  endif()

  gamesys_collect_raw_inputs(_raw_inputs "${_inputs_file}" "${_GS_COMMON_INPUTS_FILE}")
  set(_raw_copy_commands)
  foreach(_raw_input IN LISTS _raw_inputs)
    get_filename_component(_raw_output_dir "${GS_TEST_RUNTIME_DIR}/${_folder}/${_raw_input}c" DIRECTORY)
    list(APPEND _raw_copy_commands
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${_raw_output_dir}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${GS_TEST_ROOT}/${_raw_input}"
        "${GS_TEST_RUNTIME_DIR}/${_folder}/${_raw_input}c")
  endforeach()
  gamesys_collect_prebuilt_outputs(_prebuilt_copy_commands _prebuilt_sources "${GS_TEST_ROOT}" "${GS_TEST_RUNTIME_DIR}/${_folder}" ${_folder_sources})

  set(_bob_root "${_GS_BOB_ROOT_BASE}/${_folder}")
  set(_stamp "${CMAKE_CURRENT_BINARY_DIR}/.bob/${_folder}.stamp")
  add_custom_command(
    OUTPUT "${_stamp}"
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${_GS_BOB_STAGE_ROOT}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_GS_BOB_STAGE_ROOT}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${GS_TEST_ROOT}" "${_GS_BOB_STAGE_ROOT}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_GS_BOB_STAGE_ROOT}/builtins"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${_GS_BUILTINS_GRAPHICS_DIR}" "${_GS_BOB_STAGE_ROOT}/builtins/graphics"
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${_GS_BOB_STAGE_ROOT}/build"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_bob_root}"
    COMMAND "${CMAKE_COMMAND}" -E env
      "DM_BOB_ROOTFOLDER=${_bob_root}"
      "DM_JAVA_RUNTIME_FLAGS=${DEFOLD_JAVA_RUNTIME_FLAGS}"
      ${_GS_SHADER_ENV}
      "${Java_JAVA_EXECUTABLE}" ${DEFOLD_JAVA_RUNTIME_FLAGS_LIST}
      -cp "${_GS_BOB_CLASSPATH}"
      com.dynamo.bob.Bob
      --root "${_GS_BOB_STAGE_ROOT}"
      --output "build/${_folder}"
      --build-input-file "${_GS_BOB_STAGE_ROOT}/${_folder}/build.inputs"
      --build-input-file "${_GS_BOB_STAGE_ROOT}/common_build.inputs"
      --use-uncompressed-lua-source
      --debug-output-spirv true
      --platform "${TARGET_PLATFORM}"
      ${_settings_args}
      build
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${GS_TEST_RUNTIME_DIR}/${_folder}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${_GS_BOB_STAGE_ROOT}/build/${_folder}" "${GS_TEST_RUNTIME_DIR}/${_folder}"
    ${_raw_copy_commands}
    ${_prebuilt_copy_commands}
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/.bob"
    COMMAND "${CMAKE_COMMAND}" -E touch "${_stamp}"
    DEPENDS ${_GS_PREVIOUS_CONTENT_TARGET} ${_GS_BUILTINS_GRAPHICS_SOURCES} "${_GS_BOB_LIGHT}" ${_GS_BOB_PLUGIN_JARS} "${_inputs_file}" "${_GS_COMMON_INPUTS_FILE}" ${_folder_sources} ${_GS_SHARED_TEST_SOURCES} ${_GS_ALL_TEST_SOURCES} ${_prebuilt_sources}
    WORKING_DIRECTORY "${GS_TEST_ROOT}"
    ${_GS_CONTENT_JOB_POOL}
    COMMENT "Building gamesys test data folder ${_folder}"
    VERBATIM)
  list(APPEND gamesys_content_outputs "${_stamp}")
  # Give each command its own target so dependencies of the runtime aggregate
  # do not become implicit prerequisites of every Bob invocation.
  set(_content_target "gamesys_test_data_${_folder}")
  add_custom_target(${_content_target} DEPENDS "${_stamp}")
  list(APPEND gamesys_content_targets ${_content_target})
  if(CMAKE_GENERATOR MATCHES "^Ninja")
    # Two chains expose the queued JVM work to Ninja's critical-path scheduler.
    # Target dependencies only order commands; touching a stamp must not rebuild
    # later folders as a file dependency would.
    list(APPEND _GS_CONTENT_QUEUE ${_content_target})
  else()
    # Other generators ignore job pools, so use one ordered target chain.
    set(_GS_PREVIOUS_CONTENT_TARGET ${_content_target})
  endif()
endforeach()
