message("sdk.cmake:")

# NOTE: Minimum iOS-version is also specified in Info.plist-files
# (MinimumOSVersion and perhaps DTPlatformVersion)
set(SDK_VERSION_IPHONEOS_MIN "11.0")
set(SDK_VERSION_MACOSX_MIN "10.15")

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")

if (TARGET_PLATFORM MATCHES "arm64-macos|x86_64-macos")
    # Try to detect packaged toolchains (e.g. Xcode) inside DEFOLD_SDK_ROOT
    include(sdk_xcode)
endif()
