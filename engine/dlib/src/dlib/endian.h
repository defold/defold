#ifndef DM_ENDIAN
#define DM_ENDIAN

#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__) || defined(__AVM2__)
#include <netinet/in.h>
#elif defined(_WIN32)
#include <winsock2.h>
#else
#error "Unsupported platform"
#endif

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

namespace dmEndian {
    static inline uint16_t ToNetwork(uint16_t x) {
        return htons(x);
    }

    static inline uint16_t ToHost(uint16_t x) {
        return ntohs(x);
    }

    static inline uint32_t ToNetwork(uint32_t x) {
        return htonl(x);
    }

    static inline uint32_t ToHost(uint32_t x) {
        return ntohl(x);
    }

    static inline uint64_t ToNetwork(uint64_t x) {
#if DM_ENDIAN == DM_ENDIAN_LITTLE
        return (((uint64_t)htonl(x)) << 32) | htonl(x >> 32);
#else
        return x;
#endif
    }

    static inline uint64_t ToHost(uint64_t x) {
#if DM_ENDIAN == DM_ENDIAN_LITTLE
        return (((uint64_t)ntohl(x)) << 32) | ntohl(x >> 32);
#else
        return x;
#endif
    }
}


#endif
