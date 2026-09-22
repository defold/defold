cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(basisu LANGUAGES ${DEFOLD_LANGUAGE_LIST})

set(CMAKE_INSTALL_PREFIX "${DEFOLD_SDK_ROOT}/ext" CACHE PATH "External library install prefix" FORCE)
set(BASISU_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/package/basis_universal-2.50")

# Patch a build copy so the vendored upstream sources remain unchanged.
find_package(Git REQUIRED)
set(BASISU_RUNTIME_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/runtime")
set(BASISU_RUNTIME_SOURCE "${BASISU_RUNTIME_SOURCE_DIR}/transcoder/basisu_transcoder.cpp")
set(BASISU_RUNTIME_HEADER "${BASISU_RUNTIME_SOURCE_DIR}/transcoder/basisu_astc_helpers.h")
file(GLOB BASISU_RUNTIME_INPUTS CONFIGURE_DEPENDS "${BASISU_SOURCE_DIR}/transcoder/*")
add_custom_command(OUTPUT "${BASISU_RUNTIME_SOURCE}" "${BASISU_RUNTIME_HEADER}"
  # Copy the headers together so quoted includes resolve to the patched copy.
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
    "${BASISU_SOURCE_DIR}/transcoder" "${BASISU_RUNTIME_SOURCE_DIR}/transcoder"
  COMMAND "${GIT_EXECUTABLE}" apply --unsafe-paths
    "--directory=${BASISU_RUNTIME_SOURCE_DIR}" "${CMAKE_CURRENT_LIST_DIR}/defold.patch"
  DEPENDS ${BASISU_RUNTIME_INPUTS} "${CMAKE_CURRENT_LIST_DIR}/defold.patch"
  WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../.."
  COMMENT "Applying Basis Universal runtime patches"
  VERBATIM)

# Match the previous runtime's UASTC LDR and restricted ETC1S support.
add_library(basis_transcoder STATIC "${BASISU_RUNTIME_SOURCE}")
target_include_directories(basis_transcoder PRIVATE "${BASISU_RUNTIME_SOURCE_DIR}/transcoder")
target_compile_definitions(basis_transcoder PRIVATE
  BASISD_SUPPORT_UASTC=1 BASISD_SUPPORT_XUASTC=0 BASISD_SUPPORT_UASTC_HDR=0
  BASISD_SUPPORT_KTX2=0 BASISD_SUPPORT_KTX2_ZSTD=0
  # These switches restrict ETC1S conversions; UASTC has its own GPU paths.
  BASISD_SUPPORT_DXT1=0 BASISD_SUPPORT_DXT5A=0
  BASISD_SUPPORT_ATC=0 BASISD_SUPPORT_ASTC=0
  BASISD_SUPPORT_ETC2_EAC_RG11=0 BASISD_SUPPORT_ETC2_EAC_A8=0
  BASISD_SUPPORT_PVRTC1=0 BASISD_SUPPORT_PVRTC2=0
  BASISD_SUPPORT_BC7_MODE5=0 BASISD_SUPPORT_FXT1=0)
if(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
  # Source options follow the inherited platform -O3/-O0 options.
  set_property(SOURCE "${BASISU_RUNTIME_SOURCE}" APPEND PROPERTY COMPILE_OPTIONS
    "$<$<NOT:$<CONFIG:Debug>>:-Oz>")
endif()
set(BASISU_TARGETS basis_transcoder)
set(BASISU_INSTALL_TARGETS basis_transcoder)

if(TARGET_PLATFORM MATCHES "^(x86_64-linux|arm64-linux|x86_64-win32|x86_64-macos|arm64-macos)$")
  # Matches the encoder sources in the pinned upstream CMakeLists.txt, without
  # the command line tool, WASM bindings, or optional external astcenc backend.
  set(BASISU_ENCODER_SOURCES
    basisu_backend.cpp
    basisu_basis_file.cpp
    basisu_comp.cpp
    basisu_enc.cpp
    basisu_etc.cpp
    basisu_frontend.cpp
    basisu_gpu_texture.cpp
    basisu_pvrtc1_4.cpp
    basisu_resampler.cpp
    basisu_resample_filters.cpp
    basisu_ssim.cpp
    basisu_uastc_enc.cpp
    basisu_bc7e_scalar.cpp
    basisu_dds_export.cpp
    basisu_bc7enc.cpp
    jpgd.cpp
    basisu_kernels_sse.cpp
    basisu_bc15_spmd.cpp
    basisu_bc15_spmd_sse.cpp
    basisu_opencl.cpp
    pvpngreader.cpp
    basisu_uastc_hdr_4x4_enc.cpp
    basisu_astc_hdr_6x6_enc.cpp
    basisu_astc_hdr_common.cpp
    basisu_astc_ldr_common.cpp
    basisu_astc_ldr_encode.cpp
    basisu_astc_ldr_fencode.cpp
    basisu_xbc7_encode.cpp
    basisu_tinyexr.cpp
    3rdparty/android_astc_decomp.cpp)
  list(TRANSFORM BASISU_ENCODER_SOURCES PREPEND "${BASISU_SOURCE_DIR}/encoder/")
  # One complete, uninstrumented library for offline tools and editor JNI.
  add_library(basis_encoder STATIC ${BASISU_ENCODER_SOURCES}
    "${BASISU_SOURCE_DIR}/transcoder/basisu_transcoder.cpp"
    "${BASISU_SOURCE_DIR}/zstd/zstd.c")
  target_compile_definitions(basis_encoder PRIVATE
    BASISD_SUPPORT_KTX2=1 BASISD_SUPPORT_KTX2_ZSTD=1
    BASISU_SUPPORT_OPENCL=0 BASISU_SUPPORT_ASTCENC=0 MINIZ_NO_STDIO)

  if(TARGET_PLATFORM MATCHES "^(x86_64-win32|x86_64-macos)$")
    target_compile_definitions(basis_encoder PRIVATE BASISU_SUPPORT_SSE=1)
    if(NOT MSVC)
      target_compile_options(basis_encoder PRIVATE -msse4.1)
    endif()
  else()
    target_compile_definitions(basis_encoder PRIVATE BASISU_SUPPORT_SSE=0)
  endif()

  list(APPEND BASISU_TARGETS basis_encoder)
  list(APPEND BASISU_INSTALL_TARGETS basis_encoder)
endif()

foreach(_target IN LISTS BASISU_TARGETS)
  target_compile_features(${_target} PRIVATE cxx_std_17)
  target_include_directories(${_target} PRIVATE "${BASISU_SOURCE_DIR}")
  # Keep Defold's CRT/STL settings in Debug too: defining _DEBUG here would
  # select MSVC's debug STL while consumers still use the release CRT (/MT).
  target_compile_definitions(${_target} PRIVATE
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>)
  set_target_properties(${_target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  if(NOT MSVC)
    target_compile_options(${_target} PRIVATE -fno-strict-aliasing)
  endif()
endforeach()

defold_install_targets(TARGETS ${BASISU_INSTALL_TARGETS}
  ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")

# Retire dlib's generated archives after installing their replacements. Otherwise
# Waf and SDK consumers can find the old version first in SDK/lib.
foreach(_target IN ITEMS basis_transcoder basis_encoder basis_encoder_noasan)
  install(CODE "file(REMOVE \"\$ENV{DESTDIR}${DEFOLD_SDK_ROOT}/lib/${TARGET_PLATFORM}/${CMAKE_STATIC_LIBRARY_PREFIX}${_target}${CMAKE_STATIC_LIBRARY_SUFFIX}\")")
  if(TARGET_PLATFORM MATCHES "win32$")
    install(CODE "file(REMOVE \"\$ENV{DESTDIR}${DEFOLD_SDK_ROOT}/lib/${TARGET_PLATFORM}/lib${_target}.lib\")")
  endif()
endforeach()

# The uninstrumented encoder is shared by tools and editor JNI.
install(CODE "file(REMOVE \"\$ENV{DESTDIR}${DEFOLD_SDK_ROOT}/ext/lib/${TARGET_PLATFORM}/${CMAKE_STATIC_LIBRARY_PREFIX}basis_encoder_noasan${CMAKE_STATIC_LIBRARY_SUFFIX}\")")

install(DIRECTORY "${BASISU_SOURCE_DIR}/encoder"
                  "${BASISU_RUNTIME_SOURCE_DIR}/transcoder"
                  "${BASISU_SOURCE_DIR}/zstd"
  DESTINATION include/basis
  FILES_MATCHING PATTERN "*.h" PATTERN "*.inl" PATTERN "*.inc" PATTERN "*.hh")
install(FILES "${BASISU_SOURCE_DIR}/LICENSE" "${BASISU_SOURCE_DIR}/NOTICE"
  DESTINATION share/licenses/basisu)
install(DIRECTORY "${BASISU_SOURCE_DIR}/LICENSES/" DESTINATION share/licenses/basisu)
