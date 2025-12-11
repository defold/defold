defold_log("platform_macos.cmake")

# Derive arch from TARGET_PLATFORM
if(TARGET_PLATFORM MATCHES "^arm64-")
  set(_DEFOLD_TARGET_ARCH "arm64")
else()
  set(_DEFOLD_TARGET_ARCH "x86_64")
endif()

# Ensure CMake uses the derived arch (overrides host default when needed)
set(CMAKE_OSX_ARCHITECTURES "${_DEFOLD_TARGET_ARCH}" CACHE STRING "Defold target arch" FORCE)

# Since it's a cmake feature, let's check it first if it's overridden
set(_DEFOLD_MACOS_MIN "${CMAKE_OSX_DEPLOYMENT_TARGET}")
if(NOT _DEFOLD_MACOS_MIN)
  # Fallback if not provided by the toolchain
  set(_DEFOLD_MACOS_MIN ${SDK_VERSION_MACOSX_MIN})
endif()

# Common compile options for macOS (mirrors waf_dynamo defaults)
target_compile_definitions(defold_sdk INTERFACE
  DM_PLATFORM_MACOS
  GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
  GL_SILENCE_DEPRECATION)

target_compile_options(defold_sdk INTERFACE
  -mmacosx-version-min=${SDK_VERSION_MACOSX_MIN}
  -target ${_DEFOLD_TARGET_ARCH}-apple-darwin19)

if(_XCODE_TOOLCHAIN)
  # Add libc++ include dir and disable default stdlib includes for C++
  target_compile_options(defold_sdk INTERFACE
    $<$<COMPILE_LANGUAGE:CXX>:-isystem>
    $<$<COMPILE_LANGUAGE:CXX>:${CMAKE_OSX_SYSROOT}/usr/include/c++/v1>)
endif()

# C++ specific
target_compile_options(defold_sdk INTERFACE
  $<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>
  $<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>)

# Link options (use generator expressions for optional sysroot)
target_link_options(defold_sdk INTERFACE
  -stdlib=libc++
  -mmacosx-version-min=${_DEFOLD_MACOS_MIN}
  -target ${_DEFOLD_TARGET_ARCH}-apple-darwin19
  # Frameworks
  -Wl,-framework,AppKit
  -Wl,-framework,Carbon
  -Wl,-weak_framework,Foundation
  # Always assume Xcode toolchain in use
  -isysroot ${CMAKE_OSX_SYSROOT})
