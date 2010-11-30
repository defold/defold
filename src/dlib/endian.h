#ifndef DM_ENDIAN
#define DM_ENDIAN

/// Endian. Defined to #DM_ENDIAN_LITTLE or #DM_ENDIAN_BIG
#define DM_ENDIAN
#undef DM_ENDIAN

#define DM_ENDIAN_LITTLE 0
#define DM_ENDIAN_BIG 1

#if defined(__x86_64) or defined(__x86_64__) or defined(_X86_) or defined(__i386__) or defined(_M_IX86) or defined(__LITTLE_ENDIAN__)
#define DM_ENDIAN DM_ENDIAN_LITTLE
#else
#error "Unknown endian"
#endif

#endif
