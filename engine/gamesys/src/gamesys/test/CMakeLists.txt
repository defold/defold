cmake_minimum_required(VERSION 4.0)

# gamesys tests

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../../scripts/cmake")
include(defold)

set(GS_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(GS_SRC_ROOT  "${CMAKE_CURRENT_LIST_DIR}/..")
set(GS_PROTO_DIR "${GS_SRC_ROOT}/../../proto/gamesys")
get_filename_component(GS_ENGINE_CONTENT_DIR "${GS_TEST_ROOT}/../../../../engine/content" ABSOLUTE)
set(GS_TEST_RUN_DIR "${DEFOLD_GAMESYS_BINARY_DIR}/gamesys-test-runtime")
set(GS_TEST_RUNTIME_DIR "${GS_TEST_RUN_DIR}/build/src/gamesys/test")
file(MAKE_DIRECTORY "${GS_TEST_RUN_DIR}")

# When included from engine/gamesys, the 'gamesys' target is already defined.
# If configured standalone, callers must provide/link appropriate libs.

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
find_package(Java REQUIRED COMPONENTS Runtime)

set(_GS_BOB_LIGHT "${DEFOLD_SDK_ROOT}/share/java/bob-light.jar")
if(NOT EXISTS "${_GS_BOB_LIGHT}")
  if(NOT TARGET bob_light)
    message(FATAL_ERROR "bob-light.jar not found and bob_light target is unavailable: ${_GS_BOB_LIGHT}")
  endif()
endif()

file(GLOB _GS_BOB_PLUGIN_JARS CONFIGURE_DEPENDS "${DEFOLD_SDK_ROOT}/share/java/plugins/*.jar")
set(_GS_BOB_CLASSPATH_LIST "${_GS_BOB_LIGHT}" ${_GS_BOB_PLUGIN_JARS})
set(_GS_BUILTINS_ZIP "${DEFOLD_SDK_ROOT}/share/builtins.zip")
if(EXISTS "${_GS_BUILTINS_ZIP}")
  list(APPEND _GS_BOB_CLASSPATH_LIST "${_GS_BUILTINS_ZIP}")
endif()
cmake_path(CONVERT "${_GS_BOB_CLASSPATH_LIST}" TO_NATIVE_PATH_LIST _GS_BOB_CLASSPATH)

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

set(_GS_BOB_STAGE_ROOT "${GS_TEST_RUNTIME_DIR}/.bob/root")
set(_GS_BOB_STAGE_STAMP "${CMAKE_CURRENT_BINARY_DIR}/.bob/stage.stamp")
if(DEFINED ENV{DM_BOB_ROOTFOLDER} AND NOT "$ENV{DM_BOB_ROOTFOLDER}" STREQUAL "")
  set(_GS_BOB_ROOT_BASE "$ENV{DM_BOB_ROOTFOLDER}/gamesys")
else()
  set(_GS_BOB_ROOT_BASE "${GS_TEST_RUNTIME_DIR}/.bob/bob-root")
endif()
add_custom_command(
  OUTPUT "${_GS_BOB_STAGE_STAMP}"
  COMMAND "${CMAKE_COMMAND}" -E remove_directory "${_GS_BOB_STAGE_ROOT}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${_GS_BOB_STAGE_ROOT}"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory "${GS_TEST_ROOT}" "${_GS_BOB_STAGE_ROOT}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${_GS_BOB_STAGE_ROOT}/builtins"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory "${_GS_BUILTINS_GRAPHICS_DIR}" "${_GS_BOB_STAGE_ROOT}/builtins/graphics"
  COMMAND "${CMAKE_COMMAND}" -E remove_directory "${_GS_BOB_STAGE_ROOT}/build"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/.bob"
  COMMAND "${CMAKE_COMMAND}" -E touch "${_GS_BOB_STAGE_STAMP}"
  DEPENDS ${_GS_ALL_TEST_SOURCES} ${_GS_BUILTINS_GRAPHICS_SOURCES} "${_GS_COMMON_INPUTS_FILE}"
  COMMENT "Staging gamesys test data for Bob"
  VERBATIM)

set(gamesys_content_outputs)
set(_GS_PREVIOUS_CONTENT_STAMP)
foreach(_folder IN LISTS _GS_TEST_DATA_FOLDERS)
  set(_inputs_file "${GS_TEST_ROOT}/${_folder}/build.inputs")
  if(NOT EXISTS "${_inputs_file}")
    message(FATAL_ERROR "Missing gamesys test data input list: ${_inputs_file}")
  endif()

  file(GLOB_RECURSE _folder_sources CONFIGURE_DEPENDS LIST_DIRECTORIES false "${GS_TEST_ROOT}/${_folder}/*")
  list(FILTER _folder_sources EXCLUDE REGEX "/build/")
  list(FILTER _folder_sources EXCLUDE REGEX "\\.DS_Store$")

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
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${_GS_BOB_STAGE_ROOT}/build/${_folder}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_bob_root}"
    COMMAND "${CMAKE_COMMAND}" -E env
      "DM_BOB_ROOTFOLDER=${_bob_root}"
      "DM_JAVA_RUNTIME_FLAGS=${DEFOLD_JAVA_RUNTIME_FLAGS}"
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
    DEPENDS "${_GS_BOB_STAGE_STAMP}" ${_GS_PREVIOUS_CONTENT_STAMP} "${_GS_BOB_LIGHT}" ${_GS_BOB_PLUGIN_JARS} "${_inputs_file}" "${_GS_COMMON_INPUTS_FILE}" ${_folder_sources} ${_GS_SHARED_TEST_SOURCES} ${_GS_ALL_TEST_SOURCES} ${_prebuilt_sources}
    WORKING_DIRECTORY "${_GS_BOB_STAGE_ROOT}"
    COMMENT "Building gamesys test data folder ${_folder}"
    VERBATIM)
  list(APPEND gamesys_content_outputs "${_stamp}")
  set(_GS_PREVIOUS_CONTENT_STAMP "${_stamp}")
