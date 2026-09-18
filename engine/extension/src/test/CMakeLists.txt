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
  target_include_directories(test_extension PRIVATE "${DEFOLD_HOME}/external/glfw/lib/ios/app")
  defold_target_link_glfw(test_extension "${TARGET_PLATFORM}")
  defold_target_link_opengl(test_extension "${TARGET_PLATFORM}")
  target_link_libraries(test_extension PRIVATE "-framework QuartzCore" "-framework CoreMotion")

  defold_add_executable(test_extension_ios_app "${EXTENSION_TEST_ROOT}/test_extension_ios_app.mm")
  target_compile_options(test_extension_ios_app PRIVATE "$<$<COMPILE_LANGUAGE:OBJCXX>:-fno-rtti>")
  target_include_directories(test_extension_ios_app PRIVATE "${DEFOLD_HOME}/external/glfw/lib/ios/app")
  defold_target_link_libraries(test_extension_ios_app "${TARGET_PLATFORM}" testmain dlib profile_null)
  defold_target_link_glfw(test_extension_ios_app "${TARGET_PLATFORM}")
  defold_target_link_opengl(test_extension_ios_app "${TARGET_PLATFORM}")
  target_link_libraries(test_extension_ios_app PRIVATE "-framework QuartzCore" "-framework CoreMotion")
  defold_register_test_target(test_extension_ios_app ON)
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
