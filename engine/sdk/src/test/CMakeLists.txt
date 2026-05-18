cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

set(SDK_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(SDK_ROOT "${SDK_TEST_ROOT}/../.." ABSOLUTE)
set(SDK_SRC_DIR "${SDK_ROOT}/src")
set(SDK_INSTALLED_INCLUDE_DIR "${DEFOLD_SDK_ROOT}/sdk/include")
set(SDK_EXTRA_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/extra")
set(SDK_GENERATED_TEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${SDK_EXTRA_INCLUDE_DIR}")
file(MAKE_DIRECTORY "${SDK_GENERATED_TEST_DIR}")

function(sdk_write_if_different path content)
  if(EXISTS "${path}")
    file(READ "${path}" _sdk_existing_content)
    if(_sdk_existing_content STREQUAL content)
      return()
    endif()
  endif()
  file(WRITE "${path}" "${content}")
endfunction()

function(sdk_apply_header_test_settings target)
  set_property(TARGET ${target} PROPERTY LINK_LIBRARIES "")
  target_include_directories(${target} PRIVATE
    "${SDK_INSTALLED_INCLUDE_DIR}"
    "${SDK_SRC_DIR}"
    "${SDK_EXTRA_INCLUDE_DIR}"
    "${DEFOLD_HOME}/engine/ddf/src"
    "${DEFOLD_DDF_BINARY_DIR}"
    "${DEFOLD_GAMEOBJECT_BINARY_DIR}"
    "${DEFOLD_GAMEOBJECT_BINARY_DIR}/src/proto"
    "${DEFOLD_GAMESYS_BINARY_DIR}"
    "${DEFOLD_GRAPHICS_BINARY_DIR}"
    "${DEFOLD_INPUT_BINARY_DIR}"
    "${DEFOLD_PARTICLE_BINARY_DIR}"
    "${DEFOLD_RENDER_BINARY_DIR}"
    "${DEFOLD_RIG_BINARY_DIR}"
    "${DEFOLD_SCRIPT_BINARY_DIR}"
    "${DEFOLD_EXT_INCLUDE_DIR}"
    "${DEFOLD_EXT_PLATFORM_INCLUDE_DIR}")
  target_compile_definitions(${target} PRIVATE
    __STDC_LIMIT_MACROS
    DDF_EXPOSE_DESCRIPTORS
    GOOGLE_PROTOBUF_NO_RTTI
    DM_USE_CMAKE
    DLIB_LOG_DOMAIN=\"SDK\")
  if(TARGET_PLATFORM MATCHES "^arm64|^x86_64")
    target_compile_definitions(${target} PRIVATE DM_PLATFORM_64BIT)
  else()
    target_compile_definitions(${target} PRIVATE DM_PLATFORM_32BIT)
  endif()
  if(TARGET_PLATFORM MATCHES "macos")
    target_compile_definitions(${target} PRIVATE DM_PLATFORM_MACOS DM_HOSTFS=\"\")
    if(TARGET_PLATFORM MATCHES "^arm64-")
      set(_sdk_test_arch arm64)
    else()
      set(_sdk_test_arch x86_64)
    endif()
    target_compile_options(${target} PRIVATE
      -mmacosx-version-min=${SDK_VERSION_MACOSX_MIN}
      -target ${_sdk_test_arch}-apple-darwin19)
  elseif(TARGET_PLATFORM MATCHES "ios")
    target_compile_definitions(${target} PRIVATE DM_PLATFORM_IOS DM_HOSTFS=\"\")
  elseif(TARGET_PLATFORM MATCHES "android")
    target_compile_definitions(${target} PRIVATE ANDROID DM_HOSTFS=\"\")
  elseif(TARGET_PLATFORM MATCHES "web")
    target_compile_definitions(${target} PRIVATE DM_PLATFORM_HTML5 DM_NO_SYSTEM_FUNCTION DM_HOSTFS=\"/node_vfs/\")
  elseif(TARGET_PLATFORM MATCHES "linux")
    target_compile_definitions(${target} PRIVATE DM_PLATFORM_LINUX DM_HOSTFS=\"\")
  elseif(TARGET_PLATFORM MATCHES "win32")
    target_compile_definitions(${target} PRIVATE DM_PLATFORM_WINDOWS WIN32 _WIN32 DM_HOSTFS=\"\")
  endif()
  foreach(_sdk_header_dependency IN ITEMS
      gameobject_proto_headers
      gamesys_proto_headers
      gamesys_model_proto_headers
      graphics_proto_header
      input_proto_headers
      particle_proto_headers
      render_proto_headers
      rig_proto_headers
      script_proto_headers)
    if(TARGET ${_sdk_header_dependency})
      add_dependencies(${target} ${_sdk_header_dependency})
    endif()
  endforeach()
  set_target_properties(${target} PROPERTIES EXCLUDE_FROM_ALL TRUE)
endfunction()

file(GLOB_RECURSE SDK_EXTRA_HEADERS CONFIGURE_DEPENDS
  "${DEFOLD_INCLUDE_DIR}/*ddf*.h"
  "${DEFOLD_INCLUDE_DIR}/res_*.h")
foreach(_extra_header IN LISTS SDK_EXTRA_HEADERS)
  file(RELATIVE_PATH _extra_relative "${DEFOLD_INCLUDE_DIR}" "${_extra_header}")
  get_filename_component(_extra_relative_dir "${SDK_EXTRA_INCLUDE_DIR}/${_extra_relative}" DIRECTORY)
  file(MAKE_DIRECTORY "${_extra_relative_dir}")
  configure_file("${_extra_header}" "${SDK_EXTRA_INCLUDE_DIR}/${_extra_relative}" COPYONLY)
endforeach()

add_library(test_sdk_lib STATIC EXCLUDE_FROM_ALL "${SDK_TEST_ROOT}/test_sdk_lib.cpp")
sdk_apply_header_test_settings(test_sdk_lib)

set(_sdk_exports TestSdk)
defold_create_exported_symbols_file("${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols.cpp" "${_sdk_exports}")

defold_add_executable(test_sdk
  EXCLUDE_FROM_ALL
  "${SDK_TEST_ROOT}/test_sdk.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols.cpp")
target_include_directories(test_sdk PRIVATE
  "${SDK_TEST_ROOT}"
  "${SDK_SRC_DIR}")
defold_target_link_libraries(test_sdk "${TARGET_PLATFORM}"
  testmain
  extension
  dlib
  lua
  profile_null
  test_sdk_lib)
defold_target_link_socket(test_sdk "${TARGET_PLATFORM}" PRIVATE)
if(TARGET_PLATFORM MATCHES "macos|ios")
  target_link_options(test_sdk PRIVATE "-Wl,-u,_dmExportedSymbols")
elseif(TARGET_PLATFORM MATCHES "linux|android|web")
  target_link_options(test_sdk PRIVATE "-Wl,-u,dmExportedSymbols")
elseif(TARGET_PLATFORM STREQUAL "x86-win32")
  target_link_options(test_sdk PRIVATE "/INCLUDE:_dmExportedSymbols")
elseif(TARGET_PLATFORM MATCHES "win32")
  target_link_options(test_sdk PRIVATE "/INCLUDE:dmExportedSymbols")
endif()
if(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
  find_package(Threads REQUIRED)
  target_link_libraries(test_sdk PRIVATE Threads::Threads dl)
endif()
defold_register_test_target(test_sdk ON "${SDK_ROOT}")

set(_sdk_exclude_dirs vectormath external lua)
set(_sdk_exclude_files sdk.h)
set(_sdk_exclude_endings
  _gen.hpp
  _posix.h
  _win32.h
  _mach.h
  _android.h
  _pthread.h
  _vendor.h
  _nx64.h
  _ps4.h
  _ps5.h)
set(_sdk_c_includes
  extension.h
  configfile.h
  resource.h)

file(READ "${SDK_TEST_ROOT}/test_include.cpp" _sdk_include_cpp_template)
file(READ "${SDK_TEST_ROOT}/test_include.c" _sdk_include_c_template)
file(GLOB_RECURSE _sdk_header_candidates CONFIGURE_DEPENDS
  RELATIVE "${SDK_INSTALLED_INCLUDE_DIR}/dmsdk"
  "${SDK_INSTALLED_INCLUDE_DIR}/dmsdk/*.h"
  "${SDK_INSTALLED_INCLUDE_DIR}/dmsdk/*.hpp")

set(_sdk_include_cpp_sources)
set(_sdk_include_c_sources)
foreach(_sdk_header IN LISTS _sdk_header_candidates)
  get_filename_component(_sdk_header_name "${_sdk_header}" NAME)
  if(_sdk_header_name IN_LIST _sdk_exclude_files)
    continue()
  endif()

  set(_sdk_skip OFF)
  foreach(_sdk_exclude_dir IN LISTS _sdk_exclude_dirs)
    if(_sdk_header MATCHES "(^|/)${_sdk_exclude_dir}(/|$)")
      set(_sdk_skip ON)
    endif()
  endforeach()
  foreach(_sdk_exclude_ending IN LISTS _sdk_exclude_endings)
    if(_sdk_header MATCHES "${_sdk_exclude_ending}$")
      set(_sdk_skip ON)
    endif()
  endforeach()
  if(_sdk_skip)
    continue()
  endif()

  string(MAKE_C_IDENTIFIER "${_sdk_header}" _sdk_header_id)
  set(_sdk_include_path "dmsdk/${_sdk_header}")
  set(_sdk_cpp_source "${SDK_GENERATED_TEST_DIR}/test_include_${_sdk_header_id}.cpp")
  string(REPLACE "{custom_include}" "${_sdk_include_path}" _sdk_cpp_content "${_sdk_include_cpp_template}")
  sdk_write_if_different("${_sdk_cpp_source}" "${_sdk_cpp_content}")
  list(APPEND _sdk_include_cpp_sources "${_sdk_cpp_source}")

  if(_sdk_header_name IN_LIST _sdk_c_includes)
    set(_sdk_c_source "${SDK_GENERATED_TEST_DIR}/test_include_${_sdk_header_id}.c")
    string(REPLACE "{custom_include}" "${_sdk_include_path}" _sdk_c_content "${_sdk_include_c_template}")
    sdk_write_if_different("${_sdk_c_source}" "${_sdk_c_content}")
    list(APPEND _sdk_include_c_sources "${_sdk_c_source}")
  endif()
endforeach()

if(_sdk_include_cpp_sources)
  add_library(test_sdk_includes_cpp STATIC EXCLUDE_FROM_ALL ${_sdk_include_cpp_sources})
  sdk_apply_header_test_settings(test_sdk_includes_cpp)
  if(NOT TARGET build_tests)
    add_custom_target(build_tests)
  endif()
  add_dependencies(build_tests test_sdk_includes_cpp)
endif()

if(_sdk_include_c_sources)
  add_library(test_sdk_includes_c STATIC EXCLUDE_FROM_ALL ${_sdk_include_c_sources})
  sdk_apply_header_test_settings(test_sdk_includes_c)
  if(NOT TARGET build_tests)
    add_custom_target(build_tests)
  endif()
  add_dependencies(build_tests test_sdk_includes_c)
endif()