endforeach()

# ---------------------------------------------------------------------------
# Host files loaded through source-tree-relative host paths.
# Mirror the Waf test runner staging paths so the CMake runtime has the same
# source-tree-relative files under GS_TEST_RUN_DIR.
# ---------------------------------------------------------------------------
set(_GS_HOST_TEST_PATHS
  "resource/blank.basis"
  "sound_ext/replacement.wav"
  "font"
  "image"
  "sys/sys_buffer_test_content.raw"
  "http")
set(_GS_STAGED_HOST_TEST_PATHS)
set(_GS_HOST_TEST_STAGE_FILES)
foreach(_host_path IN LISTS _GS_HOST_TEST_PATHS)
  set(_host_src "${GS_TEST_ROOT}/${_host_path}")
  set(_host_dst "${GS_TEST_RUN_DIR}/src/gamesys/test/${_host_path}")
  if(NOT EXISTS "${_host_src}")
    message(FATAL_ERROR "Missing gamesys host test path: ${_host_src}")
  endif()

  if(IS_DIRECTORY "${_host_src}")
    file(GLOB_RECURSE _host_deps CONFIGURE_DEPENDS LIST_DIRECTORIES false "${_host_src}/*")
    set(_host_output "${CMAKE_CURRENT_BINARY_DIR}/.host/${_host_path}.stamp")
    get_filename_component(_host_output_dir "${_host_output}" DIRECTORY)
    add_custom_command(
      OUTPUT "${_host_output}"
      COMMAND "${CMAKE_COMMAND}" -E remove_directory "${_host_dst}"
      COMMAND "${CMAKE_COMMAND}" -E copy_directory "${_host_src}" "${_host_dst}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${_host_output_dir}"
      COMMAND "${CMAKE_COMMAND}" -E touch "${_host_output}"
      DEPENDS ${_host_deps}
      COMMENT "Staging gamesys host test directory ${_host_path}"
      VERBATIM)
  else()
    set(_host_output "${_host_dst}")
    get_filename_component(_host_dst_dir "${_host_dst}" DIRECTORY)
    add_custom_command(
      OUTPUT "${_host_output}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${_host_dst_dir}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_host_src}" "${_host_dst}"
      DEPENDS "${_host_src}"
      COMMENT "Staging gamesys host test file ${_host_path}"
      VERBATIM)
  endif()

  list(APPEND _GS_STAGED_HOST_TEST_PATHS "${_host_output}")
  list(APPEND _GS_HOST_TEST_STAGE_FILES
    "src/gamesys/test/${_host_path}" "src/gamesys/test/${_host_path}")
