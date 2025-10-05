if(DEFINED DEFOLD_SDK_CMAKE_INCLUDED)
    return()
endif()
set(DEFOLD_SDK_CMAKE_INCLUDED ON CACHE INTERNAL "sdk.cmake include guard")

if(DEFINED DEFOLD_SDK_DETECTED)
    # Avoid re-running detection when sdk.cmake is included multiple times
    return()
endif()

defold_log("sdk.cmake:")

# Bootstrap DEFOLD_HOME/DEFOLD_SDK_ROOT when running before defold.cmake
if(NOT DEFINED DEFOLD_HOME)
    get_filename_component(DEFOLD_HOME "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()
if(NOT DEFINED DEFOLD_SDK_ROOT)
    if(DEFINED ENV{DYNAMO_HOME} AND EXISTS "$ENV{DYNAMO_HOME}")
        set(DEFOLD_SDK_ROOT "$ENV{DYNAMO_HOME}" CACHE PATH "Path to Defold SDK (tmp/dynamo_home)" FORCE)
    elseif(EXISTS "${DEFOLD_HOME}/tmp/dynamo_home")
        set(DEFOLD_SDK_ROOT "${DEFOLD_HOME}/tmp/dynamo_home" CACHE PATH "Path to Defold SDK (tmp/dynamo_home)" FORCE)
    endif()
endif()

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
elseif (TARGET_PLATFORM MATCHES "arm64-win32|x86_64-win32|x86-win32")
    include(sdk_windows)
elseif (TARGET_PLATFORM MATCHES "js-web|wasm-web|wasm_pthread-web")
    include(sdk_emscripten)
else()
    message(FATAL "Unsupported platform: ${TARGET_PLATFORM}")
endif()

set(DEFOLD_SDK_DETECTED ON CACHE INTERNAL "Defold SDK/toolchain detection has run")
