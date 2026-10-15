set(GRAPHICS_TEST_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}")

defold_create_exported_symbols_file(
  "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols_graphics_null.cpp"
  "GraphicsAdapterNull")

defold_add_executable(test_graphics
  "${GRAPHICS_TEST_SRC_DIR}/test_graphics.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols_graphics_null.cpp")
target_include_directories(test_graphics PRIVATE
  "${GRAPHICS_SRC_DIR}"
  "${GRAPHICS_PROTO_ROOT}")
defold_target_link_libraries(test_graphics "${TARGET_PLATFORM}"
  testmain
  ddf
  dlib
  profile_null
  platform_null
  graphics_null
  graphics_transcoder_null)
defold_target_link_socket(test_graphics "${TARGET_PLATFORM}" PRIVATE)
if(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
  find_package(Threads REQUIRED)
  target_link_libraries(test_graphics PRIVATE Threads::Threads dl)
endif()
defold_register_test_target(test_graphics ON "${GRAPHICS_TEST_SRC_DIR}")

# Needs the real basis transcoder (test_graphics above links the null one, where Transcode is a stub)
defold_add_executable(test_transcoder
  "${GRAPHICS_TEST_SRC_DIR}/test_transcoder.cpp")
target_include_directories(test_transcoder PRIVATE
  "${GRAPHICS_SRC_DIR}"
  "${GRAPHICS_PROTO_ROOT}")
defold_target_link_libraries(test_transcoder "${TARGET_PLATFORM}"
  testmain
  ddf
  dlib
  profile_null
  graphics_null
  graphics_transcoder_basisu
  basis_transcoder)
defold_target_link_socket(test_transcoder "${TARGET_PLATFORM}" PRIVATE)
if(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
  find_package(Threads REQUIRED)
  target_link_libraries(test_transcoder PRIVATE Threads::Threads dl)
endif()
defold_register_test_target(test_transcoder ON "${GRAPHICS_TEST_SRC_DIR}")

# Headless WARP tests; require the Windows Graphics Tools debug layer.
if(TARGET graphics_dx12 AND TARGET_PLATFORM STREQUAL "x86_64-win32")
  defold_add_executable(test_graphics_dx12 "${GRAPHICS_TEST_SRC_DIR}/test_graphics_dx12.cpp")
  target_include_directories(test_graphics_dx12 PRIVATE "${GRAPHICS_SRC_DIR}" "${GRAPHICS_PROTO_ROOT}")
  defold_target_link_libraries(test_graphics_dx12 "${TARGET_PLATFORM}"
    testmain ddf dlib profile_null graphics_dx12 graphics_transcoder_null)
  defold_target_link_platform(test_graphics_dx12 "${TARGET_PLATFORM}" SCOPE PRIVATE)
  defold_target_link_glfw(test_graphics_dx12 "${TARGET_PLATFORM}" SCOPE PRIVATE)
  target_link_libraries(test_graphics_dx12 PRIVATE d3d12 dxgi d3dcompiler)
  defold_target_link_socket(test_graphics_dx12 "${TARGET_PLATFORM}" PRIVATE)
  option(DEFOLD_RUN_DX12_TESTS "Run the WARP/debug-layer DX12 suite in run_tests" OFF)
  defold_register_test_target(test_graphics_dx12 ${DEFOLD_RUN_DX12_TESTS})
endif()

if(WITH_METAL AND TARGET graphics_metal)
  defold_add_executable(test_graphics_metal "${GRAPHICS_TEST_SRC_DIR}/test_graphics_metal.mm")
  target_include_directories(test_graphics_metal PRIVATE "${GRAPHICS_SRC_DIR}" "${GRAPHICS_PROTO_ROOT}")
  defold_target_link_libraries(test_graphics_metal "${TARGET_PLATFORM}"
    testmain ddf dlib profile_null platform_null graphics_metal graphics_transcoder_null)
  defold_target_link_graphics(test_graphics_metal "${TARGET_PLATFORM}" SCOPE PRIVATE)
  if(TARGET_PLATFORM MATCHES "^(arm64-macos|x86_64-macos)$")
    target_link_libraries(test_graphics_metal PRIVATE "-framework IOKit")
  endif()
  defold_register_test_target(test_graphics_metal OFF)
endif()

if(NOT TARGET_PLATFORM MATCHES "^(x86_64-ps4|x86_64-ps5)$")
  defold_get_graphics_symbols(_graphics_app_symbols "${TARGET_PLATFORM}")

  if(_graphics_app_symbols)
    defold_create_exported_symbols_file(
      "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols_graphics_app.cpp"
      "${_graphics_app_symbols}")

    defold_add_executable(test_app_graphics
      "${GRAPHICS_TEST_SRC_DIR}/test_app_graphics.cpp"
      "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols_graphics_app.cpp")
    target_include_directories(test_app_graphics PRIVATE
      "${GRAPHICS_SRC_DIR}"
      "${GRAPHICS_PROTO_ROOT}")
    if(TARGET_PLATFORM_OS STREQUAL "xbone")
      target_include_directories(test_app_graphics PRIVATE
        "${DEFOLD_XBONE_PRIVATE_REPO_ROOT}/engine/graphics/src/test")
    endif()
    defold_target_link_libraries(test_app_graphics "${TARGET_PLATFORM}"
      testmain
      ddf
      dlib
      profile_null)
    defold_target_link_graphics(test_app_graphics "${TARGET_PLATFORM}" SCOPE PRIVATE)
    defold_target_link_platform(test_app_graphics "${TARGET_PLATFORM}" SCOPE PRIVATE)
    defold_target_link_socket(test_app_graphics "${TARGET_PLATFORM}" PRIVATE)
    if("GraphicsAdapterDX12" IN_LIST _graphics_app_symbols)
      target_compile_definitions(test_app_graphics PRIVATE DM_TEST_APP_GRAPHICS_HAS_DX12=1)
    endif()
    if("GraphicsAdapterVulkan" IN_LIST _graphics_app_symbols)
      target_compile_definitions(test_app_graphics PRIVATE DM_TEST_APP_GRAPHICS_HAS_VULKAN=1)
    endif()
    if("GraphicsAdapterOpenGL" IN_LIST _graphics_app_symbols)
      target_compile_definitions(test_app_graphics PRIVATE DM_TEST_APP_GRAPHICS_HAS_OPENGL=1)
    endif()
    if("GraphicsAdapterOpenGLES" IN_LIST _graphics_app_symbols)
      target_compile_definitions(test_app_graphics PRIVATE DM_TEST_APP_GRAPHICS_HAS_OPENGLES=1)
    endif()
    if("GraphicsAdapterMetal" IN_LIST _graphics_app_symbols)
      target_compile_definitions(test_app_graphics PRIVATE DM_TEST_APP_GRAPHICS_HAS_METAL=1)
    endif()
    if("GraphicsAdapterWebGPU" IN_LIST _graphics_app_symbols AND TARGET_PLATFORM STREQUAL "arm64-macos")
      target_sources(test_app_graphics PRIVATE "${GRAPHICS_TEST_SRC_DIR}/test_app_graphics_webgpu_macos.mm")
    endif()
    if("GraphicsAdapterVulkan" IN_LIST _graphics_app_symbols AND TARGET_PLATFORM MATCHES "^(arm64-macos|x86_64-macos|arm64-ios|arm64-nx64)$")
      target_compile_definitions(test_app_graphics PRIVATE DM_TEST_APP_GRAPHICS_DEFAULT_VULKAN=1)
    elseif("GraphicsAdapterMetal" IN_LIST _graphics_app_symbols)
      target_compile_definitions(test_app_graphics PRIVATE DM_TEST_APP_GRAPHICS_DEFAULT_METAL=1)
    elseif("GraphicsAdapterOpenGLES" IN_LIST _graphics_app_symbols)
      target_compile_definitions(test_app_graphics PRIVATE DM_TEST_APP_GRAPHICS_DEFAULT_OPENGLES=1)
    else()
      target_compile_definitions(test_app_graphics PRIVATE DM_TEST_APP_GRAPHICS_DEFAULT_OPENGL=1)
    endif()
    if(TARGET_PLATFORM MATCHES "^(arm64-macos|x86_64-macos)$")
      target_link_libraries(test_app_graphics PRIVATE "-framework IOKit")
    endif()
    if(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
      find_package(Threads REQUIRED)
      target_link_libraries(test_app_graphics PRIVATE Threads::Threads dl)
    endif()
    defold_target_link_libraries_web(test_app_graphics "${TARGET_PLATFORM}" library_sys.js)
    defold_register_test_target(test_app_graphics OFF)
    if((NOT CMAKE_CROSSCOMPILING AND TARGET_PLATFORM MATCHES "^(arm64-macos|x86_64-macos|arm64-linux|x86_64-linux|x86_64-win32)$") OR TARGET_PLATFORM STREQUAL "arm64_sim-ios")
      # Follow fontviewer: generate aligned embedded assets in the build directory.
      set(_graphics_capture_generated "${CMAKE_CURRENT_BINARY_DIR}/graphics_capture_generated")
      set(_graphics_capture_shaders vp fp cube.fp vp.msl fp.msl cube.fp.msl vp.wgsl fp.wgsl cube.fp.wgsl texture.fp texture.fp.msl texture.fp.wgsl)
      if("GraphicsAdapterVulkan" IN_LIST _graphics_app_symbols)
        find_program(_graphics_glslang NAMES glslang glslangValidator
          HINTS "${DEFOLD_SDK_ROOT}/ext/bin/${HOST_PLATFORM}" REQUIRED)
        foreach(_shader IN ITEMS vert frag cube.frag texture.frag)
          if(_shader STREQUAL "vert")
            set(_stage vert)
          else()
            set(_stage frag)
          endif()
          set(_spirv "${_graphics_capture_generated}/graphics_capture.${_shader}.spv")
          add_custom_command(OUTPUT "${_spirv}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${_graphics_capture_generated}"
            COMMAND "${_graphics_glslang}" -V -S "${_stage}" "${GRAPHICS_TEST_SRC_DIR}/graphics_capture.${_shader}" -o "${_spirv}"
            DEPENDS "${GRAPHICS_TEST_SRC_DIR}/graphics_capture.${_shader}" "${_graphics_glslang}"
            VERBATIM)
          list(APPEND _graphics_capture_shaders "${_shader}.spv")
        endforeach()
      endif()
      foreach(_shader IN LISTS _graphics_capture_shaders)
        if(_shader MATCHES "\\.spv$")
          set(_source "${_graphics_capture_generated}/graphics_capture.${_shader}")
        else()
          set(_source "${GRAPHICS_TEST_SRC_DIR}/graphics_capture.${_shader}")
        endif()
        string(REPLACE "." "_" _symbol "${_shader}")
        defold_embed_to_header(INPUT "${_source}"
          OUTPUT_HEADER "${_graphics_capture_generated}/graphics_capture.${_shader}.h"
          OUTPUT_CPP "${_graphics_capture_generated}/graphics_capture.${_shader}.cpp"
          SYMBOL "capture_${_symbol}" OUT_HEADER_VAR _header OUT_CPP_VAR _cpp)
        target_sources(test_app_graphics PRIVATE "${_header}" "${_cpp}")
      endforeach()
      target_include_directories(test_app_graphics PRIVATE "${_graphics_capture_generated}")
      target_sources(test_app_graphics PRIVATE "${GRAPHICS_TEST_SRC_DIR}/graphics_capture.cpp")
      set(_graphics_texture_dir "${GRAPHICS_TEST_SRC_DIR}/texture_formats")
      if(TARGET_PLATFORM STREQUAL "arm64_sim-ios")
        set(_graphics_texture_dir "texture_formats")
      endif()
      target_compile_definitions(test_app_graphics PRIVATE DM_TEST_GRAPHICS_CAPTURE
        DM_TEST_GRAPHICS_PLATFORM="${TARGET_PLATFORM}"
        DM_TEST_TEXTURE_FORMATS_DIR="${_graphics_texture_dir}")

      _defold_find_python(_graphics_python)
      # Compile fixture variants with the freshly built host tool. Cross builds
      # consume the checked-in variants and never execute a target-platform tool.
      if(NOT CMAKE_CROSSCOMPILING AND TARGET shaderc_standalone)
        find_program(_graphics_glslang NAMES glslang glslangValidator
          HINTS "${DEFOLD_SDK_ROOT}/ext/bin/${HOST_PLATFORM}" REQUIRED)
        # Some Tint packages lack execute permission; the generator stages an executable copy.
        find_file(_graphics_tint NAMES "tint${CMAKE_EXECUTABLE_SUFFIX}"
          HINTS "${DEFOLD_SDK_ROOT}/ext/bin/${HOST_PLATFORM}" REQUIRED)
        set(_graphics_shader_tests "${_graphics_python}" "${GRAPHICS_TEST_SRC_DIR}/test_graphics_shaders.py"
          --glslang "${_graphics_glslang}" --shaderc "$<TARGET_FILE:shaderc_standalone>"
          --tint "${_graphics_tint}" --java "${DEFOLD_JAVA_EXECUTABLE}")
        add_custom_target(run_test_graphics_shaders
          COMMAND ${_graphics_shader_tests}
          DEPENDS shaderc_standalone
          VERBATIM)
        defold_add_to_run_tests(run_test_graphics_shaders RUN_GROUP graphics
          COMMAND ${_graphics_shader_tests} DEPENDS shaderc_standalone)
        defold_register_sequential_test_command(graphics_shaders
          COMMAND ${_graphics_shader_tests} DEPENDS shaderc_standalone)
      endif()
      defold_test_run_settings(_graphics_runner _graphics_run_options graphics)
      if(TARGET_PLATFORM STREQUAL "arm64_sim-ios")
        set(_graphics_matrix metal vulkan)
      elseif(TARGET_PLATFORM MATCHES "macos$")
        set(_graphics_matrix metal opengl vulkan webgpu)
      elseif(TARGET_PLATFORM MATCHES "linux$")
        set(_graphics_matrix vulkan)
      else()
        set(_graphics_matrix opengl vulkan)
      endif()
      set(_graphics_available)
      foreach(_backend IN LISTS _graphics_matrix)
        if(_backend STREQUAL "metal")
          set(_symbol GraphicsAdapterMetal)
        elseif(_backend STREQUAL "opengl")
          set(_symbol GraphicsAdapterOpenGL)
        elseif(_backend STREQUAL "vulkan")
          set(_symbol GraphicsAdapterVulkan)
        elseif(_backend STREQUAL "webgpu")
          set(_symbol GraphicsAdapterWebGPU)
        endif()
        if(_symbol IN_LIST _graphics_app_symbols)
          list(APPEND _graphics_available "${_backend}")
        endif()
      endforeach()
      set(_graphics_capture_env)
      if(NOT TARGET_PLATFORM STREQUAL "arm64_sim-ios")
        list(APPEND _graphics_capture_env "GRAPHICS_CAPTURE_EXECUTABLE=$<TARGET_FILE:test_app_graphics>")
      endif()
      set(_graphics_python_command "${CMAKE_COMMAND}" -E env ${_graphics_capture_env}
        "${_graphics_python}" "${GRAPHICS_TEST_SRC_DIR}/test_graphics_images.py")
      set(_graphics_image_command "${_graphics_python}" "${GRAPHICS_TEST_SRC_DIR}/run_graphics_images.py"
        --executable "$<TARGET_FILE:test_app_graphics>"
        --target-platform "${TARGET_PLATFORM}"
        --capture-dir "${GRAPHICS_ROOT}/build/graphics-test-images/${TARGET_PLATFORM}"
        --output "${GRAPHICS_ROOT}/build/graphics-render-report/${TARGET_PLATFORM}"
        --matrix ${_graphics_matrix} --available ${_graphics_available})
      add_custom_target(run_test_graphics_images_python
        COMMAND ${_graphics_runner} ${_graphics_python_command}
        DEPENDS test_app_graphics
        ${_graphics_run_options} COMMENT "Testing graphics image capture and reporting")
      add_custom_target(generate_graphics_test_images
        COMMAND ${_graphics_runner} ${_graphics_image_command}
        DEPENDS test_app_graphics run_test_graphics_images_python
        ${_graphics_run_options} COMMENT "Rendering the graphics likeness matrix")

      # Automatic hosted CI coverage is Linux/Vulkan and the iOS simulator. The manual target
      # above always remains available, regardless of this policy.
      set(_graphics_ci_command ${_graphics_image_command})
      if(DEFINED ENV{GITHUB_ACTIONS} AND NOT TARGET_PLATFORM MATCHES "linux$|^arm64_sim-ios$")
        list(APPEND _graphics_ci_command --skip-all "Hosted CI graphics captures explicitly disabled for ${TARGET_PLATFORM}")
        message(STATUS "Graphics captures skipped on hosted ${TARGET_PLATFORM} CI; manual target remains available")
      endif()
      add_custom_target(run_graphics_test_images
        COMMAND ${_graphics_runner} ${_graphics_ci_command}
        DEPENDS test_app_graphics
        ${_graphics_run_options} COMMENT "Running graphics likeness tests with CI policy")
      if(NOT CMAKE_GENERATOR STREQUAL "Xcode")
        defold_add_to_run_tests(run_test_graphics_images_python RUN_GROUP graphics
          COMMAND ${_graphics_python_command} DEPENDS test_app_graphics)
        # This runner command carries the CI policy, while the manual target does not.
        defold_add_to_run_tests(run_graphics_test_images RUN_GROUP graphics
          COMMAND ${_graphics_ci_command} DEPENDS test_app_graphics)
      endif()
      defold_register_sequential_test_command(graphics_images_python
        COMMAND ${_graphics_python_command} DEPENDS test_app_graphics)
      defold_register_sequential_test_command(graphics_images
        COMMAND ${_graphics_ci_command} DEPENDS test_app_graphics)
    endif()
  endif()
endif()
