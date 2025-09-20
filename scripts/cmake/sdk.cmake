message("sdk.cmake:")

# NOTE: Minimum iOS-version is also specified in Info.plist-files
# (MinimumOSVersion and perhaps DTPlatformVersion)
set(SDK_VERSION_IPHONEOS_MIN "11.0")
set(SDK_VERSION_MACOSX_MIN "10.15")

if (TARGET_PLATFORM MATCHES "arm64-macos|x86_64-macos|arm64-ios|x86_64-ios")
    # Try to detect packaged toolchains (e.g. Xcode) inside DEFOLD_SDK_ROOT
    include(sdk_xcode)
elseif (TARGET_PLATFORM MATCHES "arm64-android|armv7-android")
    include(sdk_android)
elseif (TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
    include(sdk_linux)
elseif (TARGET_PLATFORM MATCHES "arm64-win32|x86_64-win32")
    include(sdk_windows)
elseif (TARGET_PLATFORM MATCHES "js-web|wasm-web|wasm_pthread-web")
    include(sdk_emscripten)
else()
    message(FATAL "Unsupported platform: ${TARGET_PLATFORM}")
    set(CMAKE_C_COMPILER "clang")
    set(CMAKE_CXX_COMPILER "clang++")
endif()
