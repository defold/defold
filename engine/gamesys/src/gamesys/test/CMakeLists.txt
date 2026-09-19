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

find_package(Java REQUIRED COMPONENTS Runtime)
include("${CMAKE_CURRENT_LIST_DIR}/test_content.cmake")

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
# Embed resources into headers/sources (used by the gamesys tests)
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
add_custom_target(gamesys_test_content)
add_dependencies(gamesys_test_content ${gamesys_content_targets} gamesys_test_embeds gamesys_host_test_files)

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
  ComponentTypeParticleFX
  ComponentTypeTileMap
  ResourceProviderFile
  ResourceTypeAnim
  ResourceTypeAnimationSet
  ResourceTypeCollection
  ResourceTypeData
  ResourceTypeLight
  ResourceTypeParticleFX
  ResourceTypeTileMap
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
  ScriptHttp
  ScriptLibParticleFX
  ScriptLibTileMap)

defold_get_physics_libraries(_GAMESYS_TEST_PHYSICS_LIBS)
defold_get_font_gen_library(_GAMESYS_TEST_FONT_GEN_LIB)
defold_get_font_libraries(_GAMESYS_TEST_FONT_LIBS)
defold_get_gamesys_libraries(_GAMESYS_TEST_GAMESYS_LIBS)

if(DEFOLD_PLATFORM_SUPPORTS_COMPUTE)
  list(APPEND _COMMON_EXPORTED ScriptComputeExt)
endif()

set(_DEFOLD_TEST_LIBS
  ${_GAMESYS_TEST_FONT_GEN_LIB}
  testmain test_script dlib mbedtls zip ddf resource gameobject extension script input
  particle rig gui liveupdate profile_null platform_null hid_null
  render render_font_default sound_null
  ${_GAMESYS_TEST_PHYSICS_LIBS}
  ${_GAMESYS_TEST_FONT_LIBS})

# Mirrors Waf's GRAPHICS_GAMESYS_TEST group. These tests are headless and must
# not select the platform graphics backend through the general graphics mapper.
set(_GS_TEST_GRAPHICS_LIBS
  graphics_null image graphics_transcoder_basisu basis_transcoder)

set(_GS_TEST_DEFINES)
if(TARGET_PLATFORM MATCHES "^(x86_64-xbone|js-web|wasm-web|wasm_pthread-web)$")
  # Keep jc_test_state layout identical to the Waf-built test support library.
  list(APPEND _GS_TEST_DEFINES JC_TEST_NO_DEATH_TEST)
endif()

set(_GS_TEST_INCLUDES
  "${GS_SRC_ROOT}"
  "${GS_PROTO_DIR}"
  "${DEFOLD_SDK_ROOT}/include/script"
  "${_GS_EMBED_ROOT}"
  "${GS_GENERATED_INCLUDE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}")

function(gamesys_configure_common_test target)
  add_dependencies(${target} gamesys_test_embeds)
  target_include_directories(${target} PRIVATE ${_GS_TEST_INCLUDES})
  target_compile_definitions(${target} PRIVATE ${_GS_TEST_DEFINES})
  target_link_libraries(${target} PRIVATE ${_GAMESYS_TEST_GAMESYS_LIBS})
  defold_target_link_libraries(${target} ${TARGET_PLATFORM} ${_DEFOLD_TEST_LIBS})
  defold_target_link_libraries(${target} ${TARGET_PLATFORM} ${_GS_TEST_GRAPHICS_LIBS})
  defold_target_link_bullet3d(${target} ${TARGET_PLATFORM})
  defold_target_link_lua(${target} ${TARGET_PLATFORM})
  defold_target_link_glfw(${target} ${TARGET_PLATFORM})
  defold_target_link_platform(${target} ${TARGET_PLATFORM})
  defold_target_link_socket(${target} ${TARGET_PLATFORM})
  defold_target_link_app(${target} ${TARGET_PLATFORM})
  # The headless suites use null device backends and module-local content.
  # Keep them together because resource reload tests modify staged files.
  # HTTP tests register separately and retain the shared server group.
  defold_register_test_target(${target} ON "${GS_TEST_RUN_DIR}"
    RUN_GROUP gamesys
    RUNTIME_DEPENDS gamesys_test_content
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
  "${GS_TEST_ROOT}/test_gamesys_components.cpp"
  "${GS_TEST_ROOT}/test_gamesys_gui.cpp"
  "${GS_TEST_ROOT}/test_gamesys_render.cpp"
  "${GS_TEST_ROOT}/test_gamesys_script.cpp"
)
set_source_files_properties(${TEST_GS_SOURCES} PROPERTIES OBJECT_DEPENDS "${_EMBED_HEADERS}")

set(_TEST1_SYMBOLS ${_COMMON_EXPORTED};ScriptModelExt)
set(_TEST1_EXPORTED_CPP "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols_test_gamesys.cpp")
defold_create_exported_symbols_file("${_TEST1_EXPORTED_CPP}" "${_TEST1_SYMBOLS}")

defold_add_executable(test_gamesys ${TEST_GS_SOURCES} ${_TEST1_EXPORTED_CPP} ${_EMBED_HEADERS} ${_EMBED_CPP})
gamesys_configure_common_test(test_gamesys)

