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

#ifndef DM_TEXC_PIXEL_H
#define DM_TEXC_PIXEL_H

#include <simd/simd.h>

namespace dmTexc
{
    static inline uint16_t FloatToHalf(float value)
    {
        uint32_t bits;
        memcpy(&bits, &value, sizeof(bits));

        // IEEE-754 float32: 1 sign bit, 8 exponent bits, 23 mantissa bits.
        // IEEE-754 float16: 1 sign bit, 5 exponent bits, 10 mantissa bits.
        uint32_t sign = (bits >> 16) & 0x8000;
        int32_t  exponent = ((bits >> 23) & 0xff) - 127 + 15;
        uint32_t mantissa = bits & 0x7fffff;

        if (exponent <= 0)
        {
            // Values too small for a normalized half become subnormals, or zero
            // when the exponent is outside the representable subnormal range.
            if (exponent < -10)
            {
                return (uint16_t)sign;
            }
            mantissa = (mantissa | 0x800000) >> (1 - exponent);
            return (uint16_t)(sign | ((mantissa + 0x1000) >> 13));
        }

        if (exponent >= 31)
        {
            // Preserve infinities and NaNs as half-float special values.
            if (mantissa == 0)
            {
                return (uint16_t)(sign | 0x7c00);
            }
            return (uint16_t)(sign | 0x7c00 | (mantissa >> 13));
        }

        // Round the float32 mantissa from 23 to 10 bits. Use addition between
        // exponent and mantissa fields so rounding overflow carries into exponent.
        return (uint16_t)(sign | ((exponent << 10) + ((mantissa + 0x1000) >> 13)));
    }

    static inline float fract(float v)
    {
        float f = v - (int32_t)v;
        return f;
    }

    // note: returns [-intensity;intensity[, magnitude of 2x intensity
    // note: from "NEXT GENERATION POST PROCESSING IN CALL OF DUTY: ADVANCED WARFARE"
    //       http://advances.realtimerendering.com/s2014/index.html
    static inline float InterleavedGradientNoise(float u, float v)
    {
        const float magic[3] = { 0.06711056f, 0.00583715f, 52.9829189f };
        return fract(magic[2] * fract(u * magic[0] + v * magic[1]));
    }

    static inline uint8_t addNoise(uint8_t v_in, int8_t noise)
    {
        int16_t v = v_in + noise;
        if (v > 255)
            v = 255;
        else if (v < 0)
            v = 0;
        return (uint8_t)v;
    }

#if !defined(DM_SIMD_SCALAR_H)
    static inline dsimd_ushort8_t Divide255(dsimd_ushort8_t value)
    {
        // Exact floor(value / 255) for products of two bytes (0..65025).
        return Simd128ShiftRightU16(Simd128AddU16(Simd128AddU16(value, Simd128SplatU16(1)), Simd128ShiftRightU16(value, 8)), 8);
    }
#endif

