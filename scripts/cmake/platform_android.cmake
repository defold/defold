defold_log("platform_android.cmake:")

# Try to resolve NDK root and sysroot
set(_DEFOLD_ANDROID_NDK "${ANDROID_NDK}")
if(NOT _DEFOLD_ANDROID_NDK AND DEFINED CMAKE_ANDROID_NDK)
    set(_DEFOLD_ANDROID_NDK "${CMAKE_ANDROID_NDK}")
endif()

set(_DEFOLD_SYSROOT "${CMAKE_SYSROOT}")

# Common compile definitions and options (mirrors waf_dynamo defaults)
target_compile_definitions(defold_sdk INTERFACE ANDROID)
target_compile_definitions(defold_sdk INTERFACE DM_HOSTFS=\"\")

target_compile_options(defold_sdk INTERFACE
  -gdwarf-2
  -ffunction-sections
  -fdata-sections
  -fstack-protector
  -fno-strict-aliasing
  -funwind-tables)

if(NOT WITH_HWASAN AND NOT WITH_ASAN)
    target_compile_options(defold_sdk INTERFACE -fomit-frame-pointer)
endif()

if(TARGET_PLATFORM MATCHES "arm64-android")
    target_compile_definitions(defold_sdk INTERFACE __aarch64__)
    target_compile_options(defold_sdk INTERFACE -march=armv8-a)
    target_link_options(defold_sdk INTERFACE -Wl,-z,max-page-size=16384)
elseif(TARGET_PLATFORM MATCHES "x86_64-android")
    # No -march: the NDK x86_64-linux-android<api>-clang wrapper already targets the
    # baseline mandated by the Android x86_64 ABI (SSE4.2 + POPCNT).
    target_compile_definitions(defold_sdk INTERFACE GOOGLE_PROTOBUF_NO_RTTI)
    target_compile_options(defold_sdk INTERFACE -fvisibility=hidden)
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
  -Wl,--gc-sections
  -Wl,--no-undefined
  -Wl,-z,noexecstack
  -landroid
  -llog
  -z text
  -Wl,--build-id=uuid)

if(NOT WITH_HWASAN)
    target_link_options(defold_sdk INTERFACE -static-libstdc++)
endif()

set(DEFOLD_ANDROID_TEST_ENV)
if(WITH_ASAN AND BUILD_TESTS)
    if(TARGET_PLATFORM STREQUAL "arm64-android")
        set(_asan_arch aarch64)
    elseif(TARGET_PLATFORM STREQUAL "x86_64-android")
        set(_asan_arch x86_64)
    else()
        set(_asan_arch arm)
    endif()
    # Query the compiler so the runtime always matches the NDK used to build.
    execute_process(
        COMMAND "${CMAKE_CXX_COMPILER}" "--target=${CMAKE_CXX_COMPILER_TARGET}"
            "--print-file-name=libclang_rt.asan-${_asan_arch}-android.so"
        RESULT_VARIABLE _asan_result
        OUTPUT_VARIABLE _asan_runtime
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT _asan_result EQUAL 0 OR NOT EXISTS "${_asan_runtime}")
        message(FATAL_ERROR "Android ASAN runtime not found: ${_asan_runtime}")
    endif()
    set(DEFOLD_ANDROID_TEST_ENV "${CMAKE_COMMAND}" -E env "ANDROID_ASAN_RUNTIME=${_asan_runtime}")
endif()
