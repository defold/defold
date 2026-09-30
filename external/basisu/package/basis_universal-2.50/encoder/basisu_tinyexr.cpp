// File: basisu_tinyexr.cpp
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

// Pull in our local fork of the miniz library. (Binomial wrote the original miniz library. Basisu was tested with this specific version.)
#define MINIZ_HEADER_FILE_ONLY
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "basisu_miniz.h"

// A bit of a hack to force tinyexr to use plain zlib-style compression API's, then we'll direct them to our own customized copy of miniz with #define's.
// This allows us to use tinyexr.h without modifying it at all, or relying on zlib, or pulling in a system-wide miniz dependency. 
// This assumes tinyexr.h doesn't include zlib.h (it doesn't: "Please include your own zlib-compatible API header before...")
// (Time will tell how fragile this is in reality.)
#define TINYEXR_USE_MINIZ (0)

#define Z_OK buminiz::MZ_OK
#define uLong buminiz::mz_ulong
#define uLongf buminiz::mz_ulong

typedef unsigned char Byte;
typedef Byte Bytef;

#define compressBound	buminiz::mz_compressBound
#define compress		buminiz::mz_compress
#define uncompress		buminiz::mz_uncompress

// tinyexr.h is third-party code we use unmodified, so silence its warnings here with SPECIFIC, low-risk
// disables only.
// These affect only this translation unit, which contains nothing but the tinyexr implementation include.
#ifdef _MSC_VER
#pragma warning (disable: 4060) // warning C4060: switch statement contains no 'case' or 'default' labels
#pragma warning (disable: 4100) // warning C4100: unreferenced formal parameter
#pragma warning (disable: 4245) // warning C4245: conversion from 'type1' to 'type2', signed/unsigned mismatch
#pragma warning (disable: 4505) // warning C4505: unreferenced function with internal linkage has been removed
#pragma warning (disable: 4702) // warning C4702: unreachable code
#pragma warning (disable: 4530) // warning C4530: C++ exception handler used, but unwind semantics are not enabled. Specify /EHsc
#endif

#if defined(__GNUC__) // covers both gcc and clang
#pragma GCC diagnostic ignored "-Wunused-parameter" // tinyexr: unused params in its (de)compress helpers (DecompressPxr24, etc.)
#pragma GCC diagnostic ignored "-Wunused-function"  // tinyexr: unused static helpers (CompressPxr24, CompressB44, AddIntAttribute)
#endif

#define TINYEXR_IMPLEMENTATION
#include "3rdparty/tinyexr.h"
