# Decode Vorbis packets with libnogg and stream Ogg pages with the vendored libogg.
include(FetchContent)
find_package(Git REQUIRED)
set(_nogg_patch "${CMAKE_CURRENT_LIST_DIR}/defold-simd.patch")
# A changed patch must be applied to a fresh copy of the upstream sources.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_nogg_patch}")
file(SHA256 "${_nogg_patch}" _nogg_patch_hash)
string(SUBSTRING "${_nogg_patch_hash}" 0 12 _nogg_patch_hash)
FetchContent_Declare(defold_libnogg
  URL "${CMAKE_CURRENT_LIST_DIR}/libnogg-1.18-source.tar.gz"
  URL_HASH SHA256=7bee6d63c0808f879696a4d1781ab1d251929a40b6f7a571daa0997aff7f8a9c
  SOURCE_DIR "${FETCHCONTENT_BASE_DIR}/nogg-${_nogg_patch_hash}"
  PATCH_COMMAND "${CMAKE_COMMAND}" "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
                "-DPATCH_FILE=${_nogg_patch}"
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
  decode/setup.c decode/stb_vorbis.c util/float-to-int16.c util/memory.c)
list(TRANSFORM _nogg_sources PREPEND "${defold_libnogg_SOURCE_DIR}/src/")
function(sound_add_vorbis_decoder target)
  defold_add_library(${target} STATIC
    "${SOUND_SRC_DIR}/decoders/decoder_vorbis.cpp" "${_ogg}/src/framing.c" ${_nogg_sources})
  target_include_directories(${target} PRIVATE "${SOUND_SRC_DIR}" "${defold_libnogg_SOURCE_DIR}"
    "${_ogg}/include" "${CMAKE_CURRENT_BINARY_DIR}/nogg")
  defold_target_link_libraries(${target} "${TARGET_PLATFORM}" SCOPE PUBLIC dlib)
endfunction()

sound_add_vorbis_decoder(decoder_ogg_nosimd)
if(TARGET_PLATFORM MATCHES "web$")
  target_compile_options(decoder_ogg_nosimd PRIVATE -mno-simd128)
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
  if(CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(decoder_ogg_nosimd PRIVATE /clang:-fno-vectorize /clang:-fno-slp-vectorize)
  else()
    target_compile_options(decoder_ogg_nosimd PRIVATE -fno-vectorize -fno-slp-vectorize)
  endif()
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  target_compile_options(decoder_ogg_nosimd PRIVATE -fno-tree-vectorize)
endif()

sound_add_vorbis_decoder(decoder_ogg)
if(TARGET_PLATFORM MATCHES "^arm64" OR
   (TARGET_PLATFORM STREQUAL "armv7-android" AND (CMAKE_ANDROID_ARM_NEON OR ANDROID_ARM_NEON)))
  target_compile_definitions(decoder_ogg PRIVATE ENABLE_ASM_ARM_NEON)
elseif(TARGET_PLATFORM MATCHES "^x86_64")
  defold_target_simd_x86_isa(decoder_ogg SSE2)
  target_compile_definitions(decoder_ogg PRIVATE ENABLE_ASM_X86_SSE2)
elseif(TARGET_PLATFORM MATCHES "web$")
  target_compile_options(decoder_ogg PRIVATE -msimd128 -mfpu=neon)
  target_compile_definitions(decoder_ogg PRIVATE ENABLE_ASM_ARM_NEON)
endif()
