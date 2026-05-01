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

#ifndef DMSDK_ENDIAN_H
#define DMSDK_ENDIAN_H

#include <stdint.h>

#if defined(_MSC_VER)
#include <stdlib.h>
#endif

#if defined(__has_builtin)
#define DM_ENDIAN_HAS_BUILTIN(x) __has_builtin(x)
#else
#define DM_ENDIAN_HAS_BUILTIN(x) 0
#endif

/// Endian. Defined to #DM_ENDIAN_LITTLE or #DM_ENDIAN_BIG.
#define DM_ENDIAN
#undef DM_ENDIAN

#define DM_ENDIAN_LITTLE 0
#define DM_ENDIAN_BIG 1

/*# Endian API documentation
 * Endian conversion functions.
 *
 * @document
 * @name Endian
 * @language C
 */

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define DM_ENDIAN DM_ENDIAN_LITTLE
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define DM_ENDIAN DM_ENDIAN_BIG
#elif defined(_WIN32) || defined(_GAMING_XBOX) || defined(__NX__) || defined(__SCE__) || \
    defined(__LITTLE_ENDIAN__) || defined(__ARMEL__) || defined(__AARCH64EL__) || \
    defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86) || \
    defined(_M_ARM) || defined(_M_ARM64)
#define DM_ENDIAN DM_ENDIAN_LITTLE
#elif defined(__BIG_ENDIAN__) || defined(__ARMEB__) || defined(__AARCH64EB__)
#define DM_ENDIAN DM_ENDIAN_BIG
#else
#error "Unknown endian"
#endif

/*# swap bytes in a 16-bit value
 * @name EndianSwap16
 * @param x [type:uint16_t] Value to byte swap
 * @return value [type:uint16_t] Byte-swapped value
 */
static inline uint16_t EndianSwap16(uint16_t x)
{
#if defined(_MSC_VER)
    return _byteswap_ushort(x);
#elif DM_ENDIAN_HAS_BUILTIN(__builtin_bswap16) || defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap16(x);
#else
    return (uint16_t)((x << 8) | (x >> 8));
#endif
}

/*# swap bytes in a 32-bit value
 * @name EndianSwap32
 * @param x [type:uint32_t] Value to byte swap
 * @return value [type:uint32_t] Byte-swapped value
 */
static inline uint32_t EndianSwap32(uint32_t x)
{
#if defined(_MSC_VER)
    return _byteswap_ulong(x);
#elif DM_ENDIAN_HAS_BUILTIN(__builtin_bswap32) || defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(x);
#else
    return ((x & 0x000000ffU) << 24) |
           ((x & 0x0000ff00U) << 8) |
           ((x & 0x00ff0000U) >> 8) |
           ((x & 0xff000000U) >> 24);
#endif
}

/*# swap bytes in a 64-bit value
 * @name EndianSwap64
 * @param x [type:uint64_t] Value to byte swap
 * @return value [type:uint64_t] Byte-swapped value
 */
static inline uint64_t EndianSwap64(uint64_t x)
{
#if defined(_MSC_VER)
    return _byteswap_uint64(x);
#elif DM_ENDIAN_HAS_BUILTIN(__builtin_bswap64) || defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap64(x);
#else
    return ((x & 0x00000000000000ffULL) << 56) |
           ((x & 0x000000000000ff00ULL) << 40) |
           ((x & 0x0000000000ff0000ULL) << 24) |
           ((x & 0x00000000ff000000ULL) << 8) |
           ((x & 0x000000ff00000000ULL) >> 8) |
           ((x & 0x0000ff0000000000ULL) >> 24) |
           ((x & 0x00ff000000000000ULL) >> 40) |
           ((x & 0xff00000000000000ULL) >> 56);
#endif
}

/*# convert a 16-bit host value to network byte order
 * @name EndianToNetwork16
 * @param x [type:uint16_t] Host-order value
 * @return value [type:uint16_t] Network-order value
 */
static inline uint16_t EndianToNetwork16(uint16_t x)
{
#if DM_ENDIAN == DM_ENDIAN_LITTLE
    return EndianSwap16(x);
#else
    return x;
#endif
}

/*# convert a 16-bit network value to host byte order
 * @name EndianToHost16
 * @param x [type:uint16_t] Network-order value
 * @return value [type:uint16_t] Host-order value
 */
static inline uint16_t EndianToHost16(uint16_t x)
{
    return EndianToNetwork16(x);
}

/*# convert a 32-bit host value to network byte order
 * @name EndianToNetwork32
 * @param x [type:uint32_t] Host-order value
 * @return value [type:uint32_t] Network-order value
 */
static inline uint32_t EndianToNetwork32(uint32_t x)
{
#if DM_ENDIAN == DM_ENDIAN_LITTLE
    return EndianSwap32(x);
#else
    return x;
#endif
}

/*# convert a 32-bit network value to host byte order
 * @name EndianToHost32
 * @param x [type:uint32_t] Network-order value
 * @return value [type:uint32_t] Host-order value
 */
static inline uint32_t EndianToHost32(uint32_t x)
{
    return EndianToNetwork32(x);
}

/*# convert a 64-bit host value to network byte order
 * @name EndianToNetwork64
 * @param x [type:uint64_t] Host-order value
 * @return value [type:uint64_t] Network-order value
 */
static inline uint64_t EndianToNetwork64(uint64_t x)
{
#if DM_ENDIAN == DM_ENDIAN_LITTLE
    return EndianSwap64(x);
#else
    return x;
#endif
}

/*# convert a 64-bit network value to host byte order
 * @name EndianToHost64
 * @param x [type:uint64_t] Network-order value
 * @return value [type:uint64_t] Host-order value
 */
static inline uint64_t EndianToHost64(uint64_t x)
{
    return EndianToNetwork64(x);
}

#endif // DMSDK_ENDIAN_H
