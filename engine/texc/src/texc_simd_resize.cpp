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

#include "texc_resize.h"
#include "texc_private.h"
#include <simd/simd.h>
#include <dlib/math.h>

namespace dmTexc
{
#if defined(DM_TEXC_TEST_WIDE)
    namespace Wide
#elif DM_SIMD_X86_ISA == 3
    namespace SSE3
#elif DM_SIMD_X86_ISA == 42
    namespace SSE42
#elif DM_SIMD_X86_ISA == 256
    namespace AVX2
#elif DM_SIMD_X86_ISA == 512
    namespace AVX512
#elif defined(DM_SIMD_NEON)
    namespace NEON
#else
#error SIMD resizing requires a compiled texc SIMD backend
#endif
    {
        static void ConvertRow(const uint8_t* source, float* destination, uint32_t width)
        {
            dsimd_float4_t scale = SimdSplat(1.0f / 255.0f);
            uint32_t       x = 0;
            for (; x + 4 <= width; x += 4)
            {
                dsimd_byte16_t  pixels = SimdLoadBytes(source + x * 4);
                dsimd_ushort8_t low = SimdWidenBytesLow(pixels);
                dsimd_ushort8_t high = SimdWidenBytesHigh(pixels);
                SimdStore(destination + x * 4, SimdMul(SimdConvertUintToFloat(SimdWidenU16Low(low)), scale));
                SimdStore(destination + x * 4 + 4, SimdMul(SimdConvertUintToFloat(SimdWidenU16High(low)), scale));
                SimdStore(destination + x * 4 + 8, SimdMul(SimdConvertUintToFloat(SimdWidenU16Low(high)), scale));
                SimdStore(destination + x * 4 + 12, SimdMul(SimdConvertUintToFloat(SimdWidenU16High(high)), scale));
            }
            for (; x < width; ++x)
            {
                const uint8_t* pixel = source + x * 4;
                SimdStore(destination + x * 4, SimdMul(SimdMake(pixel[0], pixel[1], pixel[2], pixel[3]), scale));
            }
        }

        static void FilterHorizontal(const float* source, float* destination, uint32_t width, const ResizeAxis* axis)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                const ResizeSpan* span = &axis->m_Spans[x];
                dsimd_float4_t    sum = SimdSplat(0.0f);
                for (uint32_t i = 0; i < span->m_Count; ++i)
                {
                    const ResizeTap* tap = &axis->m_Taps[span->m_Offset + i];
                    sum = SimdMulAdd(SimdLoad(source + tap->m_Pixel * 4), SimdSplat(tap->m_Weight), sum);
                }
                SimdStore(destination + x * 4, sum);
            }
        }

        static dsimd_uint4_t Quantize(dsimd_float4_t value)
        {
            value = SimdMin(SimdMax(value, SimdSplat(0.0f)), SimdSplat(1.0f));
            value = SimdMulAdd(value, SimdSplat(255.0f), SimdSplat(0.5f));
            // Integer sign bits are clear after clamping. The store/load is a bit cast
            // through the C API and optimizes to registers in the SIMD backends.
            uint32_t words[4];
            SimdStoreInt(words, SimdTruncateToInt(value));
            return SimdLoadUint(words);
        }

        static void StoreRow(const float* source, uint8_t* destination, uint32_t width)
        {
            uint32_t x = 0;
            for (; x + 4 <= width; x += 4)
            {
                dsimd_uint4_t a = Quantize(SimdLoad(source + x * 4));
                dsimd_uint4_t b = Quantize(SimdLoad(source + x * 4 + 4));
                dsimd_uint4_t c = Quantize(SimdLoad(source + x * 4 + 8));
                dsimd_uint4_t d = Quantize(SimdLoad(source + x * 4 + 12));
                SimdStoreBytes(destination + x * 4, SimdPackBytes(SimdPackU16(a, b), SimdPackU16(c, d)));
            }
            for (; x < width; ++x)
            {
                uint32_t words[4];
                SimdStoreUint(words, Quantize(SimdLoad(source + x * 4)));
                for (uint32_t c = 0; c < 4; ++c)
                    destination[x * 4 + c] = (uint8_t)words[c];
            }
        }

        // Implemented by the architecture-specific backend, using 128/256/512-bit rows.
        void                AccumulateResizeRow(const float*, float*, size_t, float, bool);

        const ResizeKernels g_ResizeKernels = { ConvertRow, FilterHorizontal, AccumulateResizeRow, StoreRow };

        void                ResizeRGBA32FPixels(const float* src, uint32_t source_width, uint32_t source_height, float* dst, uint32_t width, uint32_t height)
        {
            float scale_x = (float)source_width / (float)width;
            float scale_y = (float)source_height / (float)height;

            for (uint32_t y = 0; y < height; ++y)
            {
                float    src_y = dmMath::Max(0.0f, (y + 0.5f) * scale_y - 0.5f);
                uint32_t y0 = dmMath::Min((uint32_t)src_y, source_height - 1);
                uint32_t y1 = dmMath::Min(y0 + 1, source_height - 1);
                float    ty = src_y - y0;

                for (uint32_t x = 0; x < width; ++x)
                {
                    float          src_x = dmMath::Max(0.0f, (x + 0.5f) * scale_x - 0.5f);
                    uint32_t       x0 = dmMath::Min((uint32_t)src_x, source_width - 1);
                    uint32_t       x1 = dmMath::Min(x0 + 1, source_width - 1);
                    float          tx = src_x - x0;

                    dsimd_float4_t c00 = SimdLoad(src + ((size_t)y0 * source_width + x0) * 4);
                    dsimd_float4_t c10 = SimdLoad(src + ((size_t)y0 * source_width + x1) * 4);
                    dsimd_float4_t c01 = SimdLoad(src + ((size_t)y1 * source_width + x0) * 4);
                    dsimd_float4_t c11 = SimdLoad(src + ((size_t)y1 * source_width + x1) * 4);
                    dsimd_float4_t weight_x = SimdSplat(tx);
                    dsimd_float4_t cx0 = SimdMulAdd(SimdSub(c10, c00), weight_x, c00);
                    dsimd_float4_t cx1 = SimdMulAdd(SimdSub(c11, c01), weight_x, c01);
                    SimdStore(dst + ((size_t)y * width + x) * 4,
                              SimdMulAdd(SimdSub(cx1, cx0), SimdSplat(ty), cx0));
                }
            }
        }
    }
} // namespace dmTexc
