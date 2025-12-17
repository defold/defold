defold_log("platform_ios.cmake")

# Derive arch from TARGET_PLATFORM
if(TARGET_PLATFORM MATCHES "^arm64-")
  set(_DEFOLD_TARGET_ARCH "arm64")
else()
  set(_DEFOLD_TARGET_ARCH "x86_64")
endif()

# Ensure CMake uses the derived arch (overrides host default when needed)
set(CMAKE_OSX_ARCHITECTURES "${_DEFOLD_TARGET_ARCH}" CACHE STRING "Defold target arch" FORCE)

# Minimum iOS version: prefer CMAKE_OSX_DEPLOYMENT_TARGET if set
set(_DEFOLD_IPHONEOS_MIN "${CMAKE_OSX_DEPLOYMENT_TARGET}")
if(NOT _DEFOLD_IPHONEOS_MIN)
  set(_DEFOLD_IPHONEOS_MIN ${SDK_VERSION_IPHONEOS_MIN})
endif()

# Compile definitions (mirrors waf_dynamo iOS defaults)
target_compile_definitions(defold_sdk INTERFACE DM_PLATFORM_IOS)

if(_DEFOLD_TARGET_ARCH STREQUAL "x86_64")
  target_compile_definitions(defold_sdk INTERFACE DM_PLATFORM_IOS_SIMULATOR IOS_SIMULATOR)
  # Guard: Simulator builds must use an iPhoneSimulator SDK
  if(DEFINED CMAKE_OSX_SYSROOT AND NOT CMAKE_OSX_SYSROOT MATCHES ".*iPhoneSimulator.*\.sdk/?$")
    message(FATAL_ERROR "TARGET_PLATFORM=x86_64-ios requires iPhoneSimulator SDK; found: ${CMAKE_OSX_SYSROOT}")
  endif()
endif()

# Common iOS compile options
target_compile_options(defold_sdk INTERFACE
  -miphoneos-version-min=${_DEFOLD_IPHONEOS_MIN})

# Add sysroot to compile flags when available
target_compile_options(defold_sdk INTERFACE -isysroot ${CMAKE_OSX_SYSROOT})

# Add libc++ include dir and disable default stdlib includes for C++
target_compile_options(defold_sdk INTERFACE
  $<$<COMPILE_LANGUAGE:CXX>:-isystem>
  $<$<COMPILE_LANGUAGE:CXX>:${CMAKE_OSX_SYSROOT}/usr/include/c++/v1>)

# C++ specific (match waf: libc++, no default stdlib includes)
target_compile_options(defold_sdk INTERFACE
  $<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>
  $<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>)

# Link options (expressed directly)
target_link_options(defold_sdk INTERFACE
  -stdlib=libc++
  -miphoneos-version-min=${_DEFOLD_IPHONEOS_MIN}
  -dead_strip
  # Frameworks
  -Wl,-framework,UIKit
  -Wl,-framework,SystemConfiguration
  -Wl,-framework,AVFoundation
  -Wl,-weak_framework,Foundation
  # Ensure Objective-C runtime is linked on host builds
  -fobjc-link-runtime
  -isysroot ${CMAKE_OSX_SYSROOT})
