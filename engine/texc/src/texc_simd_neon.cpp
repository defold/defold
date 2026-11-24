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

#include "texc_pixel.h"
#include "texc_resize.h"
#include "texc_private.h"

namespace dmTexc
{
    namespace NEON
    {
        void PreMultiplyAlpha(uint8_t* data, uint32_t width, uint32_t height)
        {
            uint32_t remaining = width * height;
            while (remaining >= 4)
            {
                SimdStoreBytes(data, PreMultiplyPixels(SimdLoadBytes(data)));
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

        void ConvertPreview(const uint8_t* input_data, uint8_t* output_data, uint32_t width, uint32_t height)
        {
            if (!width || !height)
                return;
            size_t row_bytes = (size_t)width * 4;
            for (uint32_t y = 0; y < height; ++y)
            {
                const uint8_t* source = input_data + y * row_bytes;
                uint8_t*       destination = output_data + (height - y - 1) * row_bytes;
                uint32_t       remaining = width;
                while (remaining >= 4)
                {
                    SimdStoreBytes(destination, PreMultiplyPixels(ReversePixels(SimdLoadBytes(source))));
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
        }

        bool ResizeRGBA8(const uint8_t* source, uint32_t source_width, uint32_t source_height, uint8_t* destination, uint32_t width, uint32_t height)
        {
            return ResizeRGBA8WithKernels(source, source_width, source_height, destination, width, height, &g_ResizeKernels);
        }

        void AccumulateResizeRow(const float* source, float* destination, size_t count, float weight, bool add)
        {
            dsimd_float4_t scale = SimdSplat(weight);
            if (add)
            {
                for (size_t i = 0; i < count; i += 4)
                    SimdStore(destination + i, SimdMulAdd(SimdLoad(source + i), scale, SimdLoad(destination + i)));
            }
            else
            {
                for (size_t i = 0; i < count; i += 4)
                    SimdStore(destination + i, SimdMul(SimdLoad(source + i), scale));
            }
        }

        void RGBA8888ToRGB888(const uint8_t* source, uint32_t count, uint8_t* destination)
        {
            static const uint8_t indices[16] = { 0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, 128, 128, 128, 128 };
            uint32_t             i = 0;
            for (; count - i >= 16; i += 16)
            {
                dsimd_byte16_t mask = SimdLoadBytes(indices);
                dsimd_float4_t p0 = SimdUintBits(SimdBytesToUint(SimdShuffleBytes(SimdLoadBytes(source + (size_t)i * 4), mask)));
                dsimd_float4_t p1 = SimdUintBits(SimdBytesToUint(SimdShuffleBytes(SimdLoadBytes(source + (size_t)i * 4 + 16), mask)));
                dsimd_float4_t p2 = SimdUintBits(SimdBytesToUint(SimdShuffleBytes(SimdLoadBytes(source + (size_t)i * 4 + 32), mask)));
                dsimd_float4_t p3 = SimdUintBits(SimdBytesToUint(SimdShuffleBytes(SimdLoadBytes(source + (size_t)i * 4 + 48), mask)));
                // Each compacted register holds twelve bytes; join without over-storing the final output.
                SimdStore(destination + (size_t)i * 3, SimdShuffle2(p0, p1, 0, 1, 2, 4));
                SimdStore(destination + (size_t)i * 3 + 16, SimdShuffle2(p1, p2, 1, 2, 4, 5));
                SimdStore(destination + (size_t)i * 3 + 32, SimdShuffle2(p2, p3, 2, 4, 5, 6));
            }
            for (; i < count; ++i)
            {
                destination[(size_t)i * 3] = source[(size_t)i * 4];
                destination[(size_t)i * 3 + 1] = source[(size_t)i * 4 + 1];
                destination[(size_t)i * 3 + 2] = source[(size_t)i * 4 + 2];
            }
        }

        static dsimd_ushort8_t RGB565Pairs(dsimd_ushort8_t rg, dsimd_ushort8_t ba)
        {
            // Preserve red's five high bits, insert six green bits, then replace the low five bits with blue.
            dsimd_ushort8_t value = SimdOrU16(SimdAndU16(SimdShiftLeftU16(rg, 8), SimdSplatU16(0xf800)), SimdShiftRightU16(rg, 5));
            return SimdOrU16(SimdAndU16(value, SimdSplatU16(0xffe0)), SimdShiftRightU16(SimdShiftLeftU16(ba, 8), 11));
        }
        void RGBA8888ToRGB565(const uint8_t* source, uint32_t count, uint16_t* destination)
        {
            uint32_t i = 0;
            for (; count - i >= 16; i += 16)
            {
                dsimd_ushort8_t rg0, ba0, rg1, ba1;
                SimdLoadInterleavedU16x2(source + (size_t)i * 4, &rg0, &ba0);
                SimdLoadInterleavedU16x2(source + (size_t)i * 4 + 32, &rg1, &ba1);
                SimdStoreU16(destination + i, RGB565Pairs(rg0, ba0));
                SimdStoreU16(destination + i + 8, RGB565Pairs(rg1, ba1));
            }
            for (; i < count; ++i)
            {
                const uint8_t* p = source + (size_t)i * 4;
                destination[i] = ((p[0] >> 3) << 11) | ((p[1] >> 2) << 5) | (p[2] >> 3);
            }
        }

        static dsimd_ushort8_t RGBA4444Pairs(dsimd_ushort8_t pixels)
        {
            return SimdOrU16(SimdAndU16(pixels, SimdSplatU16(0x00f0)), SimdShiftRightU16(pixels, 12));
        }
        void RGBA8888ToRGBA4444(const uint8_t* source, uint32_t count, uint16_t* destination)
        {
            static const uint8_t indices[16] = { 1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14 };
            uint32_t             i = 0;
            for (; count - i >= 16; i += 16)
            {
                dsimd_ushort8_t p0 = RGBA4444Pairs(SimdLoadU16(source + (size_t)i * 4));
                dsimd_ushort8_t p1 = RGBA4444Pairs(SimdLoadU16(source + (size_t)i * 4 + 16));
                dsimd_ushort8_t p2 = RGBA4444Pairs(SimdLoadU16(source + (size_t)i * 4 + 32));
                dsimd_ushort8_t p3 = RGBA4444Pairs(SimdLoadU16(source + (size_t)i * 4 + 48));
                SimdStoreBytes(destination + i, SimdShuffleBytes(SimdPackBytes(p0, p1), SimdLoadBytes(indices)));
                SimdStoreBytes(destination + i + 8, SimdShuffleBytes(SimdPackBytes(p2, p3), SimdLoadBytes(indices)));
            }
            for (; i < count; ++i)
            {
                const uint8_t* p = source + (size_t)i * 4;
                destination[i] = ((p[0] >> 4) << 12) | ((p[1] >> 4) << 8) | ((p[2] >> 4) << 4) | (p[3] >> 4);
            }
        }

        static dsimd_byte16_t ReversePixels(dsimd_byte16_t value)
        {
            return SimdUintToBytes(SimdFloatBits(SimdShuffle(SimdUintBits(SimdBytesToUint(value)), 3, 2, 1, 0)));
        }

        void FlipImageX(uint8_t* data, uint32_t width, uint32_t height, uint32_t pixel_size)
        {
            for (uint32_t y = 0; y < height; ++y)
            {
                uint8_t* row = data + (size_t)y * width * pixel_size;
                uint32_t x = 0;
                for (; x + 4 <= width / 2; x += 4)
                {
                    uint8_t* a = row + x * pixel_size;
                    uint8_t* b = row + (width - x - 4) * pixel_size;
                    if (pixel_size == 4)
                    {
                        dsimd_byte16_t av = SimdLoadBytes(a);
                        dsimd_byte16_t bv = SimdLoadBytes(b);
                        SimdStoreBytes(a, ReversePixels(bv));
                        SimdStoreBytes(b, ReversePixels(av));
                    }
                    else
                    {
                        dsimd_byte16_t a0 = SimdLoadBytes(a);
                        dsimd_byte16_t a1 = SimdLoadBytes(a + 16);
                        dsimd_byte16_t a2 = SimdLoadBytes(a + 32);
                        dsimd_byte16_t a3 = SimdLoadBytes(a + 48);
                        dsimd_byte16_t b0 = SimdLoadBytes(b);
                        dsimd_byte16_t b1 = SimdLoadBytes(b + 16);
                        dsimd_byte16_t b2 = SimdLoadBytes(b + 32);
                        dsimd_byte16_t b3 = SimdLoadBytes(b + 48);
                        SimdStoreBytes(a, b3);
                        SimdStoreBytes(a + 16, b2);
                        SimdStoreBytes(a + 32, b1);
                        SimdStoreBytes(a + 48, b0);
                        SimdStoreBytes(b, a3);
                        SimdStoreBytes(b + 16, a2);
                        SimdStoreBytes(b + 32, a1);
                        SimdStoreBytes(b + 48, a0);
                    }
                }
                for (; x < width / 2; ++x)
                    for (uint32_t c = 0; c < pixel_size; ++c)
                    {
                        uint8_t* a = row + x * pixel_size + c;
                        uint8_t* b = row + (width - x - 1) * pixel_size + c;
                        uint8_t  t = *a;
                        *a = *b;
                        *b = t;
                    }
            }
        }

        static dsimd_uint4_t FloatBitsToHalf(dsimd_uint4_t bits)
        {
            // Match FloatToHalf, including ties, subnormals and special-value payloads.
            // Hardware half conversion uses different rounding and overflow behavior.
            dsimd_uint4_t sign = SimdAndUint(SimdShiftRightUint(bits, 16), SimdSplatUint(0x8000));
            dsimd_uint4_t exponent = SimdAndUint(SimdShiftRightUint(bits, 23), SimdSplatUint(255));
            dsimd_uint4_t mantissa = SimdAndUint(bits, SimdSplatUint(0x7fffff));
            dsimd_uint4_t result = SimdAddUint(SimdShiftLeftUint(SimdSubUint(exponent, SimdSplatUint(112)), 10),
                                               SimdShiftRightUint(SimdAddUint(mantissa, SimdSplatUint(0x1000)), 13));
            dsimd_uint4_t subnormal = SimdShiftRightUintEach(SimdOrUint(mantissa, SimdSplatUint(0x800000)), SimdSubUint(SimdSplatUint(113), exponent));
            subnormal = SimdShiftRightUint(SimdAddUint(subnormal, SimdSplatUint(0x1000)), 13);
            subnormal = SimdSelectUint(SimdLessUint(exponent, SimdSplatUint(102)), SimdSplatUint(0), subnormal);
            result = SimdSelectUint(SimdLessUint(exponent, SimdSplatUint(113)), subnormal, result);
            dsimd_uint4_t special = SimdOrUint(SimdSplatUint(0x7c00), SimdShiftRightUint(mantissa, 13));
            result = SimdSelectUint(SimdLessUint(exponent, SimdSplatUint(143)), result, special);
            return SimdOrUint(sign, result);
        }

        void FloatToHalf(const float* source, uint32_t count, uint16_t* destination)
        {
            uint32_t i = 0;
            for (; i + 8 <= count; i += 8)
            {
                dsimd_uint4_t low = FloatBitsToHalf(SimdLoadUint(source + i));
                dsimd_uint4_t high = FloatBitsToHalf(SimdLoadUint(source + i + 4));
                SimdStoreU16(destination + i, SimdPackU16(low, high));
            }
            for (; i < count; ++i)
                destination[i] = dmTexc::FloatToHalf(source[i]);
        }

        static dsimd_float4_t DitherFract(dsimd_float4_t value)
        {
            return SimdSub(value, SimdConvertIntToFloat(SimdTruncateToInt(value)));
        }

        static dsimd_uint4_t DitherNoise(dsimd_float4_t random, float multiplier, float bias)
        {
            dsimd_float4_t value = SimdSub(SimdMul(random, SimdSplat(multiplier)), SimdSplat(bias));
            // Truncate signed noise before narrowing it.
            dsimd_int4_t  signed_noise = SimdTruncateToInt(value);
            dsimd_uint4_t bits;
            memcpy(&bits, &signed_noise, sizeof(bits));
            return bits;
        }

        static dsimd_byte16_t DitherAdd(dsimd_byte16_t pixels, dsimd_ushort8_t low, dsimd_ushort8_t high)
        {
            return SimdPackSignedBytes(SimdAddU16(SimdWidenBytesLow(pixels), low),
                                       SimdAddU16(SimdWidenBytesHigh(pixels), high));
        }

        static dsimd_float4_t DitherRandom(dsimd_float4_t x, dsimd_float4_t y)
        {
            return DitherFract(SimdMul(DitherFract(SimdMulAdd(x, SimdSplat(0.06711056f), y)),
                                       SimdSplat(52.9829189f)));
        }

        static void DitherRGBx565Block(uint8_t* data, uint32_t x, dsimd_float4_t y_offset)
        {
            dsimd_float4_t  coord = SimdMake((float)x, (float)(x + 1), (float)(x + 2), (float)(x + 3));
            dsimd_float4_t  rnd0 = DitherRandom(coord, y_offset);
            dsimd_float4_t  rnd1 = DitherRandom(SimdAdd(coord, SimdSplat(4)), y_offset);
            dsimd_float4_t  rnd2 = DitherRandom(SimdAdd(coord, SimdSplat(8)), y_offset);
            dsimd_float4_t  rnd3 = DitherRandom(SimdAdd(coord, SimdSplat(12)), y_offset);
            dsimd_ushort8_t rlo = SimdNarrowU16(DitherNoise(rnd0, 8.0f, 4.0f),
                                                DitherNoise(rnd1, 8.0f, 4.0f));
            dsimd_ushort8_t rhi = SimdNarrowU16(DitherNoise(rnd2, 8.0f, 4.0f),
                                                DitherNoise(rnd3, 8.0f, 4.0f));
            dsimd_ushort8_t glo = SimdNarrowU16(DitherNoise(SimdSub(SimdSplat(1), rnd0), 4.0f, 2.0f),
                                                DitherNoise(SimdSub(SimdSplat(1), rnd1), 4.0f, 2.0f));
            dsimd_ushort8_t ghi = SimdNarrowU16(DitherNoise(SimdSub(SimdSplat(1), rnd2), 4.0f, 2.0f),
                                                DitherNoise(SimdSub(SimdSplat(1), rnd3), 4.0f, 2.0f));
            dsimd_byte16_t  r, g, b, a;
            SimdLoadInterleavedBytes4(data, &r, &g, &b, &a);
            r = DitherAdd(r, rlo, rhi);
            g = DitherAdd(g, glo, ghi);
            b = DitherAdd(b, rlo, rhi);

            SimdStoreInterleavedBytes4(data, r, g, b, a);
        }

        void DitherRGBx565(uint8_t* data, uint32_t width, uint32_t height)
        {
            for (uint32_t y = 0; y < height; ++y)
            {
                uint32_t       x = 0;
                dsimd_float4_t y_offset = SimdSplat((float)y * 0.00583715f);
                for (; x + 16 <= width; x += 16, data += 64)
                    DitherRGBx565Block(data, x, y_offset);
                for (; x < width; ++x, data += 4)
                {
                    float random = InterleavedGradientNoise((float)x, (float)y);
                    data[0] = addNoise(data[0], (int8_t)(random * 8.0f - 4.0f));
                    data[1] = addNoise(data[1], (int8_t)((1.0f - random) * 4.0f - 2.0f));
                    data[2] = addNoise(data[2], (int8_t)(random * 8.0f - 4.0f));
                }
            }
        }
    } // namespace NEON
} // namespace dmTexc
