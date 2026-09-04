cmake_minimum_required(VERSION 3.6)

if(NOT TARGET_PLATFORM)
  message(FATAL_ERROR "TARGET_PLATFORM must be set")
endif()

set(_BULLET_USE_DEFOLD_PLATFORM OFF)
if(TARGET_PLATFORM MATCHES "^(arm64-nx64|x86_64-ps4|x86_64-ps5|x86_64-xbone|x86_64-xbox)$")
  cmake_minimum_required(VERSION 4.0)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
  include(defold)
  set(_BULLET_USE_DEFOLD_PLATFORM ON)
endif()

# Configure cross-compilation before project() enables the compiler.
if(_BULLET_USE_DEFOLD_PLATFORM)
  message(DEBUG "Using Defold platform toolchain for ${TARGET_PLATFORM}")
elseif(TARGET_PLATFORM STREQUAL "arm64-macos")
  set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Target architecture" FORCE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version" FORCE)
  set(CMAKE_OSX_SYSROOT "macosx" CACHE STRING "Apple SDK" FORCE)
elseif(TARGET_PLATFORM STREQUAL "x86_64-macos")
  set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Target architecture" FORCE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version" FORCE)
  set(CMAKE_OSX_SYSROOT "macosx" CACHE STRING "Apple SDK" FORCE)
elseif(TARGET_PLATFORM STREQUAL "arm64-ios")
  set(CMAKE_SYSTEM_NAME "Darwin")
  set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Target architecture" FORCE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum iOS version" FORCE)
  set(CMAKE_OSX_SYSROOT "iphoneos" CACHE STRING "Apple SDK" FORCE)
elseif(TARGET_PLATFORM STREQUAL "arm64_sim-ios")
  set(CMAKE_SYSTEM_NAME "Darwin")
  set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Target architecture" FORCE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum iOS version" FORCE)
  set(CMAKE_OSX_SYSROOT "iphonesimulator" CACHE STRING "Apple SDK" FORCE)
elseif(TARGET_PLATFORM MATCHES "^(arm64|armv7|x86_64)-android$")
  if(DEFINED ENV{ANDROID_NDK_ROOT})
    set(_ANDROID_NDK_ROOT "$ENV{ANDROID_NDK_ROOT}")
  elseif(DEFINED ENV{ANDROID_NDK_HOME})
    set(_ANDROID_NDK_ROOT "$ENV{ANDROID_NDK_HOME}")
  else()
    message(FATAL_ERROR "ANDROID_NDK_ROOT or ANDROID_NDK_HOME must be set for ${TARGET_PLATFORM}")
  endif()

  set(_ANDROID_TOOLCHAIN_FILE "${_ANDROID_NDK_ROOT}/build/cmake/android.toolchain.cmake")
  if(NOT EXISTS "${_ANDROID_TOOLCHAIN_FILE}")
    message(FATAL_ERROR "Android toolchain file not found: ${_ANDROID_TOOLCHAIN_FILE}")
  endif()
  set(CMAKE_TOOLCHAIN_FILE "${_ANDROID_TOOLCHAIN_FILE}" CACHE FILEPATH "Android toolchain" FORCE)

  if(TARGET_PLATFORM STREQUAL "arm64-android")
    set(ANDROID_ABI "arm64-v8a" CACHE STRING "Android ABI" FORCE)
    set(ANDROID_PLATFORM "android-21" CACHE STRING "Android API level" FORCE)
  elseif(TARGET_PLATFORM STREQUAL "x86_64-android")
    set(ANDROID_ABI "x86_64" CACHE STRING "Android ABI" FORCE)
    set(ANDROID_PLATFORM "android-21" CACHE STRING "Android API level" FORCE)
  else()
    set(ANDROID_ABI "armeabi-v7a" CACHE STRING "Android ABI" FORCE)
    set(ANDROID_PLATFORM "android-19" CACHE STRING "Android API level" FORCE)
    set(ANDROID_ARM_MODE "arm" CACHE STRING "Build ARM instructions instead of Thumb" FORCE)
  endif()
