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
#if DM_SIMD_X86_ISA == 3
    namespace SSE3
#elif DM_SIMD_X86_ISA == 42
    namespace SSE42
#else
#error SSE backend requires SSE3 or SSE4.2
#endif
    {
#if DM_SIMD_X86_ISA == 42
        static inline dsimd_byte16_t ConvertPreviewPixels(dsimd_byte16_t input)
        {
            static const uint8_t alpha_indices[16] = { 0, 128, 0, 128, 4, 128, 4, 128, 8, 128, 8, 128, 12, 128, 12, 128 };
            static const uint8_t color_indices[16] = { 3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12 };
            // x86 byte and word types share the same register representation.
            dsimd_ushort8_t words = SimdShuffleBytes(input, SimdLoadBytes(color_indices));
            dsimd_ushort8_t alphas = SimdShuffleBytes(input, SimdLoadBytes(alpha_indices));
            dsimd_ushort8_t alpha_factors = SimdOrUint(alphas, SimdSplatUint(0x00ff0000u));
            // Split RGBA into R/B and G/A words; multiplying alpha by 255 preserves it.
            dsimd_ushort8_t even = SimdMulU16(SimdAndU16(words, SimdSplatU16(255)), alphas);
            dsimd_ushort8_t odd = SimdMulU16(SimdShiftRightU16(words, 8), alpha_factors);
            // Exact floor(product / 255) for byte products, using multiply-high.
            even = SimdShiftRightU16(SimdMulHighU16(even, SimdSplatU16(0x8081)), 7);
            odd = SimdShiftRightU16(SimdMulHighU16(odd, SimdSplatU16(0x8081)), 7);
            // Each quotient fits in a byte. Merge without saturated packing.
            return SimdOrU16(even, SimdShiftLeftU16(odd, 8));
        }
#endif

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
#if DM_SIMD_X86_ISA == 42
                while (remaining >= 16)
                {
                    dsimd_byte16_t p0 = ConvertPreviewPixels(SimdLoadBytes(source));
                    dsimd_byte16_t p1 = ConvertPreviewPixels(SimdLoadBytes(source + 16));
                    dsimd_byte16_t p2 = ConvertPreviewPixels(SimdLoadBytes(source + 32));
                    dsimd_byte16_t p3 = ConvertPreviewPixels(SimdLoadBytes(source + 48));
                    SimdStoreBytes(destination, p0);
                    SimdStoreBytes(destination + 16, p1);
                    SimdStoreBytes(destination + 32, p2);
                    SimdStoreBytes(destination + 48, p3);
                    source += 64;
                    destination += 64;
                    remaining -= 16;
                }
#endif
                while (remaining >= 4)
                {
#if DM_SIMD_X86_ISA == 42
                    SimdStoreBytes(destination, ConvertPreviewPixels(SimdLoadBytes(source)));
#else
                    SimdStoreBytes(destination, PreMultiplyPixels(ReversePixels(SimdLoadBytes(source))));
#endif
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
    }
} // namespace dmTexc
