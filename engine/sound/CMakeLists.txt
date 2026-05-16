cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_sound LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="SOUND")

set(SOUND_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(SOUND_SRC_DIR "${SOUND_ROOT}/src")

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

set(SOUND_SOURCES ${SOUND_BASE_SOURCES} "${SOUND_SRC_DIR}/sound.cpp")
if(TARGET_PLATFORM MATCHES "android")
  list(APPEND SOUND_SOURCES "${SOUND_SRC_DIR}/devices/device_opensl.cpp")
elseif(TARGET_PLATFORM MATCHES "web$")
  list(APPEND SOUND_SOURCES "${SOUND_SRC_DIR}/devices/device_js.cpp")
elseif(TARGET_PLATFORM MATCHES "nx64")
  list(APPEND SOUND_SOURCES "${SOUND_SRC_DIR}/devices/device_nx64.cpp")
elseif(TARGET_PLATFORM MATCHES "^(x86_64-ps4|x86_64-ps5)$")
  list(APPEND SOUND_SOURCES "${SOUND_SRC_DIR}/devices/device_ps4.cpp")
elseif(TARGET_PLATFORM MATCHES "^(x86_64-win32|win32|x86_64-xbone)$")
  list(APPEND SOUND_SOURCES "${SOUND_SRC_DIR}/devices/device_wasapi.cpp")
elseif(TARGET_PLATFORM MATCHES "^(x86_64-macos|arm64-macos|x86_64-ios|arm64-ios)$")
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
  defold_target_link_libraries(${target} "${TARGET_PLATFORM}" SCOPE PUBLIC dlib)
  if(TARGET_PLATFORM MATCHES "^(x86_64-win32|win32)$")
    target_compile_definitions(${target} PRIVATE _XBOX)
    target_link_libraries(${target} PUBLIC ole32.lib)
  elseif(TARGET_PLATFORM MATCHES "^(x86_64-macos|arm64-macos|x86_64-ios|arm64-ios)$")
    target_link_libraries(${target} PUBLIC "-framework AVFoundation")
  elseif(TARGET_PLATFORM MATCHES "linux$")
    target_link_libraries(${target} PUBLIC openal)
  elseif(TARGET_PLATFORM MATCHES "android")
    target_link_libraries(${target} PUBLIC OpenSLES)
  endif()
endfunction()

function(sound_apply_simd_flags target fallback)
  if(fallback)
    target_compile_definitions(${target} PRIVATE DM_SOUND_DSP_IMPL=Fallback)
  elseif(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
    target_compile_options(${target} PRIVATE -msimd128 -msse4.2)
    target_compile_definitions(${target} PRIVATE DM_SOUND_DSP_IMPL=WASM)
  elseif(TARGET_PLATFORM STREQUAL "x86_64-linux")
    target_compile_options(${target} PRIVATE -msse4.1)
    target_compile_definitions(${target} PRIVATE DM_SOUND_DSP_IMPL=SSE)
  elseif(TARGET_PLATFORM STREQUAL "x86_64-macos")
    target_compile_definitions(${target} PRIVATE DM_SOUND_DSP_IMPL=SSE)
  elseif(TARGET_PLATFORM MATCHES "^(x86_64-win32|win32)$")
    if(TARGET_PLATFORM STREQUAL "x86_64-win32")
      target_compile_options(${target} PRIVATE /arch:SSE4.2)
    endif()
    target_compile_definitions(${target} PRIVATE DM_SOUND_DSP_IMPL=SSE)
  else()
    target_compile_definitions(${target} PRIVATE DM_SOUND_DSP_IMPL=Fallback)
  endif()
endfunction()

defold_add_library(decoder_wav STATIC "${SOUND_SRC_DIR}/decoders/decoder_wav.cpp")
target_include_directories(decoder_wav PUBLIC "${SOUND_SRC_DIR}")

defold_add_library(decoder_ogg STATIC ${SOUND_DECODER_OGG_SOURCES})
target_include_directories(decoder_ogg PUBLIC "${SOUND_SRC_DIR}")
if(NOT TARGET_PLATFORM MATCHES "^(arm64-nx64|x86_64-ps4|x86_64-ps5|x86_64-xbone)$")
  defold_target_link_libraries(decoder_ogg "${TARGET_PLATFORM}" SCOPE PUBLIC tremolo)
endif()

defold_add_library(decoder_opus STATIC "${SOUND_SRC_DIR}/decoders/decoder_opus.cpp")
target_include_directories(decoder_opus PUBLIC "${SOUND_SRC_DIR}")
defold_target_link_libraries(decoder_opus "${TARGET_PLATFORM}" SCOPE PUBLIC opus)

defold_add_library(sound STATIC ${SOUND_SOURCES})
sound_configure_target(sound)
sound_apply_simd_flags(sound OFF)

defold_add_library(sound_openal STATIC ${SOUND_OPENAL_SOURCES})
sound_configure_target(sound_openal)

defold_add_library(sound_null STATIC
  "${SOUND_SRC_DIR}/devices/device_null.cpp"
  "${SOUND_SRC_DIR}/sound_null.cpp")
sound_configure_target(sound_null)

set(SOUND_INSTALL_TARGETS decoder_wav decoder_ogg decoder_opus sound sound_openal sound_null)
if(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
  defold_add_library(sound_nosimd STATIC ${SOUND_SOURCES})
  sound_configure_target(sound_nosimd)
  sound_apply_simd_flags(sound_nosimd ON)
  list(APPEND SOUND_INSTALL_TARGETS sound_nosimd)
endif()

if(TARGET_PLATFORM MATCHES "web$")
  install(FILES "${SOUND_SRC_DIR}/js/library_sound.js" DESTINATION "lib/${TARGET_PLATFORM}/js")
elseif(TARGET_PLATFORM MATCHES "^arm.*android$")
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

install(TARGETS ${SOUND_INSTALL_TARGETS}
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})
install(FILES "${SOUND_SRC_DIR}/sound.h" DESTINATION include/sound)

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
