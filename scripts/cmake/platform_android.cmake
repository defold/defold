defold_log("platform_android.cmake:")

# Try to resolve NDK root and sysroot
set(_DEFOLD_ANDROID_NDK "${ANDROID_NDK}")
if(NOT _DEFOLD_ANDROID_NDK AND DEFINED CMAKE_ANDROID_NDK)
    set(_DEFOLD_ANDROID_NDK "${CMAKE_ANDROID_NDK}")
endif()

set(_DEFOLD_SYSROOT "${CMAKE_SYSROOT}")

# Common compile definitions and options (mirrors waf_dynamo defaults)
target_compile_definitions(defold_sdk INTERFACE ANDROID)

target_compile_options(defold_sdk INTERFACE
  -gdwarf-2
  -ffunction-sections
  -fstack-protector
  -fomit-frame-pointer
  -fno-strict-aliasing
  -funwind-tables)

if(TARGET_PLATFORM MATCHES "arm64-android")
    target_compile_definitions(defold_sdk INTERFACE __aarch64__)
    target_compile_options(defold_sdk INTERFACE -march=armv8-a)
    target_link_options(defold_sdk INTERFACE -Wl,-z,max-page-size=16384)
else()
    target_compile_definitions(defold_sdk INTERFACE
        __ARM_ARCH_5__ __ARM_ARCH_5T__ __ARM_ARCH_5E__ __ARM_ARCH_5TE__
        GOOGLE_PROTOBUF_NO_RTTI)
    target_compile_options(defold_sdk INTERFACE -march=armv7-a -mfloat-abi=softfp -fvisibility=hidden)
    target_link_options(defold_sdk INTERFACE -Wl,--fix-cortex-a8)
endif()

# Add NDK helper include paths if available
if(_DEFOLD_ANDROID_NDK)
    target_include_directories(defold_sdk SYSTEM INTERFACE
      "${_DEFOLD_ANDROID_NDK}/sources/android/native_app_glue"
      "${_DEFOLD_ANDROID_NDK}/sources/android/cpufeatures")
endif()

# Link options (with generator expressions for arch-specific flags)
target_link_options(defold_sdk INTERFACE
  -Wl,--no-undefined
  -Wl,-z,noexecstack
  -landroid
  -llog
  -z text
  -Wl,--build-id=uuid
  -static-libstdc++)

