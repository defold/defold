# Decode Vorbis packets with libnogg and stream Ogg pages with the vendored libogg.
include(FetchContent)
find_package(Git REQUIRED)
FetchContent_Declare(defold_libnogg
  URL https://achurch.org/libnogg/tarballs/libnogg-1.18.tar.gz
  URL_HASH SHA256=9600b7d2c03e447bb0ccbb3541bbe07714f5cd4fe00225ae754463b1b592a370
  PATCH_COMMAND "${CMAKE_COMMAND}" "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
                "-DPATCH_FILE=${CMAKE_CURRENT_LIST_DIR}/defold-simd.patch"
                -P "${CMAKE_CURRENT_LIST_DIR}/apply_patch.cmake")
FetchContent_MakeAvailable(defold_libnogg)

set(_ogg "${DEFOLD_HOME}/external/ogg/package/libogg-1.3.5")
set(INCLUDE_INTTYPES_H 1)
set(INCLUDE_STDINT_H 1)
set(INCLUDE_SYS_TYPES_H 1)
set(SIZE16 int16_t)
set(USIZE16 uint16_t)
set(SIZE32 int32_t)
set(USIZE32 uint32_t)
set(SIZE64 int64_t)
set(USIZE64 uint64_t)
configure_file("${_ogg}/include/ogg/config_types.h.in" "${CMAKE_CURRENT_BINARY_DIR}/nogg/ogg/config_types.h" @ONLY)

set(_nogg_sources decode/crc32.c decode/decode.c decode/io.c decode/packet.c
  decode/setup.c decode/stb_vorbis.c util/memory.c)
list(TRANSFORM _nogg_sources PREPEND "${defold_libnogg_SOURCE_DIR}/src/")
defold_add_library(decoder_nogg STATIC
  "${SOUND_SRC_DIR}/decoders/decoder_ogg.cpp" "${_ogg}/src/framing.c" ${_nogg_sources})
target_include_directories(decoder_nogg PRIVATE "${SOUND_SRC_DIR}" "${defold_libnogg_SOURCE_DIR}"
  "${_ogg}/include" "${CMAKE_CURRENT_BINARY_DIR}/nogg")
defold_target_link_libraries(decoder_nogg "${TARGET_PLATFORM}" SCOPE PUBLIC dlib)

if(TARGET_PLATFORM MATCHES "^arm64" OR
   (TARGET_PLATFORM STREQUAL "armv7-android" AND (CMAKE_ANDROID_ARM_NEON OR ANDROID_ARM_NEON)))
  target_compile_definitions(decoder_nogg PRIVATE ENABLE_ASM_ARM_NEON)
elseif(TARGET_PLATFORM MATCHES "^x86_64")
  defold_target_simd_x86_isa(decoder_nogg SSE2)
  target_compile_definitions(decoder_nogg PRIVATE ENABLE_ASM_X86_SSE2)
elseif(TARGET_PLATFORM MATCHES "web$")
  defold_feature_enabled(simd _nogg_web_simd)
  if(_nogg_web_simd)
    target_compile_options(decoder_nogg PRIVATE -msimd128 -mfpu=neon)
    target_compile_definitions(decoder_nogg PRIVATE ENABLE_ASM_ARM_NEON)
  endif()
endif()
