cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)
include(simd)

project(defold_sound LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="SOUND")

set(SOUND_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(SOUND_SRC_DIR "${SOUND_ROOT}/src")
set(SOUND_DEPENDENCIES dlib)
set(SOUND_DECODER_OPUS_DEPENDENCIES dlib opus)
file(GLOB_RECURSE SOUND_DMSDK_HEADERS CONFIGURE_DEPENDS
  "${SOUND_SRC_DIR}/dmsdk/*.h"
  "${SOUND_SRC_DIR}/dmsdk/*.hpp")

set(SOUND_BASE_SOURCES
  "${SOUND_SRC_DIR}/sound_codec.cpp"
  "${SOUND_SRC_DIR}/sound_decoder.cpp")

if(TARGET_PLATFORM MATCHES "android")
  list(APPEND SOUND_BASE_SOURCES "${SOUND_SRC_DIR}/sound_android.cpp")
elseif(TARGET_PLATFORM MATCHES "ios$")
  list(APPEND SOUND_BASE_SOURCES "${SOUND_SRC_DIR}/sound_ios.mm")
elseif(TARGET_PLATFORM MATCHES "^(x86_64-ps4|x86_64-ps5)$")
  list(APPEND SOUND_BASE_SOURCES "${SOUND_SRC_DIR}/sound_ps4.cpp")
else()
  list(APPEND SOUND_BASE_SOURCES "${SOUND_SRC_DIR}/sound_generic.cpp")
endif()

set(SOUND_OPENAL_SOURCES
  ${SOUND_BASE_SOURCES}
  "${SOUND_SRC_DIR}/sound_openal.cpp"
  "${SOUND_SRC_DIR}/devices/device_openal.cpp")

set(SOUND_SOURCES ${SOUND_BASE_SOURCES} "${SOUND_SRC_DIR}/sound.cpp" "${SOUND_SRC_DIR}/sound_dsp.cpp")
if(TARGET_PLATFORM MATCHES "android")
  list(APPEND SOUND_SOURCES "${SOUND_SRC_DIR}/devices/device_opensl.cpp")
elseif(TARGET_PLATFORM MATCHES "web$")
  list(APPEND SOUND_SOURCES "${SOUND_SRC_DIR}/devices/device_js.cpp")
elseif(TARGET_PLATFORM MATCHES "nx64")
  list(APPEND SOUND_SOURCES "${SOUND_SRC_DIR}/devices/device_nx64.cpp")
elseif(TARGET_PLATFORM MATCHES "^(x86_64-ps4|x86_64-ps5)$")
  list(APPEND SOUND_SOURCES "${SOUND_SRC_DIR}/devices/device_ps4.cpp")
elseif(TARGET_PLATFORM_OS STREQUAL "win32" OR TARGET_PLATFORM STREQUAL "x86_64-xbone")
  list(APPEND SOUND_SOURCES "${SOUND_SRC_DIR}/devices/device_wasapi.cpp")
elseif(TARGET_PLATFORM MATCHES "^(x86_64-macos|arm64-macos|arm64-ios|arm64_sim-ios)$")
  list(APPEND SOUND_SOURCES "${SOUND_SRC_DIR}/devices/device_avaudio.mm")
else()
  list(APPEND SOUND_SOURCES "${SOUND_SRC_DIR}/devices/device_openal.cpp")
endif()

set(SOUND_DECODER_OGG_SOURCES
  "${SOUND_SRC_DIR}/decoders/decoder_stb_vorbis.cpp"
  "${SOUND_SRC_DIR}/stb_vorbis/stb_vorbis.c")
if(NOT TARGET_PLATFORM MATCHES "^(arm64-nx64|x86_64-ps4|x86_64-ps5|x86_64-xbone)$")
  list(APPEND SOUND_DECODER_OGG_SOURCES "${SOUND_SRC_DIR}/decoders/decoder_tremolo.cpp")
endif()

function(sound_configure_target target)
  target_include_directories(${target} PUBLIC "${SOUND_SRC_DIR}")
  target_compile_definitions(${target} PRIVATE "DLIB_LOG_DOMAIN=\"SOUND\"")
  defold_target_link_libraries(${target} "${TARGET_PLATFORM}" SCOPE PUBLIC ${SOUND_DEPENDENCIES})
  if(TARGET_PLATFORM_OS STREQUAL "win32")
    target_compile_definitions(${target} PRIVATE _XBOX)
    target_link_libraries(${target} PUBLIC ole32.lib)
  elseif(TARGET_PLATFORM MATCHES "^(x86_64-macos|arm64-macos|arm64-ios|arm64_sim-ios)$")
    target_link_libraries(${target} PUBLIC "-framework AVFoundation")
  elseif(TARGET_PLATFORM MATCHES "linux$")
    target_link_libraries(${target} PUBLIC openal)
  elseif(TARGET_PLATFORM MATCHES "android")
    target_link_libraries(${target} PUBLIC OpenSLES)
  endif()
  if(DEFOLD_PLATFORM_SOUND_LIBS)
    target_link_libraries(${target} PUBLIC ${DEFOLD_PLATFORM_SOUND_LIBS})
  endif()
endfunction()

# Higher-ISA objects contain complete loops; no register types cross the boundary.
function(sound_add_dsp_kernel target getter name lanes isa)
  # Keep direct Simd4/Simd8/Simd16 calls without duplicating the mixing loops.
  set(_dsp_template "${SOUND_SRC_DIR}/sound_dsp_simd.cpp")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_dsp_template}")
  file(READ "${_dsp_template}" _dsp_source)
  if(NOT lanes EQUAL 4)
    string(REGEX REPLACE "(^|[^A-Za-z0-9_])Simd4([A-Z])" "\\1Simd${lanes}\\2" _dsp_source "${_dsp_source}")
  endif()
  set(_dsp_file "${CMAKE_CURRENT_BINARY_DIR}/dsp/sound_dsp_simd${lanes}.cpp")
  file(CONFIGURE OUTPUT "${_dsp_file}" CONTENT "${_dsp_source}" @ONLY)
  add_library(${target} OBJECT EXCLUDE_FROM_ALL "${_dsp_file}")
  target_link_libraries(${target} PRIVATE defold_sdk)
  target_include_directories(${target} PRIVATE "${SOUND_SRC_DIR}" "${DEFOLD_HOME}/engine/dlib/src")
  string(REPLACE "GetDSPImpl" "DSP" _namespace "${getter}")
  target_compile_definitions(${target} PRIVATE
    DM_SOUND_DSP_NAMESPACE=${_namespace} DM_SOUND_DSP_GETTER=${getter} DM_SOUND_DSP_NAME="${name}" DM_SOUND_DSP_LANES=${lanes})
  set_target_properties(${target} PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION FALSE
    INTERPROCEDURAL_OPTIMIZATION_RELEASE FALSE
    INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO FALSE)
  if(isa STREQUAL "SCALAR")
    target_compile_definitions(${target} PRIVATE DM_SIMD_FORCE_SCALAR=1)
    defold_target_simd_x86_isa(${target} SSE2)
    if(TARGET_PLATFORM MATCHES "web$")
      target_compile_options(${target} PRIVATE -mno-simd128)
    elseif(TARGET_PLATFORM STREQUAL "armv7-android")
      target_compile_options(${target} PRIVATE -mfpu=vfpv3-d16)
    endif()
  elseif(isa STREQUAL "WASM")
    target_compile_options(${target} PRIVATE -msimd128)
  elseif(isa STREQUAL "SME")
    # Apple exposes SVE operations in SME streaming mode, without ordinary SVE.
    target_compile_options(${target} PRIVATE -march=armv8.6-a+sme+nosve -ffp-contract=off)
    target_compile_definitions(${target} PRIVATE DM_SIMD_USE_SME=1)
  elseif(isa STREQUAL "NEON")
    if(TARGET_PLATFORM STREQUAL "armv7-android")
      target_compile_options(${target} PRIVATE -mfpu=neon)
    endif()
  else()
    defold_target_simd_x86_isa(${target} "${isa}")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND isa MATCHES "^(AVX2|AVX512)$")
      # Keep scalar tails in VEX form until the isolated AVX kernel returns.
      target_compile_options(${target} PRIVATE /arch:AVX)
    endif()
  endif()