    static inline dsimd_byte16_t PreMultiplyPixels(dsimd_byte16_t pixels)
    {
#if defined(DM_SIMD_SCALAR_H)
        // Keep R/B in separate 16-bit fields of each pixel. Products and the
        // division correction stay below 65536, so fields cannot carry into one another.
        dsimd_uint4_t bits = Simd128BytesToUint(pixels);
        dsimd_uint4_t alpha = Simd128ShiftRightUint(bits, 24);
        dsimd_uint4_t rb_mask = Simd128SplatUint(0x00ff00ffu);
        dsimd_uint4_t rb = Simd128MulUint(Simd128AndUint(bits, rb_mask), alpha);
        rb = Simd128AddUint(Simd128AddUint(rb, Simd128AndUint(Simd128ShiftRightUint(rb, 8), rb_mask)), Simd128SplatUint(0x00010001u));
        rb = Simd128AndUint(Simd128ShiftRightUint(rb, 8), rb_mask);
        dsimd_uint4_t green = Simd128MulUint(Simd128AndUint(Simd128ShiftRightUint(bits, 8), Simd128SplatUint(255)), alpha);
        green = Simd128ShiftRightUint(Simd128AddUint(Simd128AddUint(green, Simd128ShiftRightUint(green, 8)), Simd128SplatUint(1)), 8);
        dsimd_uint4_t result = Simd128OrUint(Simd128OrUint(rb, Simd128ShiftLeftUint(green, 8)), Simd128AndUint(bits, Simd128SplatUint(0xff000000u)));
        return Simd128UintToBytes(result);
#else
        dsimd_uint4_t alphas;
#if defined(DM_SIMD_SSE3_H) && (!defined(DM_SIMD_SSE41) || (defined(DM_SIMD_X86_ISA) && DM_SIMD_X86_ISA == 3))
        // SSE3 has no byte table lookup. Replicate each pixel's alpha with shifts.
        alphas = Simd128ShiftRightUint(Simd128BytesToUint(pixels), 24);
        alphas = Simd128OrUint(alphas, Simd128ShiftLeftUint(alphas, 8));
        alphas = Simd128OrUint(alphas, Simd128ShiftLeftUint(alphas, 16));
#else
        static const uint8_t alpha_indices[16] = { 3, 3, 3, 3, 7, 7, 7, 7, 11, 11, 11, 11, 15, 15, 15, 15 };
        alphas = Simd128BytesToUint(Simd128ShuffleBytes(pixels, Simd128LoadBytes(alpha_indices)));
#endif
        // Multiply the alpha lanes by 255 so they remain unchanged after division.
        dsimd_byte16_t  factors = Simd128UintToBytes(Simd128OrUint(alphas, Simd128SplatUint(0xff000000u)));
        dsimd_ushort8_t low = Simd128MulU16(Simd128WidenBytesLow(pixels), Simd128WidenBytesLow(factors));
        dsimd_ushort8_t high = Simd128MulU16(Simd128WidenBytesHigh(pixels), Simd128WidenBytesHigh(factors));
        return Simd128PackBytes(Divide255(low), Divide255(high));
#endif
    }

    static inline dsimd_byte16_t ReversePixels(dsimd_byte16_t pixels)
    {
#if defined(DM_SIMD_SCALAR_H) || (defined(DM_SIMD_SSE3_H) && (!defined(DM_SIMD_SSE41) || (defined(DM_SIMD_X86_ISA) && DM_SIMD_X86_ISA == 3)))
        // Reverse bytes within each 32-bit pixel without SSE3's scalar table lookup.
        dsimd_uint4_t bits = Simd128BytesToUint(pixels);
        dsimd_uint4_t low = Simd128OrUint(Simd128ShiftRightUint(bits, 24),
                                       Simd128AndUint(Simd128ShiftRightUint(bits, 8), Simd128SplatUint(0x0000ff00u)));
        dsimd_uint4_t high = Simd128OrUint(Simd128ShiftLeftUint(bits, 24),
                                        Simd128AndUint(Simd128ShiftLeftUint(bits, 8), Simd128SplatUint(0x00ff0000u)));
        return Simd128UintToBytes(Simd128OrUint(low, high));
#else
        static const uint8_t indices[16] = { 3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12 };
        return Simd128ShuffleBytes(pixels, Simd128LoadBytes(indices));
#endif
    }

    static inline void PreMultiplyPixels_Simd128(uint8_t* data, uint32_t remaining)
    {
        while (remaining >= 4)
        {
            Simd128StoreBytes(data, PreMultiplyPixels(Simd128LoadBytes(data)));
            data += 16;
            remaining -= 4;
        }
        // At most three pixels; do not read beyond small mips or unaligned buffers.
        while (remaining--)
        {
            uint32_t alpha = data[3];
            data[0] = (uint8_t)(data[0] * alpha / 255);
            data[1] = (uint8_t)(data[1] * alpha / 255);
            data[2] = (uint8_t)(data[2] * alpha / 255);
            data += 4;
        }
    }

    static inline void ConvertPreviewPixels_Simd128(const uint8_t* source, uint8_t* destination, uint32_t remaining)
    {
        while (remaining >= 4)
        {
            Simd128StoreBytes(destination, PreMultiplyPixels(ReversePixels(Simd128LoadBytes(source))));
            source += 16;
            destination += 16;
            remaining -= 4;
        }
        while (remaining--)
        {
            uint32_t alpha = source[0];
            destination[0] = (uint8_t)(source[3] * alpha / 255);
            destination[1] = (uint8_t)(source[2] * alpha / 255);
            destination[2] = (uint8_t)(source[1] * alpha / 255);
            destination[3] = (uint8_t)alpha;
            source += 4;
            destination += 4;
        }
    }
} // namespace dmTexc

#endif // DM_TEXC_PIXEL_H
