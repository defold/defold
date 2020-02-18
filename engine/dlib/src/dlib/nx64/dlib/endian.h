#ifndef DM_ENDIAN_H
#define DM_ENDIAN_H

#include <nn/util/util_endian.h>

#define DM_ENDIAN_LITTLE 0
#define DM_ENDIAN_BIG 1

#define DM_ENDIAN DM_ENDIAN_LITTLE

// Network is BIG_ENDIAN

namespace dmEndian {
    static inline uint16_t ToNetwork(uint16_t x) {
        //return htons(x);
        uint16_t n;
        nn::util::StoreBigEndian(&n, x);
        return n;
    }

    static inline uint16_t ToHost(uint16_t x) {
        //return ntohs(x);
        return nn::util::LoadBigEndian<uint16_t>((const uint16_t*)&x);
    }

    static inline uint32_t ToNetwork(uint32_t x) {
        //return htonl(x);
        uint32_t n;
        nn::util::StoreBigEndian(&n, x);
        return n;
    }

    static inline uint32_t ToHost(uint32_t x) {
        //return ntohl(x);
        return nn::util::LoadBigEndian<uint32_t>((const uint32_t*)&x);
    }

    static inline uint64_t ToNetwork(uint64_t x) {
#if DM_ENDIAN == DM_ENDIAN_LITTLE
        //return (((uint64_t)htonl(x)) << 32) | htonl(x >> 32);
        uint64_t n;
        nn::util::StoreBigEndian(&n, x);
        return n;
#else
        return x;
#endif
    }

    static inline uint64_t ToHost(uint64_t x) {
#if DM_ENDIAN == DM_ENDIAN_LITTLE
        //return (((uint64_t)ntohl(x)) << 32) | ntohl(x >> 32);
        return nn::util::LoadBigEndian<uint64_t>((const uint64_t*)&x);
#else
        return x;
#endif
    }
}


#endif // DM_ENDIAN_H
