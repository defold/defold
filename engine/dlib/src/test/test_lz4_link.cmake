# Verify the decode-only shared library retains LZ4 decoding and discards
# unused compression/tooling code, regardless of archive export annotations.
execute_process(
  COMMAND "${NM}" --defined-only --format=posix "${LIBRARY}"
  RESULT_VARIABLE _result
  OUTPUT_VARIABLE _symbols
  ERROR_VARIABLE _error)
if(NOT _result EQUAL 0)
  message(FATAL_ERROR "Cannot inspect LZ4 link probe: ${_error}")
endif()
if(NOT _symbols MATCHES "(^|\n)LZ4_decompress_safe[ \t]")
  message(FATAL_ERROR "LZ4 decompression was not retained")
endif()
if(_symbols MATCHES "(^|\n)(LZ4_compress[^ \t]*|LZ4HC_[^ \t]*|LZ4CompressBuffer|LZ4MaxCompressedSize)[ \t]")
  message(FATAL_ERROR "Unused LZ4 compression/tooling code was retained")
endif()

execute_process(
  COMMAND "${NM}" --dynamic --defined-only --format=posix "${LIBRARY}"
  RESULT_VARIABLE _result
  OUTPUT_VARIABLE _exports
  ERROR_VARIABLE _error)
if(NOT _result EQUAL 0)
  message(FATAL_ERROR "Cannot inspect LZ4 link probe exports: ${_error}")
endif()
if(_exports MATCHES "(^|\n)LZ4[^ \t]*[ \t]")
  message(FATAL_ERROR "The runtime probe exports internal LZ4 symbols")
endif()
if(NOT _exports MATCHES "(^|\n)Lz4DecodeOnly[ \t]")
  message(FATAL_ERROR "The decode-only entry point was not exported")
endif()
