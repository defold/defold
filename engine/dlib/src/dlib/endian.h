#ifndef DM_ENDIAN
#define DM_ENDIAN

/// Endian. Defined to #DM_ENDIAN_LITTLE || #DM_ENDIAN_BIG
#define DM_ENDIAN
#undef DM_ENDIAN

#define DM_ENDIAN_LITTLE 0
#define DM_ENDIAN_BIG 1

#if defined(__x86_64) || defined(__x86_64__) || defined(_X86_) || defined(__i386__) || defined(_M_IX86) || defined(__LITTLE_ENDIAN__) || (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define DM_ENDIAN DM_ENDIAN_LITTLE
#else
#error "Unknown endian"
#endif

#endif
