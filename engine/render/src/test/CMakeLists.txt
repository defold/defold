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
  test_render_execution
  test_render_frame
  test_render_frame_queue
  test_display_profiles
  test_render_programs
  test_render_script
  test_render_buffer)
  defold_add_executable(${_render_test}
    "${RENDER_TEST_ROOT}/${_render_test}.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols.cpp")
  render_configure_test(${_render_test})
endforeach()

# Native GPU tests share culling/count/pixel coverage across enabled backends.
if(TARGET_PLATFORM MATCHES "^(arm64|x86_64)-macos$" AND "engine" IN_LIST DEFOLD_SELECTED_ENGINE_LIBS)
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/test_visible_instances.spc"
    COMMAND "${Java_JAVA_EXECUTABLE}" ${DEFOLD_JAVA_RUNTIME_FLAGS_LIST}
      -cp "${DEFOLD_SDK_ROOT}/share/java/bob-light.jar"
      com.dynamo.bob.pipeline.ShaderProgramBuilder
      "${RENDER_TEST_ROOT}/test_visible_instances.vp" "${RENDER_TEST_ROOT}/test_visible_instances.fp"
      "${CMAKE_CURRENT_BINARY_DIR}/test_visible_instances.spc" "${TARGET_PLATFORM}" "${RENDER_TEST_ROOT}"
    DEPENDS "${RENDER_TEST_ROOT}/test_visible_instances.vp" "${RENDER_TEST_ROOT}/test_visible_instances.fp"
      "${DEFOLD_SDK_ROOT}/share/java/bob-light.jar"
    VERBATIM)
  add_custom_target(render_gpu_test_shaders
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/test_visible_instances.spc" "${DEFOLD_BUILD_HOME}/engine/engine/build/${TARGET_PLATFORM}/content/materials/debug.spc")
  defold_get_graphics_symbols(_render_gpu_graphics_symbols "${TARGET_PLATFORM}")
  foreach(_gpu_backend IN ITEMS metal vulkan webgpu)
    if(NOT TARGET graphics_${_gpu_backend})
      continue()
    endif()
    if(_gpu_backend STREQUAL "metal")
      set(_gpu_target test_render_gpu)
      set(_gpu_symbol GraphicsAdapterMetal)
    elseif(_gpu_backend STREQUAL "vulkan")
      set(_gpu_target test_render_gpu_vulkan)
      set(_gpu_symbol GraphicsAdapterVulkan)
    else()
      set(_gpu_target test_render_gpu_webgpu)
      set(_gpu_symbol GraphicsAdapterWebGPU)
    endif()
    if(NOT _gpu_symbol IN_LIST _render_gpu_graphics_symbols)
      continue()
    endif()
    defold_create_exported_symbols_file("${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols_${_gpu_target}.cpp" "${_gpu_symbol}")
    defold_add_executable(${_gpu_target}
      "${RENDER_TEST_ROOT}/test_render_gpu.mm"
      "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols_${_gpu_target}.cpp")
    target_include_directories(${_gpu_target} PRIVATE "${RENDER_SRC_DIR}")
    target_compile_definitions(${_gpu_target} PRIVATE
      INDIRECT_SHADER_FILE="${DEFOLD_BUILD_HOME}/engine/engine/build/${TARGET_PLATFORM}/content/materials/culling_instances.spc"
      VISIBLE_INSTANCE_SHADER_FILE="${CMAKE_CURRENT_BINARY_DIR}/test_visible_instances.spc"
      DEBUG_SHADER_FILE="${DEFOLD_BUILD_HOME}/engine/engine/build/${TARGET_PLATFORM}/content/materials/debug.spc")
    if(_gpu_backend STREQUAL "vulkan")
      target_compile_definitions(${_gpu_target} PRIVATE DM_TEST_GPU_VULKAN=1)
      # Inject allocation errors at native calls without production testing hooks.
      set(_vulkan_device_source "${CMAKE_CURRENT_SOURCE_DIR}/../../../graphics/src/vulkan/graphics_vulkan_device.cpp")
      target_sources(${_gpu_target} PRIVATE "${_vulkan_device_source}")
      set_source_files_properties("${_vulkan_device_source}" PROPERTIES
        COMPILE_DEFINITIONS "vkCreateBuffer=TestVkCreateBuffer;vkAllocateMemory=TestVkAllocateMemory")
    elseif(_gpu_backend STREQUAL "webgpu")
      target_compile_definitions(${_gpu_target} PRIVATE DM_TEST_GPU_WEBGPU=1)
    endif()
    add_dependencies(${_gpu_target} engine_culling_instances_shader render_gpu_test_shaders)
    defold_target_link_libraries(${_gpu_target} "${TARGET_PLATFORM}"
      testmain ddf dlib mbedtls profile_null image zip graphics_proto lua font extension resource script render)
    defold_target_link_graphics(${_gpu_target} "${TARGET_PLATFORM}" SCOPE PRIVATE)
    defold_target_link_platform(${_gpu_target} "${TARGET_PLATFORM}" SCOPE PRIVATE)
    defold_target_link_socket(${_gpu_target} "${TARGET_PLATFORM}" PRIVATE)
    defold_target_link_app(${_gpu_target} "${TARGET_PLATFORM}" SCOPE PRIVATE)
    defold_target_link_glfw(${_gpu_target} "${TARGET_PLATFORM}" PRIVATE)
    defold_register_test_target(${_gpu_target} OFF)
  endforeach()
endif()
target_sources(test_render_frame_queue PRIVATE "${RENDER_TEST_ROOT}/render_frame_benchmark.cpp")

# Vector-font tests load sibling font fixtures that are outside Node's mounted
# working directory. Embed them at the paths used by the tests in the web VFS.
if(TARGET_PLATFORM MATCHES "web$")
  foreach(_render_test_font IN ITEMS vera_mo_bd.ttf SourceCodePro-Regular.otf color-test.ttf)
    set(_render_test_font_path "${DEFOLD_HOME}/engine/font/src/test/data/${_render_test_font}")
    target_link_options(test_render PRIVATE
      "SHELL:--embed-file \"${_render_test_font_path}@/font/src/test/data/${_render_test_font}\"")
    set_property(TARGET test_render APPEND PROPERTY LINK_DEPENDS "${_render_test_font_path}")
  endforeach()
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
