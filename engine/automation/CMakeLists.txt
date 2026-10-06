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
  set(AUTOMATION_TEST_BOB "${DEFOLD_BOB_LIGHT_JAR}" CACHE FILEPATH "Bob jar for automation fixtures")
  set(AUTOMATION_TEST_BUILTINS "${DEFOLD_HOME}/engine/engine/content")
  file(GLOB_RECURSE AUTOMATION_FIXTURE_SOURCES CONFIGURE_DEPENDS "src/test/fixture/*")
  file(GLOB_RECURSE AUTOMATION_BUILTIN_SOURCES CONFIGURE_DEPENDS "${AUTOMATION_TEST_BUILTINS}/builtins/*")
  add_custom_command(OUTPUT "${AUTOMATION_TEST_FIXTURE}/.complete"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/src/test/build_test_content.py"
      --java "${Java_JAVA_EXECUTABLE}" --bob "${AUTOMATION_TEST_BOB}"
      --builtins "${AUTOMATION_TEST_BUILTINS}"
      --platform "${TARGET_PLATFORM}" --output "${AUTOMATION_TEST_FIXTURE}"
    DEPENDS ${AUTOMATION_FIXTURE_SOURCES} ${AUTOMATION_BUILTIN_SOURCES}
      src/test/build_test_content.py "${AUTOMATION_TEST_BOB}"
    VERBATIM)
  add_custom_target(automation_test_content DEPENDS "${AUTOMATION_TEST_FIXTURE}/.complete")
  add_custom_target(automation_tests DEPENDS automation_test_content)
  if(BUILD_TESTS)
    add_dependencies(build_tests automation_tests)
  endif()

  # Keep CTest, focused targets, and both aggregate runners on the same commands.
  # The tests share staged fixture content and logs, so serialize their resource group.
  function(automation_add_test name engine)
    set(command ${ARGN})
    add_dependencies(automation_tests ${engine})
    add_test(NAME ${name} COMMAND ${command})
    set_tests_properties(${name} PROPERTIES TIMEOUT 60 RESOURCE_LOCK automation)
    defold_test_run_settings(run_locked run_options automation)
    add_custom_target(run_${name}
      COMMAND ${run_locked} ${command}
      DEPENDS ${engine} automation_test_content
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
      COMMENT "Running ${name}"
      VERBATIM ${run_options})
    if(BUILD_TESTS)
      defold_register_sequential_test_command(${name}
        COMMAND ${command} DEPENDS ${engine} automation_test_content)
      if(NOT CMAKE_GENERATOR STREQUAL "Xcode")
        if(NOT TARGET run_tests)
          add_custom_target(run_tests)
        endif()
        defold_add_to_run_tests(run_${name}
          RUN_GROUP automation COMMAND ${command}
          DEPENDS ${engine} automation_test_content
          WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
      endif()
    endif()
  endfunction()

  # Real HTTP against an extension-free project verifies ordering and lifecycle contracts.
  automation_add_test(automation_runtime dmengine_headless "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_runtime.py"
    --engine "$<TARGET_FILE:dmengine_headless>" --fixture "${AUTOMATION_TEST_FIXTURE}")
  # Inspection of the linked image guards against shipping debug code and capture dependencies.
  automation_add_test(automation_release_exclusion dmengine_release "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_release.py" "$<TARGET_FILE:dmengine_release>")
  # An unrelated native extension must coexist with the built-in automation implementation.
  automation_add_test(automation_extension_coexistence dmengine_automation_unrelated "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_runtime.py" --extension
    --engine "$<TARGET_FILE:dmengine_automation_unrelated>" --fixture "${AUTOMATION_TEST_FIXTURE}")
  # A legacy bridge must be rejected before application initialization.
  automation_add_test(automation_legacy_rejection dmengine_automation_legacy "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_legacy.py"
    "$<TARGET_FILE:dmengine_automation_legacy>" "${AUTOMATION_TEST_FIXTURE}")

  set(native_input_default OFF)
  if(TARGET_PLATFORM MATCHES "macos$")
    set(native_input_default ON)
  endif()
  option(AUTOMATION_TEST_NATIVE_INPUT "Run automation input tests with a native window (requires a display)" ${native_input_default})
  if(AUTOMATION_TEST_NATIVE_INPUT)
    # Native HID polling must preserve holds and allow touch before physical device interaction.
    automation_add_test(automation_native_input dmengine "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_runtime.py"
      --engine "$<TARGET_FILE:dmengine>" --fixture "${AUTOMATION_TEST_FIXTURE}"
      RuntimeTest.test_pointer_hold_and_move RuntimeTest.test_drag_holds
      RuntimeTest.test_touch_taps RuntimeTest.test_touch_hold_and_move RuntimeTest.test_touch_cancellation
      RuntimeTest.test_debugger_pause_and_resume RuntimeTest.test_input_sources)
  endif()
endif()
