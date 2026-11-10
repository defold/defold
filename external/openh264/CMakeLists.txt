cmake_minimum_required(VERSION 4.0)

set(OPENH264_ROOT "${CMAKE_CURRENT_LIST_DIR}/package/openh264-2.6.0")

# Keep the pinned upstream sources unmodified. Decoder objects are used by the
# recording tests; static linking leaves them out of the engine executable.
add_library(openh264_options INTERFACE)
target_link_libraries(openh264_options INTERFACE defold_sdk)
target_include_directories(openh264_options INTERFACE
  "${OPENH264_ROOT}/codec/api/wels"
  "${OPENH264_ROOT}/codec/common/inc"
  "${OPENH264_ROOT}/codec/processing/interface")
target_compile_definitions(openh264_options INTERFACE NDEBUG)
add_library(openh264 STATIC)
target_link_libraries(openh264 PRIVATE openh264_options)
target_include_directories(openh264 PUBLIC "${OPENH264_ROOT}/codec/api")
set_target_properties(openh264 PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Encoder and decoder contain different headers with the same names. Keep the
# include paths scoped to each upstream component, as in its own build scripts.
foreach(component IN ITEMS common encoder decoder processing)
  set(root "${OPENH264_ROOT}/codec/${component}")
  file(GLOB sources CONFIGURE_DEPENDS "${root}/src/*.cpp"
    "${root}/core/src/*.cpp" "${root}/plus/src/*.cpp" "${root}/src/*/*.cpp")
  list(FILTER sources EXCLUDE REGEX "/DllEntry\\.cpp$")
  add_library(openh264_${component} OBJECT ${sources})
  target_link_libraries(openh264_${component} PRIVATE openh264_options)
  target_include_directories(openh264_${component} PRIVATE
    "${root}/core/inc" "${root}/plus/inc" "${root}/src/common")
  set_target_properties(openh264_${component} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  target_sources(openh264 PRIVATE $<TARGET_OBJECTS:openh264_${component}>)
endforeach()

if(TARGET_PLATFORM STREQUAL "arm64-linux")
  enable_language(ASM)
  file(GLOB OPENH264_ASM CONFIGURE_DEPENDS
    "${OPENH264_ROOT}/codec/common/arm64/*.S"
    "${OPENH264_ROOT}/codec/encoder/core/arm64/*.S"
    "${OPENH264_ROOT}/codec/decoder/core/arm64/*.S"
    "${OPENH264_ROOT}/codec/processing/src/arm64/*.S")
  target_sources(openh264 PRIVATE ${OPENH264_ASM})
  target_include_directories(openh264 PRIVATE "${OPENH264_ROOT}/codec/common/arm64")
  target_compile_definitions(openh264_options INTERFACE HAVE_NEON_AARCH64)
elseif(TARGET_PLATFORM STREQUAL "x86_64-linux")
  find_program(OPENH264_NASM nasm HINTS "${DEFOLD_SDK_ROOT}/ext/bin/${HOST_PLATFORM}")
  if(OPENH264_NASM)
    file(GLOB OPENH264_ASM CONFIGURE_DEPENDS
      "${OPENH264_ROOT}/codec/common/x86/*.asm"
      "${OPENH264_ROOT}/codec/encoder/core/x86/*.asm"
      "${OPENH264_ROOT}/codec/decoder/core/x86/*.asm"
      "${OPENH264_ROOT}/codec/processing/src/x86/*.asm")
    file(GLOB OPENH264_ASM_INCLUDES "${OPENH264_ROOT}/codec/common/x86/*.asm")
    list(FILTER OPENH264_ASM EXCLUDE REGEX "/asm_inc\\.asm$")
    foreach(source IN LISTS OPENH264_ASM)
      file(RELATIVE_PATH relative "${OPENH264_ROOT}" "${source}")
      string(REPLACE "/" "_" object "${relative}")
      set(object "${CMAKE_CURRENT_BINARY_DIR}/${object}.o")
      add_custom_command(OUTPUT "${object}"
        COMMAND "${OPENH264_NASM}" -f elf64 -DUNIX64 -DHAVE_AVX2
          "-I${OPENH264_ROOT}/codec/common/x86/" "${source}" -o "${object}"
        DEPENDS "${source}" ${OPENH264_ASM_INCLUDES} VERBATIM)
      target_sources(openh264 PRIVATE "${object}")
    endforeach()
    target_compile_definitions(openh264_options INTERFACE X86_ASM HAVE_AVX2)
  else()
    message(STATUS "OpenH264: NASM unavailable; using portable C++ kernels")
  endif()
endif()

defold_install_targets(TARGETS openh264 ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")
install(DIRECTORY "${OPENH264_ROOT}/codec/api/wels" DESTINATION include FILES_MATCHING PATTERN "*.h")
install(FILES "${OPENH264_ROOT}/LICENSE" DESTINATION share/licenses/openh264)
