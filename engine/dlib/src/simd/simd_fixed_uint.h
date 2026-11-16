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

#ifndef DM_SIMD_FIXED_UINT_H
#define DM_SIMD_FIXED_UINT_H

#include <simd/simd.h>

/*# Fixed-width unsigned integer SIMD operations.
 * Eight and sixteen logical uint32_t lanes compose two and four 128-bit blocks.
 * Loads/stores are unaligned and access exactly 32 or 64 bytes.
 * Arithmetic wraps modulo 2^32; equality returns all-one or zero lane masks.
 * All examines the sign bit of every 32-bit lane, matching SimdAll.
 * Register values remain inside inlined code and never cross the dispatch ABI.
 * @document
 * @name SIMD Fixed Unsigned Integer
 * @language C
 */

typedef struct dsimd_uint8_t
{
    dsimd_uint4_t m_Blocks[2];
} dsimd_uint8_t;

static inline dsimd_uint8_t Simd8LoadUint(const void* source)
{
    dsimd_uint8_t result;
    for (unsigned i = 0; i < 2; ++i)
    {
        result.m_Blocks[i] = SimdLoadUint((const uint8_t*)source + i * 16);
    }

    return result;
}

static inline void Simd8StoreUint(void* destination, dsimd_uint8_t value)
{
    for (unsigned i = 0; i < 2; ++i)
    {
        SimdStoreUint((uint8_t*)destination + i * 16, value.m_Blocks[i]);
    }
}

static inline dsimd_uint8_t Simd8SplatUint(uint32_t value)
{
    dsimd_uint8_t result;
    for (unsigned i = 0; i < 2; ++i)
    {
        result.m_Blocks[i] = SimdSplatUint(value);
    }

    return result;
}

static inline dsimd_uint8_t Simd8AndUint(dsimd_uint8_t a, dsimd_uint8_t b)
{
    dsimd_uint8_t result;
    for (unsigned i = 0; i < 2; ++i)
    {
        result.m_Blocks[i] = SimdAndUint(a.m_Blocks[i], b.m_Blocks[i]);
    }

    return result;
}

static inline dsimd_uint8_t Simd8OrUint(dsimd_uint8_t a, dsimd_uint8_t b)
{
    dsimd_uint8_t result;
    for (unsigned i = 0; i < 2; ++i)
    {
        result.m_Blocks[i] = SimdOrUint(a.m_Blocks[i], b.m_Blocks[i]);
    }

    return result;
}

static inline dsimd_uint8_t Simd8XorUint(dsimd_uint8_t a, dsimd_uint8_t b)
{
    dsimd_uint8_t result;
    for (unsigned i = 0; i < 2; ++i)
    {
        result.m_Blocks[i] = SimdXorUint(a.m_Blocks[i], b.m_Blocks[i]);
    }

    return result;
}

static inline dsimd_uint8_t Simd8SubUint(dsimd_uint8_t a, dsimd_uint8_t b)
{
    dsimd_uint8_t result;
    for (unsigned i = 0; i < 2; ++i)
    {
        result.m_Blocks[i] = SimdSubUint(a.m_Blocks[i], b.m_Blocks[i]);
    }

    return result;
}

static inline dsimd_uint8_t Simd8EqualUint(dsimd_uint8_t a, dsimd_uint8_t b)
{
    dsimd_uint8_t result;
    for (unsigned i = 0; i < 2; ++i)
    {
        result.m_Blocks[i] = SimdEqualUint(a.m_Blocks[i], b.m_Blocks[i]);
    }

    return result;
}

static inline dsimd_uint8_t Simd8NotUint(dsimd_uint8_t value)
{
    dsimd_uint8_t result;
    for (unsigned i = 0; i < 2; ++i)
    {
        result.m_Blocks[i] = SimdNotUint(value.m_Blocks[i]);
    }

    return result;
}

static inline int Simd8All(dsimd_uint8_t value)
{
    dsimd_uint4_t combined = value.m_Blocks[0];
    for (unsigned i = 1; i < 2; ++i)
    {
        combined = SimdAndUint(combined, value.m_Blocks[i]);
    }

    return SimdAll(combined);
}

typedef struct dsimd_uint16_t
{
    dsimd_uint4_t m_Blocks[4];
} dsimd_uint16_t;

static inline dsimd_uint16_t Simd16LoadUint(const void* source)
{
    dsimd_uint16_t result;
    for (unsigned i = 0; i < 4; ++i)
    {
        result.m_Blocks[i] = SimdLoadUint((const uint8_t*)source + i * 16);
    }

    return result;
}

static inline void Simd16StoreUint(void* destination, dsimd_uint16_t value)
{
    for (unsigned i = 0; i < 4; ++i)
    {
        SimdStoreUint((uint8_t*)destination + i * 16, value.m_Blocks[i]);
    }
}

static inline dsimd_uint16_t Simd16SplatUint(uint32_t value)
{
    dsimd_uint16_t result;
    for (unsigned i = 0; i < 4; ++i)
    {
        result.m_Blocks[i] = SimdSplatUint(value);
    }

    return result;
}

static inline dsimd_uint16_t Simd16AndUint(dsimd_uint16_t a, dsimd_uint16_t b)
{
    dsimd_uint16_t result;
    for (unsigned i = 0; i < 4; ++i)
    {
        result.m_Blocks[i] = SimdAndUint(a.m_Blocks[i], b.m_Blocks[i]);
    }

    return result;
}

static inline dsimd_uint16_t Simd16OrUint(dsimd_uint16_t a, dsimd_uint16_t b)
{
    dsimd_uint16_t result;
    for (unsigned i = 0; i < 4; ++i)
    {
        result.m_Blocks[i] = SimdOrUint(a.m_Blocks[i], b.m_Blocks[i]);
    }

    return result;
}

static inline dsimd_uint16_t Simd16XorUint(dsimd_uint16_t a, dsimd_uint16_t b)
{
    dsimd_uint16_t result;
    for (unsigned i = 0; i < 4; ++i)
    {
        result.m_Blocks[i] = SimdXorUint(a.m_Blocks[i], b.m_Blocks[i]);
    }

    return result;
}

static inline dsimd_uint16_t Simd16SubUint(dsimd_uint16_t a, dsimd_uint16_t b)
{
    dsimd_uint16_t result;
    for (unsigned i = 0; i < 4; ++i)
    {
        result.m_Blocks[i] = SimdSubUint(a.m_Blocks[i], b.m_Blocks[i]);
    }

    return result;
}

static inline dsimd_uint16_t Simd16EqualUint(dsimd_uint16_t a, dsimd_uint16_t b)
{
    dsimd_uint16_t result;
    for (unsigned i = 0; i < 4; ++i)
    {
        result.m_Blocks[i] = SimdEqualUint(a.m_Blocks[i], b.m_Blocks[i]);
    }

    return result;
}

static inline dsimd_uint16_t Simd16NotUint(dsimd_uint16_t value)
{
    dsimd_uint16_t result;
    for (unsigned i = 0; i < 4; ++i)
    {
        result.m_Blocks[i] = SimdNotUint(value.m_Blocks[i]);
    }

    return result;
}

static inline int Simd16All(dsimd_uint16_t value)
{
    dsimd_uint4_t combined = value.m_Blocks[0];
    for (unsigned i = 1; i < 4; ++i)
    {
        combined = SimdAndUint(combined, value.m_Blocks[i]);
    }

    return SimdAll(combined);
}

#endif // DM_SIMD_FIXED_UINT_H
