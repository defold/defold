cmake_minimum_required(VERSION 4.0)

# Also support a focused build without configuring unrelated dependencies.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(dxc_external LANGUAGES C CXX)
  if(DEFOLD_EXTERNAL_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX "${DEFOLD_EXTERNAL_INSTALL_PREFIX}" CACHE PATH "Install prefix" FORCE)
  elseif(DEFOLD_SDK_ROOT)
    set(CMAKE_INSTALL_PREFIX "${DEFOLD_SDK_ROOT}/ext" CACHE PATH "Install prefix" FORCE)
  endif()
endif()

if(NOT TARGET_PLATFORM MATCHES "^(arm64-macos|x86_64-macos|arm64-linux|x86_64-linux|x86_64-win32)$")
  message(FATAL_ERROR "DXC is a desktop build tool; unsupported platform: ${TARGET_PLATFORM}")
endif()
if(TARGET_PLATFORM MATCHES "linux$")
  # The normal CI uses native runners for both Linux architectures. Refuse to
  # silently produce a host binary in a different architecture's install path.
  if((TARGET_PLATFORM STREQUAL "arm64-linux" AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
      OR (TARGET_PLATFORM STREQUAL "x86_64-linux" AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
      OR NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "DXC Linux tools must be built on a native runner for ${TARGET_PLATFORM}")
  endif()
endif()

include(ExternalProject)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# LLVM owns its compiler flags, exceptions and RTTI. Do not add its sources to
# the engine's CMake project or inherit Defold's global compile/link flags.
set(DXC_VERSION "1.9.2609")
set(dxc_executable_version "3.7") # Upstream LLVM executable suffix, not the DXC release.
set(DXC_HEADERS_REVISION "980971e835876dc0cde415e8f9bc646e64667bf7")
set(dxc_source "${CMAKE_CURRENT_BINARY_DIR}/sources/dxc-${DXC_VERSION}")
set(dxc_headers "${CMAKE_CURRENT_BINARY_DIR}/sources/headers-${DXC_HEADERS_REVISION}")
set(dxc_build "${CMAKE_CURRENT_BINARY_DIR}/build")
set(DEFOLD_DXC_BUILD_JOBS 4 CACHE STRING "Maximum parallel DXC compilation jobs")

ExternalProject_Add(dxc_headers_source
  URL "https://github.com/microsoft/DirectX-Headers/archive/${DXC_HEADERS_REVISION}.tar.gz"
  URL_HASH SHA256=b5a4b6d8806ff7f29f19879f83d015dbe8740676d4ca0b48647a789cc7773c4e
  SOURCE_DIR "${dxc_headers}"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND "")

set(dxc_platform_args)
foreach(variable CMAKE_C_COMPILER CMAKE_CXX_COMPILER CMAKE_MAKE_PROGRAM
                 CMAKE_SYSROOT CMAKE_C_COMPILER_TARGET CMAKE_CXX_COMPILER_TARGET
                 CMAKE_OSX_SYSROOT CMAKE_OSX_ARCHITECTURES CMAKE_OSX_DEPLOYMENT_TARGET)
  if(DEFINED ${variable} AND NOT "${${variable}}" STREQUAL "")
    list(APPEND dxc_platform_args "-D${variable}=${${variable}}")
  endif()
endforeach()
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET AND SDK_VERSION_MACOSX_MIN)
  list(APPEND dxc_platform_args "-DCMAKE_OSX_DEPLOYMENT_TARGET=${SDK_VERSION_MACOSX_MIN}")
endif()

set(dxc_targets dxc dxcompiler)
if(WIN32)
  # Native Windows builds use the toolchain headers; the portable stubs are
  # only for Unix and conflict with Windows SDK RPC declarations.
  list(APPEND dxc_platform_args "-DDIRECTX_HEADER_INCLUDE_DIR:PATH=")
  # Match Defold's static CRT policy for distributable Windows tools.
  list(APPEND dxc_platform_args "-DLLVM_USE_CRT_RELEASE=MT")
  list(APPEND dxc_targets dxildll)
  set(dxc_outputs "${dxc_build}/bin/dxc.exe" "${dxc_build}/bin/dxcompiler.dll"
                  "${dxc_build}/bin/dxil.dll" "${dxc_build}/lib/dxcompiler.lib")
else()
  list(APPEND dxc_platform_args "-DDIRECTX_HEADER_INCLUDE_DIR=${dxc_headers}/include")
  set(dxc_outputs "${dxc_build}/bin/dxc"
                  "${dxc_build}/bin/dxc-${dxc_executable_version}"
                  "${dxc_build}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}dxcompiler${CMAKE_SHARED_LIBRARY_SUFFIX}")
  if(APPLE)
    list(APPEND dxc_platform_args "-DCMAKE_INSTALL_RPATH=@loader_path/../../lib/${TARGET_PLATFORM}")
  else()
    list(APPEND dxc_platform_args "-DCMAKE_INSTALL_RPATH=$ORIGIN/../../lib/${TARGET_PLATFORM}")
  endif()
  list(APPEND dxc_platform_args "-DCMAKE_BUILD_WITH_INSTALL_RPATH=ON")
endif()

ExternalProject_Add(dxc_build_external
  DEPENDS dxc_headers_source
  URL "https://github.com/microsoft/DirectXShaderCompiler/archive/refs/tags/v${DXC_VERSION}.tar.gz"
  URL_HASH SHA256=05b8e9622f9b147156998e6aab38d71997ec8391038e7302c703e9ca2448dfed
  SOURCE_DIR "${dxc_source}"
  BINARY_DIR "${dxc_build}"
  PATCH_COMMAND "${CMAKE_COMMAND}" "-DDXC_SOURCE_DIR=<SOURCE_DIR>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/patch.cmake"
  CMAKE_GENERATOR Ninja
  CMAKE_ARGS
    -C "${dxc_source}/cmake/caches/PredefinedParams.cmake"
    "-DCMAKE_BUILD_TYPE=Release"
    "-DPython3_EXECUTABLE=${Python3_EXECUTABLE}"
    "-DHLSL_INCLUDE_TESTS=OFF"
    "-DLLVM_INCLUDE_TESTS=OFF"
    "-DSPIRV_BUILD_TESTS=OFF"
    "-DENABLE_SPIRV_CODEGEN=OFF"
    "-DHLSL_BUILD_DXILCONV=OFF"
    "-DHLSL_SUPPORT_QUERY_GIT_COMMIT_INFO=OFF"
    # Source archives have no Git history. Use their checked-in version rather
    # than accidentally stamping the enclosing Defold checkout's revision.
    "-DHLSL_ENABLE_FIXED_VER=ON"
    "-DLLVM_APPEND_VC_REV=OFF"
    "-DLLVM_ENABLE_ZLIB=OFF"
    "-DLLVM_ENABLE_TERMINFO=OFF"
    "-DCLANG_EXECUTABLE_VERSION:STRING=${dxc_executable_version}"
    # Formatting generated intermediates is unnecessary and makes the build
    # depend on whichever clang-format (including wrapper scripts) is on PATH.
    "-DCLANG_FORMAT_EXE:FILEPATH="
    ${dxc_platform_args}
  BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --target ${dxc_targets}
                --parallel "${DEFOLD_DXC_BUILD_JOBS}"
  USES_TERMINAL_CONFIGURE TRUE
  USES_TERMINAL_BUILD TRUE
  BUILD_BYPRODUCTS ${dxc_outputs}
  INSTALL_COMMAND "")
ExternalProject_Add_StepDependencies(dxc_build_external patch "${CMAKE_CURRENT_SOURCE_DIR}/patch.cmake")

if(WIN32)
  install(PROGRAMS "${dxc_build}/bin/dxc.exe" DESTINATION "bin/${TARGET_PLATFORM}" COMPONENT dxc)
  install(FILES "${dxc_build}/bin/dxcompiler.dll" "${dxc_build}/bin/dxil.dll"
          DESTINATION "bin/${TARGET_PLATFORM}" COMPONENT dxc)
  install(FILES "${dxc_build}/lib/dxcompiler.lib" DESTINATION "lib/${TARGET_PLATFORM}" COMPONENT dxc)
else()
  # Copy the executable itself: bin/dxc is a symlink to the versioned file.
  install(PROGRAMS "${dxc_build}/bin/dxc-${dxc_executable_version}"
          DESTINATION "bin/${TARGET_PLATFORM}" RENAME dxc COMPONENT dxc)
  install(FILES "${dxc_build}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}dxcompiler${CMAKE_SHARED_LIBRARY_SUFFIX}"
          DESTINATION "lib/${TARGET_PLATFORM}" COMPONENT dxc)
endif()
install(FILES "${dxc_source}/include/dxc/dxcapi.h" "${dxc_source}/include/dxc/WinAdapter.h"
              "${dxc_source}/include/dxc/dxcerrors.h" DESTINATION include/dxc COMPONENT dxc)
# These must match DXC's portable COM declarations, not a newer headers release.
install(DIRECTORY "${dxc_headers}/include/" DESTINATION include/dxc/DirectX-Headers COMPONENT dxc)
install(FILES "${dxc_source}/LICENSE.TXT" DESTINATION share/licenses/dxc COMPONENT dxc)
install(FILES "${dxc_source}/lib/DxilCompression/LICENSE.TXT" DESTINATION share/licenses/dxc/DxilCompression COMPONENT dxc)
install(FILES "${dxc_headers}/LICENSE" DESTINATION share/licenses/dxc/DirectX-Headers COMPONENT dxc)
