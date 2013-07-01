/* API declaration export attribute */
#define AL_API  __attribute__((visibility("default")))
#define ALC_API __attribute__((visibility("default")))

/* Define to the library version */
#define ALSOFT_VERSION "1.15.1"

/* Define any available alignment declaration */
#define ALIGN(x) __attribute__((aligned(x)))
#ifdef __MINGW32__
#define align(x) aligned(x)
#endif

/* Define to the appropriate 'restrict' keyword */
#define restrict __restrict__

/* Define if we have the C11 aligned_alloc function */
/* #undef HAVE_ALIGNED_ALLOC */

/* Define if we have the posix_memalign function */
/* #define HAVE_POSIX_MEMALIGN */

/* Define if we have the _aligned_malloc function */
/* #undef HAVE__ALIGNED_MALLOC */

/* Define if we have SSE CPU extensions */
/* #define HAVE_SSE */

/* Define if we have ARM Neon CPU extensions */
/* #define HAVE_NEON */

/* Define if we have the ALSA backend */
/* #undef HAVE_ALSA */

/* Define if we have the OSS backend */
/* #undef HAVE_OSS */

/* Define if we have the Solaris backend */
/* #undef HAVE_SOLARIS */

/* Define if we have the SndIO backend */
/* #undef HAVE_SNDIO */

/* Define if we have the MMDevApi backend */
/* #undef HAVE_MMDEVAPI */

/* Define if we have the DSound backend */
/* #undef HAVE_DSOUND */

/* Define if we have the Windows Multimedia backend */
/* #undef HAVE_WINMM */

/* Define if we have the PortAudio backend */
/* #undef HAVE_PORTAUDIO */

/* Define if we have the PulseAudio backend */
/* #undef HAVE_PULSEAUDIO */

/* Define if we have the CoreAudio backend */
/* #define HAVE_COREAUDIO */

/* Define if we have the OpenSL backend */
#define HAVE_OPENSL

/* Define if we have the Wave Writer backend */
#define HAVE_WAVE

/* Define if we have the stat function */
#define HAVE_STAT

/* Define if we have the lrintf function */
#define HAVE_LRINTF

/* Define if we have the strtof function */
#define HAVE_STRTOF

/* Define if we have the __int64 type */
/* #undef HAVE___INT64 */

/* Define to the size of a long int type */
#define SIZEOF_LONG 4

/* Define to the size of a long long int type */
#define SIZEOF_LONG_LONG 8

/* Define if we have GCC's destructor attribute */
#define HAVE_GCC_DESTRUCTOR

/* Define if we have GCC's format attribute */
#define HAVE_GCC_FORMAT

/* Define if we have stdint.h */
#define HAVE_STDINT_H

/* Define if we have windows.h */
/* #undef HAVE_WINDOWS_H */

/* Define if we have dlfcn.h */
#define HAVE_DLFCN_H

/* Define if we have pthread_np.h */
/* #undef HAVE_PTHREAD_NP_H */

/* Define if we have xmmintrin.h */
/* #undef HAVE_XMMINTRIN_H */

/* Define if we have arm_neon.h */
/* #define HAVE_ARM_NEON_H */

/* Define if we have malloc.h */
/* #undef HAVE_MALLOC_H */

/* Define if we have cpuid.h */
/* #undef HAVE_CPUID_H */

/* Define if we have guiddef.h */
/* #undef HAVE_GUIDDEF_H */

/* Define if we have initguid.h */
/* #undef HAVE_INITGUID_H */

/* Define if we have ieeefp.h */
/* #undef HAVE_IEEEFP_H */

/* Define if we have float.h */
#define HAVE_FLOAT_H

/* Define if we have fenv.h */
#define HAVE_FENV_H

/* Define if we have _controlfp() */
/* #undef HAVE__CONTROLFP */

/* Define if we have __control87_2() */
/* #undef HAVE___CONTROL87_2 */

/* Define if we have pthread_setschedparam() */
#define HAVE_PTHREAD_SETSCHEDPARAM
