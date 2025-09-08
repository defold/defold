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

include(${CMAKE_TOOLCHAIN_FILE})
