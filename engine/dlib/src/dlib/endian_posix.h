// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DM_ENDIAN_POSIX_H
#define DM_ENDIAN_POSIX_H

#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__)
    #include <netinet/in.h>
#elif defined(_WIN32)
    #include <winsock2.h>
#endif

/// Endian. Defined to #DM_ENDIAN_LITTLE || #DM_ENDIAN_BIG
#define DM_ENDIAN
#undef DM_ENDIAN

#define DM_ENDIAN_LITTLE 0
#define DM_ENDIAN_BIG 1

#if defined(__x86_64) || defined(__x86_64__) || defined(_X86_) || defined(__i386__) || defined(_M_IX86) \
    || defined(__LITTLE_ENDIAN__) || (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
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

#endif // DM_ENDIAN_POSIX_H
