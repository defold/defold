# Dependencies normally distributed as binary packages must also be rebuilt
# for MSan. Keep the same revisions and GLFW patch as the packaged versions.
include(FetchContent)
find_program(MSAN_PATCH patch REQUIRED)

set(ASTCENC_CLI OFF CACHE BOOL "" FORCE)
set(ASTCENC_ISA_NATIVE OFF CACHE BOOL "" FORCE)
if(TARGET_PLATFORM STREQUAL "arm64-linux")
  set(ASTCENC_ISA_NEON ON CACHE BOOL "" FORCE)
  set(_astc_target astcenc-neon-static)
else()
  set(ASTCENC_ISA_SSE41 ON CACHE BOOL "" FORCE)
  set(_astc_target astcenc-sse4.1-static)
endif()
FetchContent_Declare(msan_astc
  URL https://codeload.github.com/ARM-software/astc-encoder/tar.gz/30aabb3f42406df45a910d8496f9bee17eeba9bb
  URL_HASH SHA256=4ae89d0d16435c761a65de3e96642a46b9a2e459152c72bcf1edcaf70ef7c2e8
  PATCH_COMMAND "${MSAN_PATCH}" -p1 -i "${DEFOLD_HOME}/share/ext/astcenc/patch_initialize_weights"
  EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(msan_astc)
set_target_properties(${_astc_target} PROPERTIES OUTPUT_NAME astcenc)
install(TARGETS ${_astc_target} ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")

set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "" FORCE)
# 97709575e2332548603d308890423126100608ae
FetchContent_Declare(msan_spirv
  URL https://codeload.github.com/KhronosGroup/SPIRV-Cross/tar.gz/97709575
  URL_HASH SHA256=c7b77f95b27e2132f366fab661a6fd3a07ab526e565394268f9912d1be065a9a
  EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(msan_spirv)
foreach(_spirv_target IN ITEMS spirv-cross-c spirv-cross-core spirv-cross-glsl
    spirv-cross-hlsl spirv-cross-msl spirv-cross-reflect)
  # The C API catches the C++ library's exceptions and turns them into errors.
  # Source options follow the engine SDK's default -fno-exceptions.
  get_target_property(_spirv_sources ${_spirv_target} SOURCES)
  set_property(SOURCE ${_spirv_sources} TARGET_DIRECTORY ${_spirv_target}
    APPEND PROPERTY COMPILE_OPTIONS -fexceptions)
endforeach()
install(TARGETS spirv-cross-c spirv-cross-core spirv-cross-glsl
  spirv-cross-hlsl spirv-cross-msl spirv-cross-reflect
  ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
FetchContent_Declare(msan_glfw
  URL https://codeload.github.com/glfw/glfw/tar.gz/3.4
  URL_HASH SHA256=c038d34200234d071fae9345bc455e4a8f2f544ab60150765d7704e08f3dac01
  PATCH_COMMAND "${MSAN_PATCH}" -p1 -i "${DEFOLD_HOME}/share/ext/glfw/patch_3.4"
  EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(msan_glfw)
install(TARGETS glfw ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")

FetchContent_Declare(msan_tremolo
  URL https://codeload.github.com/defold/defold-tremolo/tar.gz/b0cb4d1
  URL_HASH SHA256=cfa310d701dd2411e131d042026e10b02fc0a4b4df59a369d2df4d8cc9dcf662)
FetchContent_MakeAvailable(msan_tremolo)
set(_tremolo_sources bitwise.c codebook.c dsp.c floor0.c floor1.c floor_lookup.c
  framing.c mapping0.c mdct.c misc.c res012.c treminfo.c vorbisfile.c)
list(TRANSFORM _tremolo_sources PREPEND "${msan_tremolo_SOURCE_DIR}/Tremolo/")
add_library(tremolo STATIC ${_tremolo_sources})
target_compile_definitions(tremolo PRIVATE ONLY_C _GNU_SOURCE)
target_compile_options(tremolo PRIVATE -funsigned-char)
target_link_libraries(tremolo PRIVATE defold_sdk)
install(TARGETS tremolo ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")

# FetchContent subprojects are excluded from the default build, but these
# archives must be ready before the external install step.
add_custom_target(msan_packages ALL DEPENDS ${_astc_target} glfw
  spirv-cross-c spirv-cross-core spirv-cross-glsl spirv-cross-hlsl
  spirv-cross-msl spirv-cross-reflect)
