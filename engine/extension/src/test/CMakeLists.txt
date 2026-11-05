cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

set(EXTENSION_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(EXTENSION_ROOT "${EXTENSION_TEST_ROOT}/../..")
set(EXTENSION_SRC_DIR "${EXTENSION_ROOT}/src")

defold_add_library(test_extension_lib STATIC "${EXTENSION_TEST_ROOT}/test_extension_lib.cpp")
target_include_directories(test_extension_lib PUBLIC
  "${EXTENSION_TEST_ROOT}"
  "${EXTENSION_SRC_DIR}")
defold_target_link_libraries(test_extension_lib "${TARGET_PLATFORM}" ddf dlib)

set(_extension_exports TestExt)
defold_create_exported_symbols_file("${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols.cpp" "${_extension_exports}")

defold_add_executable(test_extension
  "${EXTENSION_TEST_ROOT}/test_extension.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols.cpp")
target_include_directories(test_extension PRIVATE
  "${EXTENSION_TEST_ROOT}"
  "${EXTENSION_SRC_DIR}")
defold_target_link_libraries(test_extension "${TARGET_PLATFORM}"
  testmain
  dlib
  profile_null
  lua
  extension
  test_extension_lib)
defold_target_link_socket(test_extension "${TARGET_PLATFORM}" PRIVATE)
if(TARGET_PLATFORM MATCHES "^(arm64-ios|arm64_sim-ios)$")
  target_sources(test_extension PRIVATE "${EXTENSION_TEST_ROOT}/test_extension_ios.mm")
  target_compile_options(test_extension PRIVATE "$<$<COMPILE_LANGUAGE:OBJCXX>:-fno-rtti>")
  target_include_directories(test_extension PRIVATE "${DEFOLD_HOME}/engine/platform/src/native/ios/app")
  defold_target_link_platform(test_extension "${TARGET_PLATFORM}")
  defold_target_link_opengl(test_extension "${TARGET_PLATFORM}")
  target_link_libraries(test_extension PRIVATE "-framework QuartzCore" "-framework CoreMotion")

  set(_ios_manifest_dir "${DEFOLD_HOME}/engine/engine/content/builtins/manifests/ios")
  set(_ios_manifest_renderer "${EXTENSION_TEST_ROOT}/render_ios_test_manifest.py")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${_ios_manifest_dir}/Info.plist" "${_ios_manifest_renderer}")
  _defold_find_python(_python)
  set(_ios_test_platform iPhoneOS)
  if(TARGET_PLATFORM STREQUAL "arm64_sim-ios")
    set(_ios_test_platform iPhoneSimulator)
  endif()

  foreach(_orientation default portrait landscape)
    set(_app_target test_extension_ios_app)
    if(NOT _orientation STREQUAL "default")
      string(APPEND _app_target "_${_orientation}")
    endif()
    defold_add_executable(${_app_target} "${EXTENSION_TEST_ROOT}/test_extension_ios_app.mm")
    target_compile_options(${_app_target} PRIVATE "$<$<COMPILE_LANGUAGE:OBJCXX>:-fno-rtti>")
    target_include_directories(${_app_target} PRIVATE "${DEFOLD_HOME}/engine/platform/src/native/ios/app")
    defold_target_link_libraries(${_app_target} "${TARGET_PLATFORM}" testmain dlib profile_null platform)
    defold_target_link_opengl(${_app_target} "${TARGET_PLATFORM}")
    target_link_libraries(${_app_target} PRIVATE "-framework QuartzCore" "-framework CoreMotion")
    if(_orientation STREQUAL "portrait")
      set(_orientations Portrait PortraitUpsideDown)
      target_compile_definitions(${_app_target} PRIVATE DM_TEST_IOS_ORIENTATION_PORTRAIT)
    elseif(_orientation STREQUAL "landscape")
      set(_orientations LandscapeLeft LandscapeRight)
      target_compile_definitions(${_app_target} PRIVATE DM_TEST_IOS_ORIENTATION_LANDSCAPE)
    else()
      set(_orientations Portrait PortraitUpsideDown LandscapeLeft LandscapeRight)
    endif()
    set(_orientation_entries "")
    foreach(_value IN LISTS _orientations)
      string(APPEND _orientation_entries "        <string>UIInterfaceOrientation${_value}</string>\n")
    endforeach()
    _defold_ios_test_bundle_id(_app_bundle_id "${_app_target}")
    set(_app_plist "${CMAKE_CURRENT_BINARY_DIR}/${_app_target}-Info.plist")
    if(_orientation STREQUAL "default")
      # Keep a minimal manifest to exercise programmatic scene configuration.
      configure_file("${EXTENSION_TEST_ROOT}/test_extension_ios_app.plist.in" "${_app_plist}" @ONLY)
      defold_register_test_target(${_app_target} ON IOS_INFO_PLIST "${_app_plist}")
    else()
      # Use the shipping manifest and launch screen so UIKit applies the same
      # orientation and multitasking rules as it does to a bundled game.
      execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "PYTHONPATH=${DEFOLD_SDK_ROOT}/ext/lib/python"
          "${_python}" "${_ios_manifest_renderer}"
          --manifest "${_ios_manifest_dir}/Info.plist" --output "${_app_plist}"
          --executable "${_app_target}" --bundle-id "${_app_bundle_id}"
          --platform "${_ios_test_platform}"
          --orientations ${_orientations}
        COMMAND_ERROR_IS_FATAL ANY)
      target_compile_definitions(${_app_target} PRIVATE DM_TEST_IOS_PRODUCTION_MANIFEST)
      defold_register_test_target(${_app_target} ON IOS_INFO_PLIST "${_app_plist}"
        IOS_BUNDLE_RESOURCES "${_ios_manifest_dir}/LaunchScreen.storyboardc")
    endif()
  endforeach()
endif()
if(TARGET extension_cs)
  target_link_libraries(test_extension PRIVATE extension_cs)
  defold_target_link_csharp_runtime(test_extension "${TARGET_PLATFORM}")
endif()
if(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
  find_package(Threads REQUIRED)
  target_link_libraries(test_extension PRIVATE Threads::Threads dl)
endif()

defold_register_test_target(test_extension ON "${EXTENSION_ROOT}")
