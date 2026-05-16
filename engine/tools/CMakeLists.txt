cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_tools LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="TOOLS")

set(TOOLS_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(TOOLS_SRC_DIR "${TOOLS_ROOT}/src")

execute_process(
  COMMAND git rev-parse HEAD
  WORKING_DIRECTORY "${DEFOLD_HOME}"
  OUTPUT_VARIABLE TOOLS_DEFOLD_SHA1
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET)
if(NOT TOOLS_DEFOLD_SHA1)
  set(TOOLS_DEFOLD_SHA1 "unknown")
endif()
add_compile_definitions(DEFOLD_SHA1="${TOOLS_DEFOLD_SHA1}")

install(FILES
  "${TOOLS_ROOT}/scripts/output_stream_fast.py"
  "${TOOLS_ROOT}/scripts/watch.py"
  DESTINATION bin
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

set(TOOLS_IS_DESKTOP OFF)
if(TARGET_PLATFORM MATCHES "macos|linux|win32$")
  set(TOOLS_IS_DESKTOP ON)
endif()

if(TOOLS_IS_DESKTOP)
  if(NOT WITH_VULKAN)
    defold_get_graphics_symbols(TOOLS_GDC_EXPORTS "${TARGET_PLATFORM}")
    set(TOOLS_GDC_EXPORT_FILE "${CMAKE_CURRENT_BINARY_DIR}/gdc_exported_symbols.cpp")
    defold_create_exported_symbols_file("${TOOLS_GDC_EXPORT_FILE}" "${TOOLS_GDC_EXPORTS}")
    defold_add_executable(gdc
      "${TOOLS_SRC_DIR}/gdc/main.cpp"
      "${TOOLS_GDC_EXPORT_FILE}")
    defold_target_link_libraries(gdc "${TARGET_PLATFORM}"
      hid
      input
      ddf
      dlib
      zip
      mbedtls
      profile_null)
    defold_target_link_platform(gdc "${TARGET_PLATFORM}" SCOPE PRIVATE)
    defold_target_link_app(gdc "${TARGET_PLATFORM}" SCOPE PRIVATE)
    defold_target_link_graphics(gdc "${TARGET_PLATFORM}" SCOPE PRIVATE)
    defold_target_link_socket(gdc "${TARGET_PLATFORM}" PRIVATE)
    if(TARGET_PLATFORM MATCHES "^(x86_64-macos|arm64-macos)$")
      target_link_libraries(gdc PRIVATE "-framework Metal" "-framework Foundation" "-framework IOSurface" "-framework GameController")
    elseif(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
      find_package(Threads REQUIRED)
      target_link_libraries(gdc PRIVATE Threads::Threads dl)
    endif()
    install(TARGETS gdc RUNTIME DESTINATION bin/${TARGET_PLATFORM})
  endif()

  set(TOOLS_LAUNCHER_SOURCES
    "${TOOLS_SRC_DIR}/launcher/launcher.cpp"
    "${TOOLS_SRC_DIR}/launcher/launcher_util.cpp")
  if(TARGET_PLATFORM MATCHES "macos")
    list(APPEND TOOLS_LAUNCHER_SOURCES "${TOOLS_SRC_DIR}/launcher/macos_events.mm")
  elseif(TARGET_PLATFORM MATCHES "win32$")
    list(APPEND TOOLS_LAUNCHER_SOURCES "${TOOLS_SRC_DIR}/launcher/launcher.rc")
  endif()

  defold_add_executable(launcher ${TOOLS_LAUNCHER_SOURCES})
  target_include_directories(launcher PRIVATE "${TOOLS_SRC_DIR}/launcher")
  defold_target_link_libraries(launcher "${TARGET_PLATFORM}" dlib zip mbedtls profile_null)
  if(TARGET_PLATFORM MATCHES "macos")
    target_link_libraries(launcher PRIVATE "-framework Cocoa")
  elseif(TARGET_PLATFORM MATCHES "win32$")
    target_link_options(launcher PRIVATE /subsystem:windows /entry:mainCRTStartup)
  endif()

  defold_add_library(launcherutil STATIC "${TOOLS_SRC_DIR}/launcher/launcher_util.cpp")
  target_include_directories(launcherutil PUBLIC "${TOOLS_SRC_DIR}/launcher")
  defold_target_link_libraries(launcherutil "${TARGET_PLATFORM}" SCOPE PUBLIC dlib zip mbedtls)

  defold_add_executable(texconvert "${TOOLS_SRC_DIR}/texconvert/main.cpp")
  defold_target_link_libraries(texconvert "${TARGET_PLATFORM}"
    dlib
    zip
    mbedtls
    texc
    profile_null
    platform_null
    basis_encoder)

  install(TARGETS launcher launcherutil texconvert
          ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
          LIBRARY DESTINATION lib/${TARGET_PLATFORM}
          RUNTIME DESTINATION bin/${TARGET_PLATFORM})

  if(TARGET_PLATFORM STREQUAL "x86_64-linux")
    set(TOOLS_MOUSE_CAPTURE_SOURCE "${TOOLS_SRC_DIR}/mouse_capture/mouse_capture_linux.cpp")
    set(TOOLS_MOUSE_CAPTURE_LIBS)
  elseif(TARGET_PLATFORM STREQUAL "x86_64-win32")
    set(TOOLS_MOUSE_CAPTURE_SOURCE "${TOOLS_SRC_DIR}/mouse_capture/mouse_capture_windows.cpp")
    set(TOOLS_MOUSE_CAPTURE_LIBS user32.lib)
  elseif(TARGET_PLATFORM MATCHES "macos")
    set(TOOLS_MOUSE_CAPTURE_SOURCE "${TOOLS_SRC_DIR}/mouse_capture/mouse_capture_macos.mm")
    set(TOOLS_MOUSE_CAPTURE_LIBS)
  endif()
  if(TOOLS_MOUSE_CAPTURE_SOURCE)
    add_library(mouse_capture_shared SHARED
      "${TOOLS_SRC_DIR}/mouse_capture/mouse_capture_exports.cpp"
      "${TOOLS_MOUSE_CAPTURE_SOURCE}")
    target_include_directories(mouse_capture_shared PRIVATE "${TOOLS_SRC_DIR}/mouse_capture")
    defold_target_link_libraries(mouse_capture_shared "${TARGET_PLATFORM}" dlib_noasan)
    defold_target_link_socket(mouse_capture_shared "${TARGET_PLATFORM}" PRIVATE)
    target_link_libraries(mouse_capture_shared PRIVATE ${TOOLS_MOUSE_CAPTURE_LIBS})
    install(TARGETS mouse_capture_shared
            LIBRARY DESTINATION lib/${TARGET_PLATFORM}
            RUNTIME DESTINATION bin/${TARGET_PLATFORM})
  endif()

  if(BUILD_TESTS)
    add_subdirectory(src/launcher/test)
  endif()
elseif(BUILD_TESTS)
  add_custom_target(build_tests)
endif()