endforeach()
add_custom_target(gamesys_host_test_files DEPENDS ${_GS_STAGED_HOST_TEST_PATHS})

# ---------------------------------------------------------------------------
# Embed resources into headers/sources (used by test_gamesys.cpp)
# Generates <path>.embed.h/.embed.cpp with variables NAME[] and NAME_SIZE.
# ---------------------------------------------------------------------------

set(_GS_EMBED_RESOURCES_FILE "${GS_TEST_ROOT}/embed_resources.txt")
gamesys_read_manifest(_GS_EMBED_RESOURCES "${_GS_EMBED_RESOURCES_FILE}")

set(_GS_EMBED_ROOT "${CMAKE_CURRENT_BINARY_DIR}/generated/embed")
set(_EMBED_HEADERS)
set(_EMBED_CPP)
foreach(_embed_rel IN LISTS _GS_EMBED_RESOURCES)
  set(_embed_input "${GS_TEST_ROOT}/${_embed_rel}")
  if(NOT EXISTS "${_embed_input}")
    message(FATAL_ERROR "Missing gamesys embedded test resource: ${_embed_input}")
  endif()

  set(_embed_h "${_GS_EMBED_ROOT}/${_embed_rel}.embed.h")
  set(_embed_cpp "${_GS_EMBED_ROOT}/${_embed_rel}.embed.cpp")
  defold_embed_to_header(
    INPUT "${_embed_input}"
    OUTPUT_HEADER "${_embed_h}"
    OUTPUT_CPP "${_embed_cpp}")
  list(APPEND _EMBED_HEADERS "${_embed_h}")
  list(APPEND _EMBED_CPP     "${_embed_cpp}")
endforeach()

if(NOT _EMBED_HEADERS)
  message(FATAL_ERROR "No gamesys embedded resources listed in ${_GS_EMBED_RESOURCES_FILE}")
endif()

set_source_files_properties(${_EMBED_HEADERS} ${_EMBED_CPP} PROPERTIES GENERATED TRUE)

# Make a phony target for the generated embed files.
add_custom_target(gamesys_test_embeds DEPENDS ${_EMBED_HEADERS} ${_EMBED_CPP})

# Make a phony target to expose content build step
add_custom_target(gamesys_test_content DEPENDS ${gamesys_content_outputs})
add_dependencies(gamesys_test_content gamesys gamesys_test_embeds gamesys_host_test_files)

# test_gamesys_http uses ComponentTest, whose factory root is collision_object.
# Mirror the HTTP test collection into that root to match the resource path used
# by the test: /http/http_request.goc.
set(_GS_HTTP_TEST_CONTENT_STAMP "${CMAKE_CURRENT_BINARY_DIR}/.bob/http_collision_object.stamp")
add_custom_command(
  OUTPUT "${_GS_HTTP_TEST_CONTENT_STAMP}"
  COMMAND "${CMAKE_COMMAND}" -E remove_directory "${GS_TEST_RUNTIME_DIR}/collision_object/http"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
    "${GS_TEST_RUNTIME_DIR}/http/http"
    "${GS_TEST_RUNTIME_DIR}/collision_object/http"
  COMMAND "${CMAKE_COMMAND}" -E touch "${_GS_HTTP_TEST_CONTENT_STAMP}"
  DEPENDS ${gamesys_content_outputs}
  COMMENT "Staging gamesys HTTP test data for ComponentTest"
  VERBATIM)
add_custom_target(gamesys_http_test_content DEPENDS "${_GS_HTTP_TEST_CONTENT_STAMP}")
add_dependencies(gamesys_http_test_content gamesys_test_content)

