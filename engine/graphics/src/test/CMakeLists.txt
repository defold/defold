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

if(NOT TARGET_PLATFORM MATCHES "^(arm64_sim-ios|x86_64-ps4|x86_64-ps5)$")
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
    defold_target_link_libraries(test_app_graphics "${TARGET_PLATFORM}"
      testmain
      ddf
      dlib
      profile_null)
    defold_target_link_graphics(test_app_graphics "${TARGET_PLATFORM}" SCOPE PRIVATE)
    defold_target_link_platform(test_app_graphics "${TARGET_PLATFORM}" SCOPE PRIVATE)
    defold_target_link_socket(test_app_graphics "${TARGET_PLATFORM}" PRIVATE)
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
    defold_target_link_libraries_web(test_app_graphics "${TARGET_PLATFORM}" library_sys.js library_glfw.js)
    defold_register_test_target(test_app_graphics OFF)
    if(NOT CMAKE_CROSSCOMPILING AND TARGET_PLATFORM MATCHES "^(arm64-macos|x86_64-macos|arm64-linux|x86_64-linux|x86_64-win32)$")
      target_sources(test_app_graphics PRIVATE "${GRAPHICS_TEST_SRC_DIR}/graphics_capture.cpp")
      target_compile_definitions(test_app_graphics PRIVATE DM_TEST_GRAPHICS_CAPTURE
        DM_TEST_TEXTURE_FORMATS_DIR="${GRAPHICS_TEST_SRC_DIR}/texture_formats")
      if(WIN32)
        target_link_libraries(test_app_graphics PRIVATE d3dcompiler)
      endif()

      _defold_find_python(_graphics_python)
      defold_test_run_settings(_graphics_runner _graphics_run_options graphics)
      if(TARGET_PLATFORM MATCHES "macos$")
        set(_graphics_matrix metal opengl webgpu)
      elseif(TARGET_PLATFORM MATCHES "linux$")
        set(_graphics_matrix vulkan)
      else()
        set(_graphics_matrix dx12)
      endif()
      set(_graphics_available)
      foreach(_backend IN LISTS _graphics_matrix)
        if(_backend STREQUAL "metal")
          set(_symbol GraphicsAdapterMetal)
        elseif(_backend STREQUAL "opengl")
          set(_symbol GraphicsAdapterOpenGL)
        elseif(_backend STREQUAL "webgpu")
          set(_symbol GraphicsAdapterWebGPU)
          if(NOT TARGET_PLATFORM STREQUAL "arm64-macos")
            continue()
          endif()
        elseif(_backend STREQUAL "vulkan")
          set(_symbol GraphicsAdapterVulkan)
        else()
          set(_symbol GraphicsAdapterDX12)
        endif()
        if(_symbol IN_LIST _graphics_app_symbols)
          list(APPEND _graphics_available "${_backend}")
        endif()
      endforeach()
      set(_graphics_python_command "${CMAKE_COMMAND}" -E env
        "GRAPHICS_CAPTURE_EXECUTABLE=$<TARGET_FILE:test_app_graphics>"
        "${_graphics_python}" "${GRAPHICS_TEST_SRC_DIR}/test_graphics_images.py")
      set(_graphics_image_command "${_graphics_python}" "${GRAPHICS_TEST_SRC_DIR}/run_graphics_images.py"
        --executable "$<TARGET_FILE:test_app_graphics>"
        --capture-dir "${GRAPHICS_ROOT}/build/graphics-test-images"
        --output "${GRAPHICS_ROOT}/build/graphics-render-report"
        --matrix ${_graphics_matrix} --available ${_graphics_available})
      add_custom_target(run_test_graphics_images_python
        COMMAND ${_graphics_runner} ${_graphics_python_command}
        DEPENDS test_app_graphics
        ${_graphics_run_options} COMMENT "Testing graphics image capture and reporting")
      add_custom_target(generate_graphics_test_images
        COMMAND ${_graphics_runner} ${_graphics_image_command}
        DEPENDS test_app_graphics run_test_graphics_images_python
        ${_graphics_run_options} COMMENT "Rendering the graphics likeness matrix")

      # Automatic hosted CI coverage is native Linux/Vulkan. The manual target
      # above always remains available, regardless of this policy.
      set(_graphics_ci_command ${_graphics_image_command})
      if(DEFINED ENV{GITHUB_ACTIONS} AND NOT TARGET_PLATFORM MATCHES "linux$")
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