# ---------------------------------------------------------------------------
# test_app_shader_program
# ---------------------------------------------------------------------------
if(NOT TARGET_PLATFORM MATCHES "^(arm64_sim-ios|x86_64-ps4|x86_64-ps5)$")
  if(DEFINED WITH_OPENGL)
    set(_shader_program_had_with_opengl ON)
    set(_shader_program_saved_with_opengl "${WITH_OPENGL}")
  endif()
  set(WITH_OPENGL ON)
  defold_get_graphics_symbols(_shader_program_graphics_symbols "${TARGET_PLATFORM}")
  if("GraphicsAdapterOpenGL" IN_LIST _shader_program_graphics_symbols OR
     "GraphicsAdapterOpenGLES" IN_LIST _shader_program_graphics_symbols)
    list(APPEND _shader_program_graphics_symbols ResourceProviderFile)
    defold_create_exported_symbols_file(
      "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols_test_app_shader_program.cpp"
      "${_shader_program_graphics_symbols}")

    defold_add_executable(test_app_shader_program
      "${GS_TEST_ROOT}/test_app_shader_program.cpp"
      "${GS_TEST_ROOT}/../resources/res_shader_program.cpp"
      "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols_test_app_shader_program.cpp")
    target_include_directories(test_app_shader_program PRIVATE
      "${GS_SRC_ROOT}"
      "${GS_PROTO_DIR}")
    defold_target_link_libraries(test_app_shader_program "${TARGET_PLATFORM}"
      testmain
      ddf
      dlib
      profile_null
      resource)
    defold_target_link_graphics(test_app_shader_program "${TARGET_PLATFORM}" SCOPE PRIVATE)
    defold_target_link_platform(test_app_shader_program "${TARGET_PLATFORM}" SCOPE PRIVATE)
    defold_target_link_socket(test_app_shader_program "${TARGET_PLATFORM}" PRIVATE)
    if("GraphicsAdapterOpenGLES" IN_LIST _shader_program_graphics_symbols AND
       NOT "GraphicsAdapterOpenGL" IN_LIST _shader_program_graphics_symbols)
      target_compile_definitions(test_app_shader_program PRIVATE DM_TEST_APP_SHADER_PROGRAM_DEFAULT_OPENGLES=1)
    endif()
    if(TARGET_PLATFORM MATCHES "^(arm64-macos|x86_64-macos)$")
      target_link_libraries(test_app_shader_program PRIVATE "-framework IOKit")
    endif()
    if(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
      find_package(Threads REQUIRED)
      target_link_libraries(test_app_shader_program PRIVATE Threads::Threads dl)
    endif()
    defold_target_link_libraries_web(test_app_shader_program "${TARGET_PLATFORM}" library_sys.js library_glfw.js)
    defold_register_test_target(test_app_shader_program OFF)
  endif()
  if(_shader_program_had_with_opengl)
    set(WITH_OPENGL "${_shader_program_saved_with_opengl}")
  else()
    unset(WITH_OPENGL)
  endif()
endif()

# ---------------------------------------------------------------------------
# test_gamesys_physics
# ---------------------------------------------------------------------------
set(TEST_GS_PHYS_SOURCES
  "${GS_TEST_ROOT}/test_gamesys_physics.cpp"
)
set_source_files_properties("${GS_TEST_ROOT}/test_gamesys_physics.cpp" PROPERTIES OBJECT_DEPENDS "${_EMBED_HEADERS}")
# Only this test requires Bullet3D extension registration. Keeping its symbol
# out of _COMMON_EXPORTED prevents unrelated test executables from requiring it.
set(_TEST2_SYMBOLS ${_COMMON_EXPORTED};ScriptModelExt;ScriptBullet3DExt)
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
  target_include_directories(test_gamesys_http PRIVATE "${GS_SRC_ROOT}" "${GS_PROTO_DIR}" "${GS_GENERATED_INCLUDE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
  target_compile_definitions(test_gamesys_http PRIVATE ${_GS_TEST_DEFINES})
  target_link_libraries(test_gamesys_http PRIVATE gamesys gamesys_gui gamesys_particle gamesys_rig_null gamesys_model_null)
  defold_target_link_libraries(test_gamesys_http ${TARGET_PLATFORM} ${_DEFOLD_TEST_LIBS})
  defold_target_link_libraries(test_gamesys_http ${TARGET_PLATFORM} ${_GS_TEST_GRAPHICS_LIBS})
  defold_target_link_bullet3d(test_gamesys_http ${TARGET_PLATFORM})
  defold_target_link_lua(test_gamesys_http ${TARGET_PLATFORM})
  defold_target_link_glfw(test_gamesys_http ${TARGET_PLATFORM})
  defold_target_link_platform(test_gamesys_http ${TARGET_PLATFORM})
  defold_target_link_socket(test_gamesys_http ${TARGET_PLATFORM})
  defold_target_link_app(test_gamesys_http ${TARGET_PLATFORM})
  defold_register_test_target(test_gamesys_http FALSE "${GS_TEST_RUN_DIR}"
    RUNTIME_DEPENDS gamesys_http_test_content
    STAGE_FILES
      "build/src/gamesys/test" "build/src/gamesys/test"
      ${_GS_HOST_TEST_STAGE_FILES})
  # Physical iOS devices cannot reliably reach a host-side test server in
  # unattended runs because local network permission is granted interactively.
  if(NOT TARGET_PLATFORM STREQUAL "arm64-ios")
    defold_register_test_with_server(test_gamesys_http ${TARGET_PLATFORM}
      PORT 9001
      WORKDIR "${GS_TEST_RUN_DIR}"
      STAGE_FILES
        "build/src/gamesys/test" "build/src/gamesys/test"
        ${_GS_HOST_TEST_STAGE_FILES})
  endif()
endif()