set(_GS_HTTP_CONFIG_RUNTIME "${GS_TEST_RUN_DIR}/src/gamesys/test/http/test_http.config.raw")
add_custom_command(
  OUTPUT "${_GS_HTTP_CONFIG_RUNTIME}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${GS_TEST_RUN_DIR}/src/gamesys/test/http"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${GS_TEST_ROOT}/http/test_http.config.raw"
    "${_GS_HTTP_CONFIG_RUNTIME}"
  DEPENDS "${GS_TEST_ROOT}/http/test_http.config.raw"
  COMMENT "Staging gamesys HTTP test config"
  VERBATIM)
add_custom_target(gamesys_http_test_config DEPENDS "${_GS_HTTP_CONFIG_RUNTIME}")
add_dependencies(gamesys_http_test_content gamesys_http_test_config)

# ---------------------------------------------------------------------------
# Exported symbols helper (prevents stripping by linker and ensures adapters)
# ---------------------------------------------------------------------------
set(_COMMON_EXPORTED
  ComponentTypeAnim
  ComponentTypeGui
  ComponentTypeMesh
  ComponentTypeScript
  ComponentTypeSound
  ComponentTypeLight
  ResourceProviderFile
  ResourceTypeAnim
  ResourceTypeAnimationSet
  ResourceTypeCollection
  ResourceTypeData
  ResourceTypeLight
  ResourceTypeGameObject
  ResourceTypeGui
  ResourceTypeGuiScript
  ResourceTypeLua
  ResourceTypeOgg
  ResourceTypeOpus
  ResourceTypeScript
  ResourceTypeTTF
  ResourceTypeFont
  ResourceTypeWav
  GraphicsAdapterNull
  ScriptImageExt
  ScriptMaterialExt
  ScriptFont
  ScriptHttp)

defold_get_physics_libraries(_GAMESYS_TEST_PHYSICS_LIBS)
defold_get_font_libraries(_GAMESYS_TEST_FONT_LIBS)
defold_get_gamesys_libraries(_GAMESYS_TEST_GAMESYS_LIBS)

if(DEFOLD_PLATFORM_SUPPORTS_COMPUTE)
  list(APPEND _COMMON_EXPORTED ScriptComputeExt)
endif()

set(_DEFOLD_TEST_LIBS
  testmain test_script dlib mbedtls zip ddf resource gameobject extension script input
  particle rig gui liveupdate graphics_null profile_null platform_null hid_null
  render render_font_default sound_null
  ${_GAMESYS_TEST_PHYSICS_LIBS}
  ${_GAMESYS_TEST_FONT_LIBS})

set(_GS_TEST_INCLUDES
  "${GS_SRC_ROOT}"
  "${GS_PROTO_DIR}"
  "${_GS_EMBED_ROOT}"
  "${GS_GENERATED_INCLUDE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}")

function(gamesys_configure_common_test target)
  add_dependencies(${target} gamesys_test_content)
  target_include_directories(${target} PRIVATE ${_GS_TEST_INCLUDES})
  target_link_libraries(${target} PRIVATE ${_GAMESYS_TEST_GAMESYS_LIBS})
  defold_target_link_libraries(${target} ${TARGET_PLATFORM} ${_DEFOLD_TEST_LIBS})
  defold_target_link_bullet3d(${target} ${TARGET_PLATFORM})
  defold_target_link_lua(${target} ${TARGET_PLATFORM})
  defold_target_link_glfw(${target} ${TARGET_PLATFORM})
  defold_target_link_platform(${target} ${TARGET_PLATFORM})
  defold_target_link_socket(${target} ${TARGET_PLATFORM})
  defold_target_link_graphics(${target} ${TARGET_PLATFORM})
  defold_target_link_app(${target} ${TARGET_PLATFORM})
  defold_register_test_target(${target} ON "${GS_TEST_RUN_DIR}"
    STAGE_FILES
      "build/src/gamesys/test" "build/src/gamesys/test"
      ${_GS_HOST_TEST_STAGE_FILES})

  defold_target_link_libraries_web(${target} ${TARGET_PLATFORM} SCOPE PRIVATE
    library_sys.js library_script.js library_render.js)
endfunction()