endfunction()

set(SOUND_DSP_KERNELS)
if(TARGET_PLATFORM MATCHES "^x86_64-(linux|macos|win32)$")
  sound_add_dsp_kernel(sound_dsp_sse3 GetDSPImplSSE3 SSE3 4 SSE3)
  sound_add_dsp_kernel(sound_dsp_sse42 GetDSPImplSSE42 SSE4.2 4 SSE4.2)
  sound_add_dsp_kernel(sound_dsp_avx2 GetDSPImplAVX2 AVX2 8 AVX2)
  sound_add_dsp_kernel(sound_dsp_avx512 GetDSPImplAVX512 AVX512 16 AVX512)
  set(SOUND_DSP_KERNELS sound_dsp_sse3 sound_dsp_sse42 sound_dsp_avx2 sound_dsp_avx512)
elseif(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
  sound_add_dsp_kernel(sound_dsp_wasm GetDSPImplWASM WASM 16 WASM)
  set(SOUND_DSP_KERNELS sound_dsp_wasm)
elseif(TARGET_PLATFORM MATCHES "^(arm64|arm64_sim|armv7)-")
  sound_add_dsp_kernel(sound_dsp_neon GetDSPImplNEON NEON 4 NEON)
  set(SOUND_DSP_KERNELS sound_dsp_neon)
  if(TARGET_PLATFORM STREQUAL "arm64-macos")
    sound_add_dsp_kernel(sound_dsp_neon8 GetDSPImplNEON8 NEON 8 NEON)
    sound_add_dsp_kernel(sound_dsp_neon16 GetDSPImplNEON16 NEON 16 NEON)
    list(APPEND SOUND_DSP_KERNELS sound_dsp_neon8 sound_dsp_neon16)
  endif()
  if(TARGET_PLATFORM STREQUAL "arm64-macos" AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    include(CheckCXXSourceCompiles)
    include(CMakePushCheckState)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_FLAGS "-march=armv8.6-a+sme+nosve")
    check_cxx_source_compiles("#include <arm_sve.h>\n#include <arm_sme.h>\n__arm_locally_streaming void kernel() { (void)svcntw(); }\nint main() { return 0; }" SOUND_WITH_SME)
    cmake_pop_check_state()
    if(SOUND_WITH_SME)
      sound_add_dsp_kernel(sound_dsp_sme GetDSPImplSME SME 16 SME)
      list(APPEND SOUND_DSP_KERNELS sound_dsp_sme)
    endif()
  endif()
endif()

function(sound_apply_simd_flags target fallback)
  target_include_directories(${target} PRIVATE "${DEFOLD_HOME}/engine/dlib/src")
  if(fallback)
    target_compile_definitions(${target} PRIVATE DM_SOUND_DSP_IMPL=Fallback)
    if(TARGET_PLATFORM MATCHES "web$")
      target_compile_options(${target} PRIVATE -mno-simd128)
    endif()
  elseif(TARGET_PLATFORM MATCHES "^x86_64-(linux|macos|win32)$")
    defold_target_simd_x86_isa(${target} SSE2)
    target_compile_definitions(${target} PRIVATE DM_SOUND_DSP_X86)
  elseif(TARGET_PLATFORM MATCHES "web$")
    target_compile_options(${target} PRIVATE -mno-simd128)
    target_compile_definitions(${target} PRIVATE DM_SOUND_DSP_WASM)
  elseif(TARGET_PLATFORM MATCHES "^(arm64|arm64_sim|armv7)-")
    target_compile_definitions(${target} PRIVATE DM_SOUND_DSP_NEON)
    if(TARGET_PLATFORM STREQUAL "arm64-macos")
      target_compile_definitions(${target} PRIVATE DM_SOUND_DSP_NEON_WIDE)
    endif()
    if(SOUND_WITH_SME)
      target_compile_definitions(${target} PRIVATE DM_SOUND_DSP_SME)
    endif()
    if(TARGET_PLATFORM STREQUAL "armv7-android")
      target_compile_options(${target} PRIVATE -mfpu=vfpv3-d16)
    endif()
  endif()
endfunction()

defold_add_library(decoder_wav STATIC "${SOUND_SRC_DIR}/decoders/decoder_wav.cpp")
target_include_directories(decoder_wav PUBLIC "${SOUND_SRC_DIR}")
defold_target_link_libraries(decoder_wav "${TARGET_PLATFORM}" SCOPE PUBLIC ${SOUND_DEPENDENCIES})

defold_add_library(decoder_ogg STATIC ${SOUND_DECODER_OGG_SOURCES})
target_include_directories(decoder_ogg PUBLIC "${SOUND_SRC_DIR}")
defold_target_link_libraries(decoder_ogg "${TARGET_PLATFORM}" SCOPE PUBLIC ${SOUND_DEPENDENCIES})

defold_add_library(decoder_opus STATIC "${SOUND_SRC_DIR}/decoders/decoder_opus.cpp")
target_include_directories(decoder_opus PUBLIC "${SOUND_SRC_DIR}")
defold_target_link_libraries(decoder_opus "${TARGET_PLATFORM}" SCOPE PUBLIC ${SOUND_DECODER_OPUS_DEPENDENCIES})

defold_add_library(sound STATIC ${SOUND_SOURCES})
sound_configure_target(sound)
sound_apply_simd_flags(sound OFF)
foreach(_kernel IN LISTS SOUND_DSP_KERNELS)
  target_sources(sound PRIVATE $<TARGET_OBJECTS:${_kernel}>)
endforeach()

if(NOT TARGET_PLATFORM MATCHES "(macos|ios)$")
  defold_add_library(sound_openal STATIC ${SOUND_OPENAL_SOURCES})
  sound_configure_target(sound_openal)
  set_target_properties(sound_openal PROPERTIES DEFOLD_SDK_HEADERS_TARGET sound_sdk_headers)
endif()

defold_add_library(sound_null STATIC
  "${SOUND_SRC_DIR}/devices/device_null.cpp"
  "${SOUND_SRC_DIR}/sound_null.cpp")
sound_configure_target(sound_null)
set_target_properties(sound_null PROPERTIES DEFOLD_SDK_HEADERS_TARGET sound_sdk_headers)

set(SOUND_INSTALL_TARGETS decoder_wav decoder_ogg decoder_opus sound sound_null)
if(TARGET sound_openal)
  list(APPEND SOUND_INSTALL_TARGETS sound_openal)
endif()
if(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
  defold_add_library(sound_nosimd STATIC ${SOUND_SOURCES})
  sound_configure_target(sound_nosimd)
  sound_apply_simd_flags(sound_nosimd ON)
  set_target_properties(sound_nosimd PROPERTIES DEFOLD_SDK_HEADERS_TARGET sound_sdk_headers)
  list(APPEND SOUND_INSTALL_TARGETS sound_nosimd)
endif()

if(TARGET_PLATFORM MATCHES "web$")
  defold_install_js_library("${SOUND_SRC_DIR}/js/library_sound.js")
elseif(TARGET_PLATFORM MATCHES "android$")
  include(tools_java)
  set(SOUND_ANDROID_CLASS_DIR "${CMAKE_CURRENT_BINARY_DIR}/java")
  set(SOUND_ANDROID_JAR "${CMAKE_CURRENT_BINARY_DIR}/sound_android.jar")
  file(GLOB_RECURSE SOUND_ANDROID_JAVA CONFIGURE_DEPENDS "${SOUND_SRC_DIR}/java/*.java")
  add_custom_command(
    OUTPUT "${SOUND_ANDROID_JAR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${SOUND_ANDROID_CLASS_DIR}"
    COMMAND "${DEFOLD_JAVAC_EXECUTABLE}" -g --release 8 -Xlint:-options -classpath "${DEFOLD_ANDROID_JAR}" -d "${SOUND_ANDROID_CLASS_DIR}" ${SOUND_ANDROID_JAVA}
    COMMAND "${Java_JAR_EXECUTABLE}" cf "${SOUND_ANDROID_JAR}" -C "${SOUND_ANDROID_CLASS_DIR}" .
    DEPENDS ${SOUND_ANDROID_JAVA}
    VERBATIM)
  add_custom_target(sound_android_jar ALL DEPENDS "${SOUND_ANDROID_JAR}")
  install(FILES "${SOUND_ANDROID_JAR}" DESTINATION share/java)
endif()

defold_install_targets(TARGETS ${SOUND_INSTALL_TARGETS}
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})

defold_add_sdk_headers_target(sound_public_sdk_headers
  DESTINATION include/sound
  SOURCE_ROOT "${SOUND_SRC_DIR}"
  FILES "${SOUND_SRC_DIR}/sound.h"
  DEPENDS sound)
defold_add_sdk_headers_target(sound_dmsdk_sdk_headers
  DESTINATION sdk/include/dmsdk
  SOURCE_ROOT "${SOUND_SRC_DIR}/dmsdk"
  FILES ${SOUND_DMSDK_HEADERS}
  DEPENDS sound)
set(SOUND_SDK_HEADER_TARGETS
  sound_public_sdk_headers
  sound_dmsdk_sdk_headers
  dlib_sdk_headers)
add_custom_target(sound_sdk_headers
  DEPENDS ${SOUND_SDK_HEADER_TARGETS})

install(FILES "${SOUND_SRC_DIR}/sound.h" DESTINATION include/sound)
install(DIRECTORY "${SOUND_SRC_DIR}/dmsdk/" DESTINATION sdk/include/dmsdk FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
