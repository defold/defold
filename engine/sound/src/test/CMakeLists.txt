cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

set(SOUND_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(SOUND_ROOT "${SOUND_TEST_ROOT}/../.." ABSOLUTE)
set(SOUND_SRC_DIR "${SOUND_ROOT}/src")
set(SOUND_TEST_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}")

find_program(SOUND_TEST_PYTHON NAMES python3 python REQUIRED)

set(SOUND_TEST_WAVS
  booster_on_sfx.wav
  door_opening.wav
  drumloop.wav
  onefootstep.wav
  osc2_sin_440hz.wav
  def2938.wav
  music_adpcm.wav
  mono_resample_framecount_16000_adpcm.wav
  ambience_adpcm.wav)

function(sound_generate_wav out_name tone rate frames channels ramp dc)
  set(_out "${SOUND_TEST_GEN_DIR}/test/${out_name}")
  set(_args --output "${_out}" --rate "${rate}" --frames "${frames}" --channels "${channels}")
  if(tone)
    list(APPEND _args --tone "${tone}")
  endif()
  if(ramp)
    list(APPEND _args --ramp)
  endif()
  if(dc)
    list(APPEND _args --dc)
  endif()
  add_custom_command(
    OUTPUT "${_out}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${SOUND_TEST_GEN_DIR}/test"
    COMMAND "${SOUND_TEST_PYTHON}" "${SOUND_TEST_ROOT}/gen_test_wav.py" ${_args}
    DEPENDS "${SOUND_TEST_ROOT}/gen_test_wav.py"
    VERBATIM)
  set(${out_name}_PATH "${_out}" PARENT_SCOPE)
endfunction()

foreach(_tone IN ITEMS 440 2000)
  foreach(_rate IN ITEMS 22050 32000 44000 44100 48000)
    foreach(_channels IN ITEMS 1 2)
      foreach(_length IN ITEMS quarter double)
        if(_length STREQUAL "quarter")
          math(EXPR _frames "${_rate} / 4")
        else()
          math(EXPR _frames "${_rate} * 2")
        endif()
        if(_channels EQUAL 1)
          set(_prefix mono)
        else()
          set(_prefix stereo)
        endif()
        set(_name "${_prefix}_tone_${_tone}_${_rate}_${_frames}.wav")
        sound_generate_wav("${_name}" "${_tone}" "${_rate}" "${_frames}" "${_channels}" OFF OFF)
        list(APPEND SOUND_TEST_WAVS "${_name}")
      endforeach()
    endforeach()
  endforeach()
endforeach()

sound_generate_wav("mono_toneramp_440_32000_64000.wav" 440 32000 64000 1 ON OFF)
list(APPEND SOUND_TEST_WAVS "mono_toneramp_440_32000_64000.wav")

sound_generate_wav("mono_dc_44100_88200.wav" "" 44100 88200 1 OFF ON)
list(APPEND SOUND_TEST_WAVS "mono_dc_44100_88200.wav")

sound_generate_wav("stereo_tone_440_96000_24000.wav" 440 96000 24000 2 OFF OFF)
list(APPEND SOUND_TEST_WAVS "stereo_tone_440_96000_24000.wav")

set(SOUND_TEST_WAV_EMBEDS)
foreach(_wav IN LISTS SOUND_TEST_WAVS)
  if(DEFINED ${_wav}_PATH)
    set(_input "${${_wav}_PATH}")
  else()
    set(_input "${SOUND_TEST_ROOT}/${_wav}")
  endif()
  set(_out_h "${SOUND_TEST_GEN_DIR}/test/${_wav}.embed.h")
  set(_out_cpp "${SOUND_TEST_GEN_DIR}/test/${_wav}.embed.cpp")
  defold_embed_to_header(INPUT "${_input}" OUTPUT_HEADER "${_out_h}" OUTPUT_CPP "${_out_cpp}")
  list(APPEND SOUND_TEST_WAV_EMBEDS "${_out_cpp}")
endforeach()
defold_add_library(embedded_wavs STATIC ${SOUND_TEST_WAV_EMBEDS})

set(SOUND_TEST_OGG_EMBEDS)
file(GLOB SOUND_TEST_OGGS CONFIGURE_DEPENDS "${SOUND_TEST_ROOT}/*.ogg")
foreach(_ogg IN LISTS SOUND_TEST_OGGS)
  get_filename_component(_name "${_ogg}" NAME)
  set(_out_h "${SOUND_TEST_GEN_DIR}/test/${_name}.embed.h")
  set(_out_cpp "${SOUND_TEST_GEN_DIR}/test/${_name}.embed.cpp")
  defold_embed_to_header(INPUT "${_ogg}" OUTPUT_HEADER "${_out_h}" OUTPUT_CPP "${_out_cpp}")
  list(APPEND SOUND_TEST_OGG_EMBEDS "${_out_cpp}")
endforeach()
defold_add_library(embedded_oggs STATIC ${SOUND_TEST_OGG_EMBEDS})

set(SOUND_TEST_OPUS_EMBEDS)
file(GLOB SOUND_TEST_OPUS CONFIGURE_DEPENDS "${SOUND_TEST_ROOT}/*.opus")
foreach(_opus IN LISTS SOUND_TEST_OPUS)
  get_filename_component(_name "${_opus}" NAME)
  set(_out_h "${SOUND_TEST_GEN_DIR}/test/${_name}.embed.h")
  set(_out_cpp "${SOUND_TEST_GEN_DIR}/test/${_name}.embed.cpp")
  defold_embed_to_header(INPUT "${_opus}" OUTPUT_HEADER "${_out_h}" OUTPUT_CPP "${_out_cpp}")
  list(APPEND SOUND_TEST_OPUS_EMBEDS "${_out_cpp}")
endforeach()
defold_add_library(embedded_opus STATIC ${SOUND_TEST_OPUS_EMBEDS})

set(SOUND_TEST_LIBS
  testmain
  dlib
  profile_null
  sound
  embedded_wavs
  embedded_oggs
  embedded_opus
  opus
  decoder_wav
  decoder_ogg
  decoder_opus)

set(SOUND_TEST_EXPORTS DefaultSoundDevice AudioDecoderWav AudioDecoderStbVorbis AudioDecoderOpus)
if(NOT TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web|win32|x86_64-win32|arm64-nx64|x86_64-ps4|x86_64-ps5|x86_64-xbone)$")
  list(APPEND SOUND_TEST_EXPORTS AudioDecoderTremolo)
  list(APPEND SOUND_TEST_LIBS tremolo)
  set(SOUND_TEST_HAS_TREMOLO ON)
endif()
function(sound_configure_test target run_test)
  target_include_directories(${target} PRIVATE
    "${SOUND_SRC_DIR}"
    "${SOUND_TEST_ROOT}"
    "${SOUND_TEST_GEN_DIR}")
  defold_target_link_libraries(${target} "${TARGET_PLATFORM}" ${SOUND_TEST_LIBS})
  defold_target_link_graphics(${target} "${TARGET_PLATFORM}" SCOPE PRIVATE)
  defold_target_link_socket(${target} "${TARGET_PLATFORM}" PRIVATE)
  defold_target_link_libraries_web(${target} "${TARGET_PLATFORM}" library_sound.js library_sys.js)
  if(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
    find_package(Threads REQUIRED)
    target_link_libraries(${target} PRIVATE Threads::Threads dl GL GLU)
  elseif(TARGET_PLATFORM MATCHES "^(x86_64-macos|arm64-macos)$")
    target_link_libraries(${target} PRIVATE "-framework OpenGL")
  elseif(TARGET_PLATFORM MATCHES "win32$")
    target_link_libraries(${target} PRIVATE opengl32.lib glu32.lib ole32.lib)
  endif()
  defold_register_test_target(${target} ${run_test} "${SOUND_ROOT}")
endfunction()

if(SOUND_TEST_HAS_TREMOLO)
  set(_sound_perf_export_file "${SOUND_TEST_GEN_DIR}/test_sound_perf_exported_symbols.cpp")
  defold_create_exported_symbols_file("${_sound_perf_export_file}" "${SOUND_TEST_EXPORTS}")
  defold_add_executable(test_sound_perf "${SOUND_TEST_ROOT}/test_sound_perf.cpp" "${_sound_perf_export_file}")
  sound_configure_test(test_sound_perf OFF)
endif()

set(_sound_export_file "${SOUND_TEST_GEN_DIR}/test_sound_exported_symbols.cpp")
set(_sound_exports ${SOUND_TEST_EXPORTS} LoopBackDevice)
defold_create_exported_symbols_file("${_sound_export_file}" "${_sound_exports}")
defold_add_executable(test_sound "${SOUND_TEST_ROOT}/test_sound.cpp" "${_sound_export_file}")
if(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
  target_compile_options(test_sound PRIVATE -msimd128 -msse4.2)
  target_compile_definitions(test_sound PRIVATE DM_SOUND_DSP_IMPL=WASM)
endif()
sound_configure_test(test_sound OFF)

set(_sound_null_export_file "${SOUND_TEST_GEN_DIR}/test_sound_null_exported_symbols.cpp")
set(_sound_null_exports NullSoundDevice TestNullDevice)
defold_create_exported_symbols_file("${_sound_null_export_file}" "${_sound_null_exports}")
defold_add_executable(test_sound_null "${SOUND_TEST_ROOT}/test_sound_null.cpp" "${_sound_null_export_file}")
target_include_directories(test_sound_null PRIVATE
  "${SOUND_SRC_DIR}"
  "${SOUND_TEST_ROOT}"
  "${SOUND_TEST_GEN_DIR}")
defold_target_link_libraries(test_sound_null "${TARGET_PLATFORM}"
  testmain
  dlib
  profile_null
  sound_null
  embedded_wavs
  embedded_oggs
  embedded_opus)
defold_target_link_socket(test_sound_null "${TARGET_PLATFORM}" PRIVATE)
defold_target_link_libraries_web(test_sound_null "${TARGET_PLATFORM}" library_sound.js)
defold_register_test_target(test_sound_null ON "${SOUND_ROOT}")

if(TARGET_PLATFORM MATCHES "linux$")
  set(_sound_openal_export_file "${SOUND_TEST_GEN_DIR}/test_sound_openal_exported_symbols.cpp")
  defold_create_exported_symbols_file("${_sound_openal_export_file}" "${SOUND_TEST_EXPORTS}")
  defold_add_executable(test_sound_openal "${SOUND_TEST_ROOT}/test_sound.cpp" "${_sound_openal_export_file}")
  target_compile_definitions(test_sound_openal PRIVATE DM_TEST_SOUND_USE_OPENAL)
  target_link_libraries(test_sound_openal PRIVATE sound_openal)
  sound_configure_test(test_sound_openal OFF)
endif()