# ---------------------------------------------------------------------------
# test_gamesys
# ---------------------------------------------------------------------------
set(TEST_GS_SOURCES
  "${GS_TEST_ROOT}/test_gamesys.cpp"
)
set_source_files_properties("${GS_TEST_ROOT}/test_gamesys.cpp" PROPERTIES OBJECT_DEPENDS "${_EMBED_HEADERS}")

set(_TEST1_SYMBOLS ${_COMMON_EXPORTED};ScriptModelExt)
set(_TEST1_EXPORTED_CPP "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols_test_gamesys.cpp")
defold_create_exported_symbols_file("${_TEST1_EXPORTED_CPP}" "${_TEST1_SYMBOLS}")

defold_add_executable(test_gamesys ${TEST_GS_SOURCES} ${_TEST1_EXPORTED_CPP} ${_EMBED_HEADERS} ${_EMBED_CPP})
gamesys_configure_common_test(test_gamesys)

# ---------------------------------------------------------------------------
# test_gamesys_physics
# ---------------------------------------------------------------------------
set(TEST_GS_PHYS_SOURCES
  "${GS_TEST_ROOT}/test_gamesys_physics.cpp"
)
set_source_files_properties("${GS_TEST_ROOT}/test_gamesys_physics.cpp" PROPERTIES OBJECT_DEPENDS "${_EMBED_HEADERS}")
set(_TEST2_SYMBOLS ${_COMMON_EXPORTED};ScriptModelExt)
set(_TEST2_EXPORTED_CPP "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols_test_gamesys_physics.cpp")
defold_create_exported_symbols_file("${_TEST2_EXPORTED_CPP}" "${_TEST2_SYMBOLS}")

defold_add_executable(test_gamesys_physics ${TEST_GS_PHYS_SOURCES} ${_TEST2_EXPORTED_CPP} ${_EMBED_HEADERS} ${_EMBED_CPP})
gamesys_configure_common_test(test_gamesys_physics)

# ---------------------------------------------------------------------------
# test_gamesys_http (not for web targets)
# ---------------------------------------------------------------------------
if(NOT TARGET_PLATFORM MATCHES "^(js-web|wasm-web|wasm_pthread-web)$")
  set(TEST_GS_HTTP_SOURCES "${GS_TEST_ROOT}/test_gamesys_http.cpp")
  set(_TEST3_SYMBOLS ${_COMMON_EXPORTED})
  set(_TEST3_EXPORTED_CPP "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols_test_gamesys_http.cpp")
  defold_create_exported_symbols_file("${_TEST3_EXPORTED_CPP}" "${_TEST3_SYMBOLS}")

  defold_add_executable(test_gamesys_http ${TEST_GS_HTTP_SOURCES} ${_TEST3_EXPORTED_CPP})
  add_dependencies(test_gamesys_http gamesys_http_test_content)
  target_include_directories(test_gamesys_http PRIVATE "${GS_SRC_ROOT}" "${GS_PROTO_DIR}" "${GS_GENERATED_INCLUDE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
  target_link_libraries(test_gamesys_http PRIVATE gamesys gamesys_rig_null gamesys_model_null)
  defold_target_link_libraries(test_gamesys_http ${TARGET_PLATFORM} ${_DEFOLD_TEST_LIBS})
  defold_target_link_bullet3d(test_gamesys_http ${TARGET_PLATFORM})
  defold_target_link_lua(test_gamesys_http ${TARGET_PLATFORM})
  defold_target_link_glfw(test_gamesys_http ${TARGET_PLATFORM})
  defold_target_link_platform(test_gamesys_http ${TARGET_PLATFORM})
  defold_target_link_socket(test_gamesys_http ${TARGET_PLATFORM})
  defold_target_link_graphics(test_gamesys_http ${TARGET_PLATFORM})
  defold_target_link_app(test_gamesys_http ${TARGET_PLATFORM})
  defold_register_test_target(test_gamesys_http FALSE "${GS_TEST_RUN_DIR}"
    STAGE_FILES
      "build/src/gamesys/test" "build/src/gamesys/test"
      ${_GS_HOST_TEST_STAGE_FILES})
  defold_register_test_with_server(test_gamesys_http ${TARGET_PLATFORM} PORT 9001 WORKDIR "${GS_TEST_RUN_DIR}")
endif()
