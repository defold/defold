cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_profiler LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="PROFILER")

set(PROFILER_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(PROFILER_SRC_DIR "${PROFILER_ROOT}/src")
set(PROFILER_DEPENDENCIES
  dlib
  zip
  mbedtls
  ddf
  resource
  extension
  script
  graphics_null
  image
  graphics_proto
  render
  render_font_default
  font
  lua)
set(PROFILER_BASIC_DEPENDENCIES dlib)
set(PROFILER_JS_DEPENDENCIES dlib script)

set(PROFILER_SOURCES
  "${PROFILER_SRC_DIR}/profiler.cpp"
  "${PROFILER_SRC_DIR}/profiler_render.cpp"
  "${PROFILER_SRC_DIR}/basic/profiler_basic.cpp")

if(TARGET_PLATFORM MATCHES "macos|ios")
  list(APPEND PROFILER_SOURCES "${PROFILER_SRC_DIR}/profiler_cocoa.mm")
elseif(TARGET_PLATFORM MATCHES "android")
  list(APPEND PROFILER_SOURCES
    "${PROFILER_SRC_DIR}/profiler_android.cpp"
    "${PROFILER_SRC_DIR}/profiler_proc_utils.cpp")
elseif(TARGET_PLATFORM MATCHES "linux")
  list(APPEND PROFILER_SOURCES
    "${PROFILER_SRC_DIR}/profiler_linux.cpp"
    "${PROFILER_SRC_DIR}/profiler_proc_utils.cpp")
elseif(TARGET_PLATFORM MATCHES "win32")
  list(APPEND PROFILER_SOURCES "${PROFILER_SRC_DIR}/profiler_win32.cpp")
elseif(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
  list(APPEND PROFILER_SOURCES "${PROFILER_SRC_DIR}/profiler_web.cpp")
else()
  list(APPEND PROFILER_SOURCES "${PROFILER_SRC_DIR}/profiler_unsupported.cpp")
endif()

defold_add_library(profilerext STATIC ${PROFILER_SOURCES})
target_include_directories(profilerext PUBLIC "${PROFILER_SRC_DIR}")
target_compile_definitions(profilerext PRIVATE "DLIB_LOG_DOMAIN=\"PROFILER\"")
defold_target_link_libraries(profilerext "${TARGET_PLATFORM}" SCOPE PUBLIC ${PROFILER_DEPENDENCIES})
if(TARGET_PLATFORM MATCHES "win32")
  target_link_libraries(profilerext PUBLIC Psapi.lib)
endif()

defold_add_library(profilerext_null STATIC "${PROFILER_SRC_DIR}/profiler_null.cpp")
target_include_directories(profilerext_null PUBLIC "${PROFILER_SRC_DIR}")
target_compile_definitions(profilerext_null PRIVATE "DLIB_LOG_DOMAIN=\"PROFILER\"")
defold_target_link_libraries(profilerext_null "${TARGET_PLATFORM}" SCOPE PUBLIC ${PROFILER_BASIC_DEPENDENCIES})
set_target_properties(profilerext_null PROPERTIES DEFOLD_SDK_HEADERS_TARGET profilerext_sdk_headers)

set(PROFILER_INSTALL_TARGETS profilerext profilerext_null)
if(NOT TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web|arm64-nx64|x86_64-ps4|x86_64-ps5|x86_64-xbone)$")
  defold_add_library(profiler_remotery STATIC
    "${PROFILER_SRC_DIR}/remotery/profiler_remotery.cpp"
    "${PROFILER_SRC_DIR}/remotery/lib/Remotery.c")
  target_include_directories(profiler_remotery PUBLIC "${PROFILER_SRC_DIR}")
  target_compile_definitions(profiler_remotery PRIVATE "DLIB_LOG_DOMAIN=\"PROFILER\"")
  defold_target_link_libraries(profiler_remotery "${TARGET_PLATFORM}" SCOPE PUBLIC ${PROFILER_BASIC_DEPENDENCIES})
  list(APPEND PROFILER_INSTALL_TARGETS profiler_remotery)
endif()

if(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
  defold_add_library(profiler_js STATIC "${PROFILER_SRC_DIR}/js/profiler_js.cpp")
  target_include_directories(profiler_js PUBLIC "${PROFILER_SRC_DIR}")
  target_compile_definitions(profiler_js PRIVATE "DLIB_LOG_DOMAIN=\"PROFILER\"")
  defold_target_link_libraries(profiler_js "${TARGET_PLATFORM}" SCOPE PUBLIC ${PROFILER_JS_DEPENDENCIES})
  list(APPEND PROFILER_INSTALL_TARGETS profiler_js)
  defold_install_js_library("${PROFILER_SRC_DIR}/js/library_profile.js")
endif()

defold_install_targets(TARGETS ${PROFILER_INSTALL_TARGETS}
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})

defold_add_sdk_headers_target(profilerext_sdk_headers
  DESTINATION include/profiler
  SOURCE_ROOT "${PROFILER_SRC_DIR}"
  FILES "${PROFILER_SRC_DIR}/profiler.h"
  DEPENDS profilerext)

install(FILES "${PROFILER_SRC_DIR}/profiler.h" DESTINATION include/profiler)

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
