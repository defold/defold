cmake_minimum_required(VERSION 4.0)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)
project(defold_automation LANGUAGES ${DEFOLD_LANGUAGE_LIST})

defold_add_library(automation_null STATIC src/automation_null.cpp)
target_include_directories(automation_null PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
defold_install_targets(TARGETS automation_null
  ARCHIVE DESTINATION lib/${TARGET_PLATFORM})

if(NOT TARGET_PLATFORM MATCHES "(macos|linux|win32|android|ios)$")
  return()
endif()

defold_add_library(automation STATIC
  src/automation.cpp
  src/automation_artifact.cpp
  src/automation_application.cpp
  src/automation_engine_access.cpp
  src/automation_http.cpp
  src/automation_json.cpp
  src/automation_recording_async.cpp
  src/automation_input.cpp
  src/automation_scene.cpp
  src/automation_util.cpp)

set(AUTOMATION_CAPTURE_SOURCES
  src/automation_screenshot.cpp
  src/automation_metal.cpp
  src/automation_recording.cpp)
defold_add_library(automation_capture_null STATIC ${AUTOMATION_CAPTURE_SOURCES})
target_compile_definitions(automation_capture_null PRIVATE DM_AUTOMATION_HEADLESS=1)
if(TARGET_PLATFORM MATCHES "macos$")
  list(APPEND AUTOMATION_CAPTURE_SOURCES src/automation_metal.mm src/automation_recording_osx.mm)
elseif(TARGET_PLATFORM MATCHES "win32$")
  list(APPEND AUTOMATION_CAPTURE_SOURCES src/automation_recording_windows.cpp)
endif()
defold_add_library(automation_capture STATIC ${AUTOMATION_CAPTURE_SOURCES})

foreach(target automation automation_capture automation_capture_null)
  # Internal headers include generated subsystem DDF headers. Order their
  # generation for clean builds as well as builds using an installed SDK.
  add_dependencies(${target} gameobject gamesys_gui gui render script extension profiler_remotery)
  target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
  target_include_directories(${target} PRIVATE
    "${DEFOLD_HOME}/engine"
    "${DEFOLD_HOME}/engine/dlib/src"
    "${DEFOLD_HOME}/engine/gamesys/src"
    "${DEFOLD_HOME}/engine/gui/src"
    "${DEFOLD_HOME}/engine/graphics/src"
    "${DEFOLD_HOME}/engine/platform/src"
    "${DEFOLD_HOME}/engine/render/src"
    "${DEFOLD_HOME}/engine/script/src"
    "${DEFOLD_HOME}/engine/hid/src"
    "${DEFOLD_HOME}/engine/extension/src")
  target_compile_definitions(${target} PRIVATE DLIB_LOG_DOMAIN="AUTOMATION")
  defold_target_link_libraries(${target} "${TARGET_PLATFORM}" SCOPE PUBLIC dlib)
  defold_target_link_lua(${target} "${TARGET_PLATFORM}" SCOPE PUBLIC)
endforeach()
if(TARGET_PLATFORM MATCHES "macos$")
  target_link_libraries(automation_capture PUBLIC
    "-framework AVFoundation" "-framework AppKit" "-framework CoreMedia"
    "-framework CoreVideo" "-framework Foundation" "-framework Metal"
    "-weak_framework ScreenCaptureKit")
elseif(TARGET_PLATFORM MATCHES "win32$")
  set_source_files_properties(src/automation_recording_windows.cpp PROPERTIES COMPILE_OPTIONS "-std=c++17")
  target_link_libraries(automation_capture PUBLIC dxgi dwmapi mfuuid ole32)
endif()
defold_install_targets(TARGETS automation automation_capture automation_capture_null
  ARCHIVE DESTINATION lib/${TARGET_PLATFORM})

if(AUTOMATION_BUILD_TESTS AND TARGET_PLATFORM STREQUAL HOST_PLATFORM AND
   TARGET_PLATFORM MATCHES "(macos|linux|win32)$" AND CMAKE_SOURCE_DIR STREQUAL DEFOLD_HOME)
  set(AUTOMATION_TEST_FIXTURE "${CMAKE_CURRENT_BINARY_DIR}/fixture")
  set(AUTOMATION_TEST_BOB "${DEFOLD_SDK_ROOT}/share/java/bob.jar" CACHE FILEPATH "Bob jar for automation fixtures")
  file(GLOB_RECURSE AUTOMATION_FIXTURE_SOURCES CONFIGURE_DEPENDS "src/test/fixture/*")
  add_custom_command(OUTPUT "${AUTOMATION_TEST_FIXTURE}/.complete"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/src/test/build_test_content.py"
      --java "${Java_JAVA_EXECUTABLE}" --bob "${AUTOMATION_TEST_BOB}"
      --platform "${TARGET_PLATFORM}" --output "${AUTOMATION_TEST_FIXTURE}"
    DEPENDS ${AUTOMATION_FIXTURE_SOURCES} src/test/build_test_content.py "${AUTOMATION_TEST_BOB}"
    VERBATIM)
  add_custom_target(automation_tests
    DEPENDS "${AUTOMATION_TEST_FIXTURE}/.complete" dmengine_headless dmengine_release
      dmengine_automation_unrelated dmengine_automation_legacy)
  # Real HTTP against an extension-free project verifies ordering and lifecycle contracts.
  add_test(NAME automation_runtime COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_runtime.py"
    --engine "$<TARGET_FILE:dmengine_headless>" --fixture "${AUTOMATION_TEST_FIXTURE}")
  # Inspection of the linked image guards against shipping debug code and capture dependencies.
  add_test(NAME automation_release_exclusion COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_release.py" "$<TARGET_FILE:dmengine_release>")
  # An unrelated native extension coexists; a legacy bridge is rejected before app initialization.
  add_test(NAME automation_extension_coexistence COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_runtime.py" --extension
    --engine "$<TARGET_FILE:dmengine_automation_unrelated>" --fixture "${AUTOMATION_TEST_FIXTURE}")
  add_test(NAME automation_legacy_rejection COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_legacy.py"
    "$<TARGET_FILE:dmengine_automation_legacy>" "${AUTOMATION_TEST_FIXTURE}")
  set_tests_properties(automation_runtime automation_release_exclusion automation_extension_coexistence
    automation_legacy_rejection PROPERTIES TIMEOUT 60)
endif()
