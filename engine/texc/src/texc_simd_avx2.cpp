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
#include <simd/simd256.h>

namespace dmTexc
{
    namespace AVX2
    {
        static const uint32_t PIXELS_PER_VECTOR = sizeof(dsimd_byte32_t) / 4;

        static inline dsimd_byte32_t PreMultiplyPixels_Simd256(dsimd_byte32_t pixels)
        {
            static const uint8_t alpha_indices[32] = {
                3, 3, 3, 3, 7, 7, 7, 7, 11, 11, 11, 11, 15, 15, 15, 15,
                3, 3, 3, 3, 7, 7, 7, 7, 11, 11, 11, 11, 15, 15, 15, 15
            };
            dsimd_uint8_t   alpha = Simd256BytesToUint(Simd256ShuffleBytes(pixels, Simd256LoadBytes(alpha_indices)));
            dsimd_byte32_t   factors = Simd256UintToBytes(Simd256OrUint(alpha, Simd256SplatUint(0xff000000u)));
            dsimd_ushort16_t low = Simd256MulU16(Simd256WidenBytesLow(pixels), Simd256WidenBytesLow(factors));
            dsimd_ushort16_t high = Simd256MulU16(Simd256WidenBytesHigh(pixels), Simd256WidenBytesHigh(factors));
            low = Simd256ShiftRightU16(Simd256AddU16(Simd256AddU16(low, Simd256SplatU16(1)), Simd256ShiftRightU16(low, 8)), 8);
            high = Simd256ShiftRightU16(Simd256AddU16(Simd256AddU16(high, Simd256SplatU16(1)), Simd256ShiftRightU16(high, 8)), 8);
            return Simd256PackBytes(low, high);
        }

        static inline dsimd_byte32_t ReversePixels_Simd256(dsimd_byte32_t pixels)
        {
            static const uint8_t indices[32] = {
                3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12,
                3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12
            };
            return Simd256ShuffleBytes(pixels, Simd256LoadBytes(indices));
        }

        void PreMultiplyAlpha(uint8_t* data, uint32_t width, uint32_t height)
        {
            uint32_t remaining = width * height;
            while (remaining >= PIXELS_PER_VECTOR)
            {
                Simd256StoreBytes(data, PreMultiplyPixels_Simd256(Simd256LoadBytes(data)));
                data += sizeof(dsimd_byte32_t);
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
                    Simd256StoreBytes(destination, PreMultiplyPixels_Simd256(ReversePixels_Simd256(Simd256LoadBytes(source))));
                    source += sizeof(dsimd_byte32_t);
                    destination += sizeof(dsimd_byte32_t);
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
            dsimd_float8_t vector_scale = Simd256Splat(weight);
            dsimd_float4_t     scale = Simd128Splat(weight);
            const size_t lanes = sizeof(dsimd_float8_t) / sizeof(float);
            size_t i = 0;
            if (add)
            {
                for (; i + lanes <= count; i += lanes)
                {
                    Simd256Store(destination + i, Simd256Add(Simd256Mul(Simd256Load(source + i), vector_scale), Simd256Load(destination + i)));
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
                    Simd256Store(destination + i, Simd256Mul(Simd256Load(source + i), vector_scale));
                }

                for (; i < count; i += 4)
                {
                    Simd128Store(destination + i, Simd128Mul(Simd128Load(source + i), scale));
                }
            }
        }

    }
} // namespace dmTexc
