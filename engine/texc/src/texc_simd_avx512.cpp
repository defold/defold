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
#include <simd/simd_512.h>

namespace dmTexc
{
    namespace AVX512
    {
        static const uint32_t PIXELS_PER_VECTOR = sizeof(dsimd_byte64_t) / 4;

        static inline dsimd_byte64_t PreMultiplyPixels_Simd512(dsimd_byte64_t pixels)
        {
            static const uint8_t alpha_indices[64] = {
                3, 3, 3, 3, 7, 7, 7, 7, 11, 11, 11, 11, 15, 15, 15, 15,
                3, 3, 3, 3, 7, 7, 7, 7, 11, 11, 11, 11, 15, 15, 15, 15,
                3, 3, 3, 3, 7, 7, 7, 7, 11, 11, 11, 11, 15, 15, 15, 15,
                3, 3, 3, 3, 7, 7, 7, 7, 11, 11, 11, 11, 15, 15, 15, 15
            };
            dsimd_uint16_t   alpha = Simd512BytesToUint(Simd512ShuffleBytes(pixels, Simd512LoadBytes(alpha_indices)));
            dsimd_byte64_t   factors = Simd512UintToBytes(Simd512OrUint(alpha, Simd512SplatUint(0xff000000u)));
            dsimd_ushort32_t low = Simd512MulU16(Simd512WidenBytesLow(pixels), Simd512WidenBytesLow(factors));
            dsimd_ushort32_t high = Simd512MulU16(Simd512WidenBytesHigh(pixels), Simd512WidenBytesHigh(factors));
            low = Simd512ShiftRightU16(Simd512AddU16(Simd512AddU16(low, Simd512SplatU16(1)), Simd512ShiftRightU16(low, 8)), 8);
            high = Simd512ShiftRightU16(Simd512AddU16(Simd512AddU16(high, Simd512SplatU16(1)), Simd512ShiftRightU16(high, 8)), 8);
            return Simd512PackBytes(low, high);
        }

        static inline dsimd_byte64_t ReversePixels_Simd512(dsimd_byte64_t pixels)
        {
            static const uint8_t indices[64] = {
                3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12,
                3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12,
                3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12,
                3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12
            };
            return Simd512ShuffleBytes(pixels, Simd512LoadBytes(indices));
        }

        void PreMultiplyAlpha(uint8_t* data, uint32_t width, uint32_t height)
        {
            uint32_t remaining = width * height;
            while (remaining >= PIXELS_PER_VECTOR)
            {
                Simd512StoreBytes(data, PreMultiplyPixels_Simd512(Simd512LoadBytes(data)));
                data += sizeof(dsimd_byte64_t);
                remaining -= PIXELS_PER_VECTOR;
            }

            PreMultiplyPixels_Simd128(data, remaining);
        }

        void ConvertPreview(const uint8_t* input_data, uint8_t* output_data, uint32_t width, uint32_t height)
        {
            if (!width || !height)
            {
                return;
            }

            size_t row_bytes = (size_t)width * 4;
            for (uint32_t y = 0; y < height; ++y)
            {
                const uint8_t* source = input_data + y * row_bytes;
                uint8_t*       destination = output_data + (height - y - 1) * row_bytes;
                uint32_t       remaining = width;
                while (remaining >= PIXELS_PER_VECTOR)
                {
                    Simd512StoreBytes(destination, PreMultiplyPixels_Simd512(ReversePixels_Simd512(Simd512LoadBytes(source))));
                    source += sizeof(dsimd_byte64_t);
                    destination += sizeof(dsimd_byte64_t);
                    remaining -= PIXELS_PER_VECTOR;
                }

                ConvertPreviewPixels_Simd128(source, destination, remaining);
            }
        }

        bool ResizeRGBA8(const uint8_t* source, uint32_t source_width, uint32_t source_height, uint8_t* destination, uint32_t width, uint32_t height)
        {
            return ResizeRGBA8WithKernels(source, source_width, source_height, destination, width, height, &g_ResizeKernels);
        }

        void AccumulateResizeRow(const float* source, float* destination, size_t count, float weight, bool add)
        {
            dsimd_float16_t vector_scale = Simd512Splat(weight);
            dsimd_float4_t     scale = Simd128Splat(weight);
            const size_t lanes = sizeof(dsimd_float16_t) / sizeof(float);
            size_t i = 0;
            if (add)
            {
                for (; i + lanes <= count; i += lanes)
                {
                    Simd512Store(destination + i, Simd512Add(Simd512Mul(Simd512Load(source + i), vector_scale), Simd512Load(destination + i)));
                }

                for (; i < count; i += 4)
                {
                    Simd128Store(destination + i, Simd128MulAdd(Simd128Load(source + i), scale, Simd128Load(destination + i)));
                }
            }
            else
            {
                for (; i + lanes <= count; i += lanes)
                {
                    Simd512Store(destination + i, Simd512Mul(Simd512Load(source + i), vector_scale));
                }

                for (; i < count; i += 4)
                {
                    Simd128Store(destination + i, Simd128Mul(Simd128Load(source + i), scale));
                }
            }
        }

    }
} // namespace dmTexc
