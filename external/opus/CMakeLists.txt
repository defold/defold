cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(opus_external VERSION 1.5.2 LANGUAGES C)

# Keep the decoder-only source selection used by the engine.
add_library(opus STATIC
  src/opus.c
  src/opus_decoder.c
  celt/bands.c
  celt/celt.c
  celt/celt_decoder.c
  celt/cwrs.c
  celt/entenc.c
  celt/entdec.c
  celt/entcode.c
  celt/kiss_fft.c
  celt/laplace.c
  celt/mathops.c
  celt/mdct.c
  celt/modes.c
  celt/pitch.c
  celt/celt_lpc.c
  celt/quant_bands.c
  celt/rate.c
  celt/vq.c
  silk/CNG.c
  silk/code_signs.c
  silk/init_decoder.c
  silk/decode_core.c
  silk/decode_frame.c
  silk/decode_parameters.c
  silk/decode_indices.c
  silk/decode_pulses.c
  silk/decoder_set_fs.c
  silk/dec_API.c
  silk/gain_quant.c
  silk/interpolate.c
  silk/LP_variable_cutoff.c
  silk/NLSF_decode.c
  silk/PLC.c
  silk/shell_coder.c
  silk/tables_gain.c
  silk/tables_LTP.c
  silk/tables_NLSF_CB_NB_MB.c
  silk/tables_NLSF_CB_WB.c
  silk/tables_other.c
  silk/tables_pitch_lag.c
  silk/tables_pulses_per_block.c
  silk/quant_LTP_gains.c
  silk/HP_variable_cutoff.c
  silk/NLSF_encode.c
  silk/NLSF_VQ.c
  silk/NLSF_unpack.c
  silk/NLSF_del_dec_quant.c
  silk/process_NLSFs.c
  silk/stereo_LR_to_MS.c
  silk/stereo_MS_to_LR.c
  silk/A2NLSF.c
  silk/ana_filt_bank_1.c
  silk/biquad_alt.c
  silk/bwexpander_32.c
  silk/bwexpander.c
  silk/decode_pitch.c
  silk/inner_prod_aligned.c
  silk/lin2log.c
  silk/log2lin.c
  silk/LPC_analysis_filter.c
  silk/LPC_inv_pred_gain.c
  silk/table_LSF_cos.c
  silk/NLSF2A.c
  silk/NLSF_stabilize.c
  silk/pitch_est_tables.c
  silk/resampler.c
  silk/resampler_down2_3.c
  silk/resampler_down2.c
  silk/resampler_private_AR2.c
  silk/resampler_private_down_FIR.c
  silk/resampler_private_IIR_FIR.c
  silk/resampler_private_up2_HQ.c
  silk/resampler_rom.c
  silk/sort.c
  silk/sum_sqr_shift.c
  silk/stereo_decode_pred.c
  silk/LPC_fit.c
  silk/NSQ.c
  silk/NSQ_del_dec.c
  silk/VAD.c
  silk/VQ_WMat_EC.c)

target_include_directories(opus PRIVATE include celt silk silk/float silk/fixed .)
target_compile_definitions(opus PRIVATE NDEBUG OPUS_BUILD USE_ALLOCA)
target_link_libraries(opus PRIVATE defold_sdk)
set_target_properties(opus PROPERTIES PREFIX "lib")

if(TARGET_PLATFORM MATCHES "^x86_64-(win32|linux|macos)$")
  target_sources(opus PRIVATE
    celt/x86/celt_lpc_sse4_1.c
    celt/x86/pitch_avx.c
    celt/x86/pitch_sse2.c
    celt/x86/pitch_sse4_1.c
    celt/x86/pitch_sse.c
    celt/x86/vq_sse2.c
    celt/x86/x86_celt_map.c
    celt/x86/x86cpu.c
    silk/x86/x86_silk_map.c
    silk/x86/NSQ_del_dec_sse4_1.c
    silk/x86/NSQ_sse4_1.c
    silk/x86/VAD_sse4_1.c
    silk/x86/VQ_WMat_EC_sse4_1.c)
  target_compile_definitions(opus PRIVATE
    OPUS_HAVE_RTCD OPUS_X86_MAY_HAVE_SSE OPUS_X86_MAY_HAVE_SSE2
    OPUS_X86_MAY_HAVE_SSE4_1 CPU_INFO_BY_C)

  # Only the dispatched implementations may use instructions beyond SSE2.
  if(NOT MSVC)
    set_source_files_properties(
      celt/x86/celt_lpc_sse4_1.c celt/x86/pitch_sse4_1.c
      silk/x86/NSQ_del_dec_sse4_1.c silk/x86/NSQ_sse4_1.c
      silk/x86/VAD_sse4_1.c silk/x86/VQ_WMat_EC_sse4_1.c
      PROPERTIES COMPILE_OPTIONS -msse4.1)
  endif()
endif()

if(DEFOLD_EXTERNAL_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${DEFOLD_EXTERNAL_INSTALL_PREFIX}" CACHE PATH "External package install prefix" FORCE)
endif()

install(TARGETS opus ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")
install(DIRECTORY include/ DESTINATION include/opus FILES_MATCHING PATTERN "*.h")
