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
  endif()
endif()
