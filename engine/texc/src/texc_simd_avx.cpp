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
#if defined(DM_TEXC_TEST_WIDE)
    namespace Wide
#elif DM_SIMD_X86_ISA == 256
    namespace AVX2
#elif DM_SIMD_X86_ISA == 512
    namespace AVX512
#else
#error AVX backend requires AVX2 or AVX-512
#endif
    {
        void PreMultiplyAlpha(uint8_t* data, uint32_t width, uint32_t height)
        {
            uint32_t remaining = width * height;
            while (remaining >= DM_SIMD_WIDE_BYTES / 4)
            {
                SimdWideStoreBytes(data, PreMultiplyPixels(SimdWideLoadBytes(data)));
                data += DM_SIMD_WIDE_BYTES;
                remaining -= DM_SIMD_WIDE_BYTES / 4;
            }
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
                while (remaining >= DM_SIMD_WIDE_BYTES / 4)
                {
                    SimdWideStoreBytes(destination, PreMultiplyPixels(ReversePixels(SimdWideLoadBytes(source))));
                    source += DM_SIMD_WIDE_BYTES;
                    destination += DM_SIMD_WIDE_BYTES;
                    remaining -= DM_SIMD_WIDE_BYTES / 4;
                }
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
            dsimd_wide_float_t wide_scale = SimdWideSplat(weight);
            dsimd_float4_t     scale = SimdSplat(weight);
            size_t             i = 0;
            if (add)
            {
                for (; i + DM_SIMD_WIDE_BYTES / 4 <= count; i += DM_SIMD_WIDE_BYTES / 4)
                    SimdWideStore(destination + i, SimdWideAdd(SimdWideMul(SimdWideLoad(source + i), wide_scale), SimdWideLoad(destination + i)));
                for (; i < count; i += 4)
                    SimdStore(destination + i, SimdMulAdd(SimdLoad(source + i), scale, SimdLoad(destination + i)));
            }
            else
            {
                for (; i + DM_SIMD_WIDE_BYTES / 4 <= count; i += DM_SIMD_WIDE_BYTES / 4)
                    SimdWideStore(destination + i, SimdWideMul(SimdWideLoad(source + i), wide_scale));
                for (; i < count; i += 4)
                    SimdStore(destination + i, SimdMul(SimdLoad(source + i), scale));
            }
        }
    }
} // namespace dmTexc
