cmake_minimum_required(VERSION 4.0)

# gamesys tests

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../../scripts/cmake")
include(defold)

set(GS_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(GS_SRC_ROOT  "${CMAKE_CURRENT_LIST_DIR}/..")
set(GS_PROTO_DIR "${GS_SRC_ROOT}/../../proto/gamesys")
get_filename_component(GS_TEST_RUN_DIR "${GS_TEST_ROOT}/../../.." ABSOLUTE)

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

# ---------------------------------------------------------------------------
# Test content setup
# Build the test content tree next to the test binaries for runtime access.
# ---------------------------------------------------------------------------
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(_GS_BOB_LIGHT "${DEFOLD_SDK_ROOT}/share/java/bob-light.jar")
if(NOT EXISTS "${_GS_BOB_LIGHT}")
  message(FATAL_ERROR "bob-light.jar not found: ${_GS_BOB_LIGHT}")
endif()

set(_GS_TEST_DATA_FOLDERS_FILE "${GS_TEST_ROOT}/test_data_folders.txt")
gamesys_read_manifest(_GS_TEST_DATA_FOLDERS "${_GS_TEST_DATA_FOLDERS_FILE}")
if(NOT _GS_TEST_DATA_FOLDERS)
  message(FATAL_ERROR "No gamesys test data folders listed in ${_GS_TEST_DATA_FOLDERS_FILE}")
endif()

set(gamesys_content_outputs)
foreach(_folder IN LISTS _GS_TEST_DATA_FOLDERS)
  set(_inputs_file "${GS_TEST_ROOT}/${_folder}/build.inputs")
  if(NOT EXISTS "${_inputs_file}")
    message(FATAL_ERROR "Missing gamesys test data input list: ${_inputs_file}")
  endif()

  file(GLOB _folder_sources CONFIGURE_DEPENDS LIST_DIRECTORIES false "${GS_TEST_ROOT}/${_folder}/*")
  list(FILTER _folder_sources EXCLUDE REGEX "/build/")
  list(FILTER _folder_sources EXCLUDE REGEX "\\.prebuilt_")
  list(FILTER _folder_sources EXCLUDE REGEX "\\.DS_Store$")

  set(_stamp "${CMAKE_CURRENT_BINARY_DIR}/.bob/${_folder}.stamp")
  add_custom_command(
    OUTPUT "${_stamp}"
    COMMAND "${Python3_EXECUTABLE}" "${GS_TEST_ROOT}/build_test_data.py"
      --bob-light "${_GS_BOB_LIGHT}"
      --root "${GS_TEST_ROOT}"
      --folder "${_folder}"
      --inputs "${_inputs_file}"
      --output-root "${CMAKE_CURRENT_BINARY_DIR}"
      --stamp "${_stamp}"
      --platform "${TARGET_PLATFORM}"
    DEPENDS "${GS_TEST_ROOT}/build_test_data.py" "${_inputs_file}" ${_folder_sources}
    WORKING_DIRECTORY "${GS_TEST_ROOT}"
    COMMENT "Building gamesys test data folder ${_folder}"
    VERBATIM)
  list(APPEND gamesys_content_outputs "${_stamp}")
endforeach()

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
add_dependencies(gamesys_test_content gamesys gamesys_test_embeds)

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
  ScriptFont
  ScriptHttp)

defold_get_physics_libraries(_GAMESYS_TEST_PHYSICS_LIBS)
defold_get_font_libraries(_GAMESYS_TEST_FONT_LIBS)
defold_get_gamesys_libraries(_GAMESYS_TEST_GAMESYS_LIBS)

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
  defold_register_test_target(${target} ON "${GS_TEST_RUN_DIR}")

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
  add_dependencies(test_gamesys_http gamesys_test_content)
  target_include_directories(test_gamesys_http PRIVATE "${GS_SRC_ROOT}" "${GS_PROTO_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
  target_link_libraries(test_gamesys_http PRIVATE gamesys gamesys_rig_null gamesys_model_null)
  defold_target_link_libraries(test_gamesys_http ${TARGET_PLATFORM} ${_DEFOLD_TEST_LIBS})
  defold_target_link_bullet3d(test_gamesys_http ${TARGET_PLATFORM})
  defold_target_link_lua(test_gamesys_http ${TARGET_PLATFORM})
  defold_target_link_glfw(test_gamesys_http ${TARGET_PLATFORM})
  defold_target_link_platform(test_gamesys_http ${TARGET_PLATFORM})
  defold_target_link_socket(test_gamesys_http ${TARGET_PLATFORM})
  defold_target_link_graphics(test_gamesys_http ${TARGET_PLATFORM})
  defold_target_link_app(test_gamesys_http ${TARGET_PLATFORM})
  defold_register_test_target(test_gamesys_http FALSE)
  defold_register_test_with_server(test_gamesys_http ${TARGET_PLATFORM} PORT 9001 WORKDIR "${GS_TEST_RUN_DIR}")
endif()
