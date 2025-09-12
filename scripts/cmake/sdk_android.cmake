message("sdk_android.cmake:")

# Detect packaged Android NDK in ${DEFOLD_SDK_ROOT}/ext/SDKs and set toolchain file

if(NOT DEFINED DEFOLD_SDK_ROOT)
    message(DEBUG "sdk_android: DEFOLD_SDK_ROOT not set; skipping packaged Android NDK detection")
    return()
endif()

set(_SDKS_DIR "${DEFOLD_SDK_ROOT}/ext/SDKs")
if(NOT EXISTS "${_SDKS_DIR}")
    message(DEBUG "sdk_android: SDKs directory not found: ${_SDKS_DIR}")
    return()
endif()

# Common locations across NDK versions
set(_NDK_TOOLCHAIN_CANDIDATES
    "${_SDKS_DIR}/android-ndk-*/build/cmake/android.toolchain.cmake"
    "${_SDKS_DIR}/android-ndk-*/android.toolchain.cmake"
)

set(_ANDROID_TOOLCHAINS "")
foreach(_pat IN LISTS _NDK_TOOLCHAIN_CANDIDATES)
    file(GLOB _matches ${_pat})
    if(_matches)
        list(APPEND _ANDROID_TOOLCHAINS ${_matches})
    endif()
endforeach()

if(NOT _ANDROID_TOOLCHAINS)
    message(DEBUG "sdk_android: No packaged Android NDK toolchain found in ${_SDKS_DIR}")
    return()
endif()

list(SORT _ANDROID_TOOLCHAINS)
list(REVERSE _ANDROID_TOOLCHAINS)
list(GET _ANDROID_TOOLCHAINS 0 _ANDROID_TOOLCHAIN_FILE)

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

    message(STATUS "sdk_android: Using packaged Android NDK: ${_ANDROID_NDK_ROOT}")
    message(STATUS "sdk_android: Toolchain file: ${_ANDROID_TOOLCHAIN_FILE}")

    # Set toolchain before project(); users can still override on the command line
    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE "${_ANDROID_TOOLCHAIN_FILE}" CACHE FILEPATH "Android NDK toolchain file" FORCE)
    endif()
    # Hint the NDK root
    set(ANDROID_NDK "${_ANDROID_NDK_ROOT}" CACHE PATH "Android NDK root" FORCE)
endif()

if (NOT CMAKE_TOOLCHAIN_FILE)
    message(FATAL "Failed to find android.toolchain.cmake")
endif()


if(TARGET_PLATFORM MATCHES "arm64-android")
    # For arm64-android, ensure Clang uses aarch64-linux-android21 target triple
    set(ANDROID_ABI "arm64-v8a" CACHE STRING "Android ABI" FORCE)
    set(ANDROID_NATIVE_API_LEVEL 21 CACHE STRING "Android API Level" FORCE)
    set(ANDROID_TOOLCHAIN "aarch64-linux-android${ANDROID_NATIVE_API_LEVEL}-clang" CACHE STRING "Android Toolchain" FORCE)

else(TARGET_PLATFORM MATCHES "armv7-android")
    # For armv7-android, ensure Clang uses aarch64-linux-android21 target triple
    set(ANDROID_ABI "armeabi-v7a" CACHE STRING "Android ABI" FORCE)
    set(ANDROID_NATIVE_API_LEVEL 19 CACHE STRING "Android API Level" FORCE)
    set(ANDROID_TOOLCHAIN "armv7-linux-androidi${ANDROID_NATIVE_API_LEVEL}-clang" CACHE STRING "Android Toolchain" FORCE)

endif()

message(STATUS "ANDROID_ABI: ${ANDROID_ABI}")
message(STATUS "ANDROID_NATIVE_API_LEVEL: ${ANDROID_NATIVE_API_LEVEL}")
message(STATUS "ANDROID_TOOLCHAIN: ${ANDROID_TOOLCHAIN}")

# the NDK toolchain uses cmake 3.6
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
include(${CMAKE_TOOLCHAIN_FILE})
