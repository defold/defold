#ifndef DM_ENDIAN
#define DM_ENDIAN

#if defined(__x86_64) or defined(__x86_64__) or defined(_X86_) or defined(__i386__) or defined(_M_IX86) or defined(__LITTLE_ENDIAN__)
#define DM_ENDIAN_LITTLE
#else
#error "Unknown endian"
#endif

#endif
