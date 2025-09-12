message("platform_android.cmake:")

# Try to resolve NDK root and sysroot
set(_DEFOLD_ANDROID_NDK "${ANDROID_NDK}")
if(NOT _DEFOLD_ANDROID_NDK AND DEFINED CMAKE_ANDROID_NDK)
    set(_DEFOLD_ANDROID_NDK "${CMAKE_ANDROID_NDK}")
endif()

set(_DEFOLD_SYSROOT "${CMAKE_SYSROOT}")

# Common compile definitions and options (mirrors waf_dynamo defaults)
add_compile_definitions(
    ANDROID
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gdwarf-2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fomit-frame-pointer")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -funwind-tables")

if(TARGET_PLATFORM MATCHES "arm64-android")
    add_compile_definitions(__aarch64__)
    add_compile_options(-march=armv8-a)
else()
    add_compile_definitions(
        __ARM_ARCH_5__ __ARM_ARCH_5T__ __ARM_ARCH_5E__ __ARM_ARCH_5TE__
        GOOGLE_PROTOBUF_NO_RTTI
    )
    add_compile_options(-march=armv7-a -mfloat-abi=softfp -fvisibility=hidden)
endif()

# Add NDK helper include paths if available
if(_DEFOLD_ANDROID_NDK)
    include_directories(
        "${_DEFOLD_ANDROID_NDK}/sources/android/native_app_glue"
        "${_DEFOLD_ANDROID_NDK}/sources/android/cpufeatures"
    )
endif()

# Link options (mirrors waf_dynamo getAndroidLinkFlags + extras)
set(_DEFOLD_ANDROID_LINK_OPTS
    -Wl,--no-undefined
    -Wl,-z,noexecstack
    -landroid
    -llog
    -z text
    -Wl,--build-id=uuid
    -static-libstdc++
)

if(TARGET_PLATFORM MATCHES "arm64-android")
    list(APPEND _DEFOLD_ANDROID_LINK_OPTS -Wl,-z,max-page-size=16384)
else()
    list(APPEND _DEFOLD_ANDROID_LINK_OPTS -Wl,--fix-cortex-a8)
endif()

add_link_options(${_DEFOLD_ANDROID_LINK_OPTS})
