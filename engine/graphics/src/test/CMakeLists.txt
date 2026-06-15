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

if(NOT TARGET_PLATFORM MATCHES "^(x86_64-ios|x86_64-ps4|x86_64-ps5)$")
  set(_graphics_app_libs graphics graphics_transcoder_basisu)
  set(_graphics_app_platform_lib platform)
  set(_graphics_app_link_glfw ON)
  set(_graphics_app_extra_libs)
  set(_graphics_app_symbols)
  set(_graphics_app_build_vulkan OFF)
  if(GRAPHICS_BUILD_VULKAN AND WITH_VULKAN)
    set(_graphics_app_build_vulkan ON)
  endif()
  if(TARGET_PLATFORM STREQUAL "x86-win32")
    set(_graphics_app_build_vulkan OFF)
  endif()

  if(GRAPHICS_PLATFORM_SUPPORTS_OPENGL)
    if(TARGET_PLATFORM STREQUAL "arm64-linux")
      set(_graphics_app_libs graphics_opengles graphics_transcoder_basisu)
      list(APPEND _graphics_app_symbols GraphicsAdapterOpenGLES)
    else()
      list(APPEND _graphics_app_symbols GraphicsAdapterOpenGL)
    endif()
  else()
    set(_graphics_app_link_glfw OFF)
  endif()

  if(TARGET_PLATFORM MATCHES "^(armv7-android|arm64-android)$")
    list(APPEND _graphics_app_extra_libs graphics)
  endif()

  if(_graphics_app_build_vulkan)
    list(APPEND _graphics_app_extra_libs graphics_vulkan)
    list(APPEND _graphics_app_symbols GraphicsAdapterVulkan)
    defold_get_glfw_version(_graphics_app_glfw_version "${TARGET_PLATFORM}")
    if(_graphics_app_glfw_version EQUAL 3)
      set(_graphics_app_platform_lib platform_vulkan)
    endif()
  endif()

  if(GRAPHICS_BUILD_DX12)
    list(APPEND _graphics_app_extra_libs graphics_dx12)
    list(APPEND _graphics_app_symbols GraphicsAdapterDX12)
  endif()

  if(GRAPHICS_BUILD_WEBGPU)
    list(APPEND _graphics_app_extra_libs graphics_webgpu)
    list(APPEND _graphics_app_symbols GraphicsAdapterWebGPU)
  endif()

  if(TARGET_PLATFORM STREQUAL "arm64-nx64")
    set(_graphics_app_link_glfw OFF)
  endif()

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
      profile_null
      ${_graphics_app_libs}
      ${_graphics_app_platform_lib}
      ${_graphics_app_extra_libs})
    defold_target_link_socket(test_app_graphics "${TARGET_PLATFORM}" PRIVATE)
    if(_graphics_app_link_glfw)
      defold_target_link_glfw(test_app_graphics "${TARGET_PLATFORM}" PRIVATE)
    endif()
    if(GRAPHICS_PLATFORM_SUPPORTS_OPENGL)
      defold_target_link_opengl(test_app_graphics "${TARGET_PLATFORM}" PRIVATE)
    endif()
    if(_graphics_app_build_vulkan)
      if(TARGET_PLATFORM MATCHES "win32$")
        target_link_libraries(test_app_graphics PRIVATE vulkan-1.lib)
      elseif(TARGET_PLATFORM MATCHES "^(arm64-linux|x86_64-linux)$")
        target_link_libraries(test_app_graphics PRIVATE vulkan X11-xcb)
        if(WITH_VULKAN_VALIDATION AND TARGET_PLATFORM STREQUAL "arm64-linux")
          target_link_libraries(test_app_graphics PRIVATE VkLayer_khronos_validation)
        endif()
      elseif(TARGET_PLATFORM MATCHES "^(arm64-macos|x86_64-macos)$")
        if(WITH_VULKAN_VALIDATION)
          target_link_libraries(test_app_graphics PRIVATE vulkan)
        else()
          target_link_libraries(test_app_graphics PRIVATE MoltenVK)
        endif()
        target_link_libraries(test_app_graphics PRIVATE "-framework Metal" "-framework IOSurface" "-framework QuartzCore")
      endif()
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
  endif()
endif()
