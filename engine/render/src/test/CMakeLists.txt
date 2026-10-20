cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

set(RENDER_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(RENDER_ROOT "${RENDER_TEST_ROOT}/../..")
set(RENDER_SRC_DIR "${RENDER_ROOT}/src")

set(RENDER_TEST_EXPORTS GraphicsAdapterNull)
defold_create_exported_symbols_file("${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols.cpp" "${RENDER_TEST_EXPORTS}")

function(render_configure_test target)
  target_include_directories(${target} PRIVATE
    "${RENDER_TEST_ROOT}"
    "${RENDER_SRC_DIR}"
    "${RENDER_ROOT}/proto"
    "${CMAKE_CURRENT_BINARY_DIR}/../..")
  defold_target_link_libraries(${target} "${TARGET_PLATFORM}"
    testmain
    ddf
    dlib
    mbedtls
    profile_null
    platform_null
    graphics_null
    image
    zip
    graphics_proto
    lua
    font
    extension
    resource
    script
    render)
  defold_target_link_socket(${target} "${TARGET_PLATFORM}" PRIVATE)
  defold_target_link_app(${target} "${TARGET_PLATFORM}" SCOPE PRIVATE)
  defold_target_link_glfw(${target} "${TARGET_PLATFORM}" PRIVATE)
  defold_target_link_libraries_web(${target} "${TARGET_PLATFORM}"
    library_sys.js
    library_script.js
    library_render.js)
  if(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
    find_package(Threads REQUIRED)
    target_link_libraries(${target} PRIVATE Threads::Threads dl)
  endif()
  if(TARGET_PLATFORM MATCHES "-macos$" AND NOT TARGET script)
    target_link_options(${target} PRIVATE "-Wl,-force_load,${DEFOLD_LIB_DIR}/libscript.a")
  endif()
  defold_register_test_target(${target} ON "${RENDER_ROOT}")
endfunction()

foreach(_render_test IN ITEMS
  test_render
  test_display_profiles
  test_render_programs
  test_render_script
  test_render_buffer)
  defold_add_executable(${_render_test}
    "${RENDER_TEST_ROOT}/${_render_test}.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols.cpp")
  render_configure_test(${_render_test})
endforeach()

if(NOT CMAKE_CROSSCOMPILING AND TARGET_PLATFORM MATCHES "^(arm64-macos|x86_64-macos|arm64-linux|x86_64-linux|x86_64-win32)$")
  set(_gltf_content_root "${DEFOLD_HOME}/engine/engine/content")
  set(_gltf_shader "${CMAKE_CURRENT_BINARY_DIR}/gltf_render/gltf.spc")
  set(_gltf_bob "${DEFOLD_SDK_ROOT}/share/java/bob-light.jar")
  file(GLOB _gltf_shader_sources CONFIGURE_DEPENDS "${_gltf_content_root}/builtins/materials/*.glsl")
  add_custom_command(OUTPUT "${_gltf_shader}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/gltf_render"
    COMMAND "${DEFOLD_JAVA_EXECUTABLE}" -cp "${_gltf_bob}" com.dynamo.bob.pipeline.ShaderProgramBuilder
      "${_gltf_content_root}/builtins/materials/gltf.vp"
      "${_gltf_content_root}/builtins/materials/gltf.fp"
      "${_gltf_shader}" "${TARGET_PLATFORM}" "${_gltf_content_root}"
    DEPENDS "${_gltf_bob}" ${_gltf_shader_sources}
      "${_gltf_content_root}/builtins/materials/gltf.vp"
      "${_gltf_content_root}/builtins/materials/gltf.fp"
    COMMENT "Compiling production glTF shaders for GPU regression tests"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    VERBATIM)
  add_custom_target(gltf_render_shaders DEPENDS "${_gltf_shader}")
  if(TARGET bob_light)
    add_dependencies(gltf_render_shaders bob_light)
  endif()
  defold_get_graphics_symbols(_gltf_graphics_symbols "${TARGET_PLATFORM}")
  if("GraphicsAdapterMetal" IN_LIST _gltf_graphics_symbols)
    set(_gltf_backend metal)
  elseif("GraphicsAdapterVulkan" IN_LIST _gltf_graphics_symbols)
    set(_gltf_backend vulkan)
  elseif("GraphicsAdapterOpenGLES" IN_LIST _gltf_graphics_symbols)
    set(_gltf_backend opengles)
  else()
    set(_gltf_backend opengl)
  endif()
  defold_create_exported_symbols_file("${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols_gltf.cpp" "${_gltf_graphics_symbols}")
  defold_add_executable(test_gltf_render
    "${RENDER_TEST_ROOT}/test_gltf_render.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols_gltf.cpp")
  target_include_directories(test_gltf_render PRIVATE "${RENDER_SRC_DIR}" "${RENDER_ROOT}/proto" "${CMAKE_CURRENT_BINARY_DIR}/../..")
  target_compile_definitions(test_gltf_render PRIVATE
    DM_TEST_GLTF_SHADER="${_gltf_shader}" DM_TEST_GLTF_BACKEND="${_gltf_backend}")
  defold_target_link_libraries(test_gltf_render "${TARGET_PLATFORM}" testmain render ddf dlib profile_null)
  defold_target_link_graphics(test_gltf_render "${TARGET_PLATFORM}" SCOPE PRIVATE)
  defold_target_link_platform(test_gltf_render "${TARGET_PLATFORM}" SCOPE PRIVATE)
  defold_target_link_app(test_gltf_render "${TARGET_PLATFORM}" SCOPE PRIVATE)
  defold_target_link_socket(test_gltf_render "${TARGET_PLATFORM}" PRIVATE)
  if(TARGET_PLATFORM MATCHES "macos$")
    target_link_libraries(test_gltf_render PRIVATE "-framework IOKit")
  endif()
  # Match the graphics image suite: hosted GPU execution is Linux/Vulkan only.
  set(_gltf_run_tests ON)
  if(DEFINED ENV{GITHUB_ACTIONS} AND NOT TARGET_PLATFORM MATCHES "linux$")
    set(_gltf_run_tests OFF)
  endif()
  defold_register_test_target(test_gltf_render ${_gltf_run_tests}
    RUN_GROUP graphics RUNTIME_DEPENDS gltf_render_shaders)
endif()

# Extend the font image matrix after renderer targets exist. The ordinary font
# unit tests keep their lower-level dependencies and need no graphics context.
if(APPLE AND TARGET test_font_bitmap_gen)
  # Test executables locate the compiler through their runpath.
  set_target_properties(fontc_shared PROPERTIES INSTALL_NAME_DIR "@rpath")
endif()
if(WIN32 AND TARGET test_font_bitmap_gen)
  add_custom_target(font_image_compiler_runtime
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:fontc_shared>" "$<TARGET_FILE_DIR:test_font_bitmap_gen>"
    DEPENDS fontc_shared)
endif()
foreach(target IN ITEMS test_font_bitmap_gen test_font_bitmap_gen_skribidi test_font_bitmap_gen_plain test_font_bitmap_gen_skribidi_plain)
  if(TARGET ${target})
    target_sources(${target} PRIVATE "${RENDER_TEST_ROOT}/font_vector_images.cpp")
    target_include_directories(${target} PRIVATE "${RENDER_SRC_DIR}"
      "${DEFOLD_HOME}/engine/font/src/test" "${DEFOLD_HOME}/engine/font/src/fontc")
    defold_target_link_libraries(${target} "${TARGET_PLATFORM}" render)
    if(target MATCHES "_skribidi")
      target_sources(${target} PRIVATE "${DEFOLD_HOME}/engine/font/src/fontc/fontc.cpp")
    else()
      # Keep the compiler's HarfBuzz dependencies separate from the legacy renderer.
      target_link_libraries(${target} PRIVATE fontc_shared)
      set_target_properties(${target} PROPERTIES
        BUILD_WITH_INSTALL_RPATH ON
        INSTALL_RPATH "$<TARGET_FILE_DIR:fontc_shared>")
      if(WIN32)
        add_dependencies(${target} font_image_compiler_runtime)
      endif()
    endif()
    defold_target_link_app(${target} "${TARGET_PLATFORM}" SCOPE PRIVATE)
  endif()
endforeach()
