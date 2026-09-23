defold_log("platform_ios.cmake")

if(DEFOLD_TARGET_SYSROOT)
  set(_DEFOLD_IOS_SYSROOT "${DEFOLD_TARGET_SYSROOT}")
else()
  set(_DEFOLD_IOS_SYSROOT "${CMAKE_OSX_SYSROOT}")
endif()

set(_DEFOLD_TARGET_ARCH "arm64")

# Ensure CMake uses the target arch (overrides host default when needed)
set(CMAKE_OSX_ARCHITECTURES "${_DEFOLD_TARGET_ARCH}" CACHE STRING "Defold target arch" FORCE)

# Minimum iOS version: prefer CMAKE_OSX_DEPLOYMENT_TARGET if set
set(_DEFOLD_IPHONEOS_MIN "${CMAKE_OSX_DEPLOYMENT_TARGET}")
if(NOT _DEFOLD_IPHONEOS_MIN)
  set(_DEFOLD_IPHONEOS_MIN ${SDK_VERSION_IPHONEOS_MIN})
endif()

# Compile definitions (mirrors waf_dynamo iOS defaults)
target_compile_definitions(defold_sdk INTERFACE
  DM_PLATFORM_IOS
  DM_NO_SYSTEM_FUNCTION
  DM_HOSTFS=\"\")

if(TARGET_PLATFORM STREQUAL "arm64_sim-ios")
  target_compile_definitions(defold_sdk INTERFACE DM_PLATFORM_IOS_SIMULATOR IOS_SIMULATOR)
  # Guard: Simulator builds must use an iPhoneSimulator SDK
  if(DEFINED _DEFOLD_IOS_SYSROOT AND NOT _DEFOLD_IOS_SYSROOT MATCHES ".*iPhoneSimulator.*\.sdk/?$")
    message(FATAL_ERROR "TARGET_PLATFORM=arm64_sim-ios requires iPhoneSimulator SDK; found: ${_DEFOLD_IOS_SYSROOT}")
  endif()
  # The simulator flag makes clang/ld mark binaries with the IOSSIMULATOR
  # platform, which simctl requires for installation on Apple Silicon runtimes.
  set(_DEFOLD_IOS_VERSION_MIN_FLAG "-mios-simulator-version-min=${_DEFOLD_IPHONEOS_MIN}")
else()
  # Guard: Device builds must use an iPhoneOS SDK
  if(DEFINED _DEFOLD_IOS_SYSROOT AND _DEFOLD_IOS_SYSROOT MATCHES ".*iPhoneSimulator.*\.sdk/?$")
    message(FATAL_ERROR "TARGET_PLATFORM=arm64-ios requires iPhoneOS SDK; found: ${_DEFOLD_IOS_SYSROOT}")
  endif()
  set(_DEFOLD_IOS_VERSION_MIN_FLAG "-miphoneos-version-min=${_DEFOLD_IPHONEOS_MIN}")
endif()

# Common iOS compile options
target_compile_options(defold_sdk INTERFACE
  ${_DEFOLD_IOS_VERSION_MIN_FLAG})

# Add sysroot to compile flags when available
target_compile_options(defold_sdk INTERFACE -isysroot ${_DEFOLD_IOS_SYSROOT})

# Add libc++ include dir and disable default stdlib includes for C++
target_compile_options(defold_sdk INTERFACE
  $<$<COMPILE_LANGUAGE:CXX>:-isystem>
  $<$<COMPILE_LANGUAGE:CXX>:${_DEFOLD_IOS_SYSROOT}/usr/include/c++/v1>)

# C++ specific (match waf: libc++, no default stdlib includes)
target_compile_options(defold_sdk INTERFACE
  $<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>
  $<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>)

# Link options (expressed directly)
target_link_options(defold_sdk INTERFACE
  -stdlib=libc++
  ${_DEFOLD_IOS_VERSION_MIN_FLAG}
  -dead_strip
  # Frameworks
  -Wl,-framework,UIKit
  -Wl,-framework,SystemConfiguration
  -Wl,-framework,AVFoundation
  -Wl,-weak_framework,Foundation
  # Ensure Objective-C runtime is linked on host builds
  -fobjc-link-runtime
  -isysroot ${_DEFOLD_IOS_SYSROOT})
