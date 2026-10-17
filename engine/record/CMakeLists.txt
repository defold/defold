cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)
project(defold_record LANGUAGES ${DEFOLD_LANGUAGE_LIST})

set(RECORD_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(RECORD_SRC_DIR "${RECORD_ROOT}/src/record")

defold_add_library(record_null STATIC src/record/record_null.cpp src/record/record_common.cpp)
target_include_directories(record_null PUBLIC "${RECORD_ROOT}/src")
defold_target_link_libraries(record_null "${TARGET_PLATFORM}" SCOPE PUBLIC dlib)
set_target_properties(record_null PROPERTIES DEFOLD_SDK_HEADERS_TARGET record_sdk_headers)
set(RECORD_INSTALL_TARGETS record_null)

defold_feature_disabled(record RECORD_FEATURE_DISABLED)
if(NOT RECORD_FEATURE_DISABLED AND TARGET_PLATFORM MATCHES "(macos|linux|win32)$")
  defold_add_library(record STATIC src/record/record.cpp src/record/record_common.cpp)
  target_include_directories(record PUBLIC "${RECORD_ROOT}/src")
  defold_target_link_libraries(record "${TARGET_PLATFORM}" SCOPE PUBLIC dlib)
  if(TARGET_PLATFORM MATCHES "macos$")
    target_sources(record PRIVATE src/record/record_osx.mm)
    target_link_libraries(record PUBLIC
      "-framework AVFoundation" "-framework AppKit" "-framework CoreMedia"
      "-framework CoreVideo" "-framework Foundation" "-weak_framework ScreenCaptureKit")
  elseif(TARGET_PLATFORM MATCHES "linux$")
    target_sources(record PRIVATE src/record/record_linux.cpp)
    target_include_directories(record PRIVATE "${DEFOLD_HOME}/external/minimp4")
    defold_target_link_libraries(record "${TARGET_PLATFORM}" SCOPE PUBLIC openh264)
  else()
    target_sources(record PRIVATE src/record/record_win32.cpp src/record/record_audio_win32.cpp)
    # Windows SDK / WRL declarations require C++17, confined to this backend.
    set_source_files_properties(src/record/record_win32.cpp src/record/record_audio_win32.cpp
      PROPERTIES COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/std:c++17,-std=c++17>")
    target_link_libraries(record PUBLIC dxgi dwmapi mfuuid ole32)
  endif()
  list(APPEND RECORD_INSTALL_TARGETS record)
endif()

# The service is independently linkable with either the native or null backend.
# Release engines omit this library; headless debug engines can report unsupported.
if(NOT RECORD_FEATURE_DISABLED AND TARGET_PLATFORM MATCHES "(macos|linux|win32)$")
  defold_add_library(record_service STATIC
    src/record/record_service.cpp src/record/record_file.cpp src/record/record_extension.cpp)
  target_include_directories(record_service PUBLIC "${RECORD_ROOT}/src")
  defold_target_link_libraries(record_service "${TARGET_PLATFORM}" SCOPE PUBLIC dlib)
  list(APPEND RECORD_INSTALL_TARGETS record_service)
endif()

foreach(target IN LISTS RECORD_INSTALL_TARGETS)
  target_compile_definitions(${target} PRIVATE DLIB_LOG_DOMAIN="RECORD")
endforeach()
defold_install_targets(TARGETS ${RECORD_INSTALL_TARGETS}
  ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
  LIBRARY DESTINATION lib/${TARGET_PLATFORM}
  RUNTIME DESTINATION bin/${TARGET_PLATFORM})
defold_add_sdk_headers_target(record_sdk_headers
  DESTINATION include/record FILES "${RECORD_SRC_DIR}/record.h"
  DEPENDS record_null dlib_sdk_headers)
install(FILES "${RECORD_SRC_DIR}/record.h" DESTINATION include/record)

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
