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
    render
    render_font_default)
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
    defold_target_link_libraries(${target} "${TARGET_PLATFORM}" render render_font_default)
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
