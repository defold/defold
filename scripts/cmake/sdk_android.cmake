defold_log("sdk_android.cmake:")

# Detect Android NDK toolchain from (in order):
# 1) Packaged NDK under ${DEFOLD_SDK_ROOT}/ext/SDKs
# 2) Local Android SDK (Android Studio) via ANDROID_SDK_ROOT / ANDROID_HOME
# 3) Well-known default SDK locations on host OS

set(_ANDROID_TOOLCHAINS "")

# 1) Packaged NDK under DEFOLD_SDK_ROOT
if(DEFINED DEFOLD_SDK_ROOT)
    set(_SDKS_DIR "${DEFOLD_SDK_ROOT}/ext/SDKs")
    if(EXISTS "${_SDKS_DIR}")
        set(_NDK_TOOLCHAIN_CANDIDATES
            "${_SDKS_DIR}/android-ndk-*/build/cmake/android.toolchain.cmake"
            "${_SDKS_DIR}/android-ndk-*/android.toolchain.cmake"
        )
        foreach(_pat IN LISTS _NDK_TOOLCHAIN_CANDIDATES)
            file(GLOB _matches ${_pat})
            if(_matches)
                list(APPEND _ANDROID_TOOLCHAINS ${_matches})
            endif()
        endforeach()
        if(NOT _ANDROID_TOOLCHAINS)
            message(DEBUG "sdk_android: No packaged Android NDK toolchain found in ${_SDKS_DIR}")
        endif()
    else()
        message(DEBUG "sdk_android: SDKs directory not found: ${_SDKS_DIR}")
    endif()
else()
    message(DEBUG "sdk_android: DEFOLD_SDK_ROOT not set; skipping packaged NDK detection")
endif()

# Helper: add toolchains from an Android SDK path
function(_defold_android_sdk_add_toolchains SDK_PATH)
    if(NOT EXISTS "${SDK_PATH}")
        return()
    endif()
    file(GLOB _ndk_roots
        "${SDK_PATH}/ndk/*"
        "${SDK_PATH}/ndk-bundle"
    )
    foreach(_ndk IN LISTS _ndk_roots)
        if(EXISTS "${_ndk}/build/cmake/android.toolchain.cmake")
            list(APPEND _ANDROID_TOOLCHAINS "${_ndk}/build/cmake/android.toolchain.cmake")
        elseif(EXISTS "${_ndk}/android.toolchain.cmake")
            list(APPEND _ANDROID_TOOLCHAINS "${_ndk}/android.toolchain.cmake")
        endif()
    endforeach()
    set(_ANDROID_TOOLCHAINS "${_ANDROID_TOOLCHAINS}" PARENT_SCOPE)
endfunction()

# 2) Local Android SDK via env vars
if(NOT _ANDROID_TOOLCHAINS)
    if(DEFINED ENV{ANDROID_SDK_ROOT})
        _defold_android_sdk_add_toolchains("$ENV{ANDROID_SDK_ROOT}")
    elseif(DEFINED ENV{ANDROID_HOME})
        _defold_android_sdk_add_toolchains("$ENV{ANDROID_HOME}")
    endif()
endif()

# 3) Well-known default SDK roots
if(NOT _ANDROID_TOOLCHAINS)
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        _defold_android_sdk_add_toolchains("$ENV{HOME}/Library/Android/sdk")
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
        _defold_android_sdk_add_toolchains("$ENV{HOME}/Android/Sdk")
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        if(DEFINED ENV{LOCALAPPDATA})
            _defold_android_sdk_add_toolchains("$ENV{LOCALAPPDATA}/Android/Sdk")
        endif()
        if(DEFINED ENV{APPDATA})
            _defold_android_sdk_add_toolchains("$ENV{APPDATA}/Android/Sdk")
        endif()
    endif()
endif()

# Pick the most recent toolchain file
if(_ANDROID_TOOLCHAINS)
    list(SORT _ANDROID_TOOLCHAINS COMPARE NATURAL)
    list(REVERSE _ANDROID_TOOLCHAINS)
    list(GET _ANDROID_TOOLCHAINS 0 _ANDROID_TOOLCHAIN_FILE)
endif()

if(EXISTS "${_ANDROID_TOOLCHAIN_FILE}")
    get_filename_component(_toolchain_dir "${_ANDROID_TOOLCHAIN_FILE}" DIRECTORY)
    # Typical path: <ndk_root>/build/cmake/android.toolchain.cmake
    # NDK root is either parent (android.toolchain.cmake in root) or grandparent (build/cmake/...)
    get_filename_component(_ndk_parent "${_toolchain_dir}" DIRECTORY)
    if(EXISTS "${_ndk_parent}/source.properties")
        set(_ANDROID_NDK_ROOT "${_ndk_parent}")
    else()
        get_filename_component(_ndk_grandparent "${_ndk_parent}" DIRECTORY)
        set(_ANDROID_NDK_ROOT "${_ndk_grandparent}")
    endif()

    if(_SDKS_DIR AND _ANDROID_TOOLCHAIN_FILE MATCHES "^${_SDKS_DIR}.*")
    defold_log("sdk_android: Using packaged Android NDK: ${_ANDROID_NDK_ROOT}")
    else()
    defold_log("sdk_android: Using local Android SDK NDK: ${_ANDROID_NDK_ROOT}")
    endif()
    defold_log("sdk_android: Toolchain file: ${_ANDROID_TOOLCHAIN_FILE}")

    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE "${_ANDROID_TOOLCHAIN_FILE}" CACHE FILEPATH "Android NDK toolchain file" FORCE)
    endif()
    set(ANDROID_NDK "${_ANDROID_NDK_ROOT}" CACHE PATH "Android NDK root" FORCE)
else()
    message(FATAL_ERROR "sdk_android: Failed to find android.toolchain.cmake in packaged or local Android SDKs")
endif()


if(TARGET_PLATFORM MATCHES "arm64-android")
    # For arm64-android, ensure Clang uses aarch64-linux-android21 target triple
    set(ANDROID_ABI "arm64-v8a" CACHE STRING "Android ABI" FORCE)
    set(ANDROID_NATIVE_API_LEVEL ${SDK_VERSION_ANDROID_ARM64_API_LEVEL} CACHE STRING "Android API Level" FORCE)
    set(ANDROID_TOOLCHAIN "aarch64-linux-android${ANDROID_NATIVE_API_LEVEL}-clang" CACHE STRING "Android Toolchain" FORCE)

else(TARGET_PLATFORM MATCHES "armv7-android")
    # For armv7-android, ensure Clang uses armv7-linux-android21 target triple
    set(ANDROID_ABI "armeabi-v7a" CACHE STRING "Android ABI" FORCE)
    set(ANDROID_NATIVE_API_LEVEL ${SDK_VERSION_ANDROID_ARMV7_API_LEVEL} CACHE STRING "Android API Level" FORCE)
    # Use the canonical Clang triplet for 32-bit ARM
    set(ANDROID_TOOLCHAIN "armv7a-linux-androideabi${ANDROID_NATIVE_API_LEVEL}-clang" CACHE STRING "Android Toolchain" FORCE)

endif()

defold_log("ANDROID_ABI: ${ANDROID_ABI}")
defold_log("ANDROID_NATIVE_API_LEVEL: ${ANDROID_NATIVE_API_LEVEL}")
defold_log("ANDROID_TOOLCHAIN: ${ANDROID_TOOLCHAIN}")

# the NDK toolchain uses cmake 3.6
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
include(${CMAKE_TOOLCHAIN_FILE})