elseif(TARGET_PLATFORM MATCHES "^(wasm|wasm_pthread)-web$")
  if(NOT DEFINED ENV{EMSDK})
    message(FATAL_ERROR "EMSDK must be set for ${TARGET_PLATFORM}")
  endif()
  set(_EMSCRIPTEN_TOOLCHAIN_FILE "$ENV{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
  if(NOT EXISTS "${_EMSCRIPTEN_TOOLCHAIN_FILE}")
    message(FATAL_ERROR "Emscripten toolchain file not found: ${_EMSCRIPTEN_TOOLCHAIN_FILE}")
  endif()
  set(CMAKE_TOOLCHAIN_FILE "${_EMSCRIPTEN_TOOLCHAIN_FILE}" CACHE FILEPATH "Emscripten toolchain" FORCE)
elseif(TARGET_PLATFORM MATCHES "^(arm64|x86_64)-linux$")
  find_program(_CLANGXX NAMES clang++)
  if(NOT _CLANGXX)
    message(FATAL_ERROR "clang++ must be available for ${TARGET_PLATFORM}")
  endif()
  set(CMAKE_CXX_COMPILER "${_CLANGXX}" CACHE FILEPATH "C++ compiler" FORCE)

  find_program(_LLVM_AR NAMES llvm-ar)
  if(_LLVM_AR)
    set(CMAKE_AR "${_LLVM_AR}" CACHE FILEPATH "Archiver" FORCE)
  endif()
  find_program(_LLVM_RANLIB NAMES llvm-ranlib)
  if(_LLVM_RANLIB)
    set(CMAKE_RANLIB "${_LLVM_RANLIB}" CACHE FILEPATH "Archive indexer" FORCE)
  endif()
endif()

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(bullet3d LANGUAGES CXX)

# CMake 3.6 predates CMAKE_MSVC_RUNTIME_LIBRARY. Rewrite the default dynamic
# runtime flags to keep the static /MT runtime used by Defold.
if(MSVC)
  foreach(_flag_var
          CMAKE_CXX_FLAGS
          CMAKE_CXX_FLAGS_DEBUG
          CMAKE_CXX_FLAGS_RELEASE
          CMAKE_CXX_FLAGS_RELWITHDEBINFO
          CMAKE_CXX_FLAGS_MINSIZEREL)
    string(REPLACE "/MDd" "/MT" _flags "${${_flag_var}}")
    string(REPLACE "/MD" "/MT" _flags "${_flags}")
    string(REPLACE "/Zi" "/Z7" _flags "${_flags}")
    set(${_flag_var} "${_flags}" CACHE STRING "MSVC C++ flags" FORCE)
  endforeach()
endif()

if(NOT DEFOLD_EXTERNAL_INSTALL_PREFIX)
  message(FATAL_ERROR "DEFOLD_EXTERNAL_INSTALL_PREFIX must be set")
endif()

set(CMAKE_INSTALL_PREFIX "${DEFOLD_EXTERNAL_INSTALL_PREFIX}" CACHE PATH "Bullet install prefix" FORCE)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(BULLET_VERSION 3.25)
set(BULLET_PHYSICS_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/package/bullet-${BULLET_VERSION}")
set(BUILD_SHARED_LIBS OFF)
set(INSTALL_LIBS OFF)

add_subdirectory("${BULLET_PHYSICS_SOURCE_DIR}/src/LinearMath" LinearMath)
add_subdirectory("${BULLET_PHYSICS_SOURCE_DIR}/src/BulletCollision" BulletCollision)
add_subdirectory("${BULLET_PHYSICS_SOURCE_DIR}/src/BulletDynamics" BulletDynamics)

set(_BULLET_TARGETS LinearMath BulletCollision BulletDynamics)
foreach(_target IN LISTS _BULLET_TARGETS)
  target_compile_definitions(${_target} PRIVATE NDEBUG _USE_MATH_DEFINES)
  if(_BULLET_USE_DEFOLD_PLATFORM)
    target_link_libraries(${_target} PRIVATE defold_sdk)
  endif()
  if(MSVC)
    target_compile_definitions(${_target} PRIVATE
      WINVER=0x0600
      _WIN32_WINNT=0x0600
      NOMINMAX
      _CRT_SECURE_NO_WARNINGS
      _HAS_EXCEPTIONS=0
      UNICODE
      _UNICODE)
    target_compile_options(${_target} PRIVATE /GR- /EHs- /EHa- /Oy-)
  else()
    target_compile_options(${_target} PRIVATE -fno-exceptions -fno-rtti -fvisibility=hidden)
  endif()
  if(TARGET_PLATFORM MATCHES "^(wasm|wasm_pthread)-web$")
    target_compile_options(${_target} PRIVATE -sDISABLE_EXCEPTION_CATCHING=1)
  endif()
  if(TARGET_PLATFORM STREQUAL "wasm_pthread-web")
    target_compile_options(${_target} PRIVATE -pthread)
  endif()
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${_target} PRIVATE -Wno-c++11-narrowing)
  endif()
  set_target_properties(${_target} PROPERTIES PREFIX "lib")
endforeach()

install(TARGETS ${_BULLET_TARGETS}
        ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")

install(DIRECTORY "${BULLET_PHYSICS_SOURCE_DIR}/src/BulletCollision"
                  "${BULLET_PHYSICS_SOURCE_DIR}/src/BulletDynamics"
                  "${BULLET_PHYSICS_SOURCE_DIR}/src/LinearMath"
        DESTINATION include
        FILES_MATCHING PATTERN "*.h")
install(FILES
        "${BULLET_PHYSICS_SOURCE_DIR}/src/btBulletCollisionCommon.h"
        "${BULLET_PHYSICS_SOURCE_DIR}/src/btBulletDynamicsCommon.h"
        DESTINATION include)
install(FILES "${BULLET_PHYSICS_SOURCE_DIR}/LICENSE.txt"
        DESTINATION share/licenses/bullet3d)
