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
#if DM_SIMD_X86_ISA == 3
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
            dsimd_float4_t scale = Simd128Splat(1.0f / 255.0f);
            uint32_t       x = 0;
            for (; x + 4 <= width; x += 4)
            {
                dsimd_byte16_t  pixels = Simd128LoadBytes(source + x * 4);
                dsimd_ushort8_t low = Simd128WidenBytesLow(pixels);
                dsimd_ushort8_t high = Simd128WidenBytesHigh(pixels);
                Simd128Store(destination + x * 4, Simd128Mul(Simd128ConvertUintToFloat(Simd128WidenU16Low(low)), scale));
                Simd128Store(destination + x * 4 + 4, Simd128Mul(Simd128ConvertUintToFloat(Simd128WidenU16High(low)), scale));
                Simd128Store(destination + x * 4 + 8, Simd128Mul(Simd128ConvertUintToFloat(Simd128WidenU16Low(high)), scale));
                Simd128Store(destination + x * 4 + 12, Simd128Mul(Simd128ConvertUintToFloat(Simd128WidenU16High(high)), scale));
            }
            for (; x < width; ++x)
            {
                const uint8_t* pixel = source + x * 4;
                Simd128Store(destination + x * 4, Simd128Mul(Simd128Make(pixel[0], pixel[1], pixel[2], pixel[3]), scale));
            }
        }

        static void FilterHorizontal(const float* source, float* destination, uint32_t width, const ResizeAxis* axis)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                const ResizeSpan* span = &axis->m_Spans[x];
                dsimd_float4_t    sum = Simd128Splat(0.0f);
                for (uint32_t i = 0; i < span->m_Count; ++i)
                {
                    const ResizeTap* tap = &axis->m_Taps[span->m_Offset + i];
                    sum = Simd128MulAdd(Simd128Load(source + tap->m_Pixel * 4), Simd128Splat(tap->m_Weight), sum);
                }
                Simd128Store(destination + x * 4, sum);
            }
        }

        static dsimd_uint4_t Quantize(dsimd_float4_t value)
        {
            value = Simd128Min(Simd128Max(value, Simd128Splat(0.0f)), Simd128Splat(1.0f));
            value = Simd128MulAdd(value, Simd128Splat(255.0f), Simd128Splat(0.5f));
            // Integer sign bits are clear after clamping. The store/load is a bit cast
            // through the C API and optimizes to registers in the SIMD backends.
            uint32_t words[4];
            Simd128StoreInt(words, Simd128TruncateToInt(value));
            return Simd128LoadUint(words);
        }

        static void StoreRow(const float* source, uint8_t* destination, uint32_t width)
        {
            uint32_t x = 0;
            for (; x + 4 <= width; x += 4)
            {
                dsimd_uint4_t a = Quantize(Simd128Load(source + x * 4));
                dsimd_uint4_t b = Quantize(Simd128Load(source + x * 4 + 4));
                dsimd_uint4_t c = Quantize(Simd128Load(source + x * 4 + 8));
                dsimd_uint4_t d = Quantize(Simd128Load(source + x * 4 + 12));
                Simd128StoreBytes(destination + x * 4, Simd128PackBytes(Simd128PackU16(a, b), Simd128PackU16(c, d)));
            }
            for (; x < width; ++x)
            {
                uint32_t words[4];
                Simd128StoreUint(words, Quantize(Simd128Load(source + x * 4)));
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

                    dsimd_float4_t c00 = Simd128Load(src + ((size_t)y0 * source_width + x0) * 4);
                    dsimd_float4_t c10 = Simd128Load(src + ((size_t)y0 * source_width + x1) * 4);
                    dsimd_float4_t c01 = Simd128Load(src + ((size_t)y1 * source_width + x0) * 4);
                    dsimd_float4_t c11 = Simd128Load(src + ((size_t)y1 * source_width + x1) * 4);
                    dsimd_float4_t weight_x = Simd128Splat(tx);
                    dsimd_float4_t cx0 = Simd128MulAdd(Simd128Sub(c10, c00), weight_x, c00);
                    dsimd_float4_t cx1 = Simd128MulAdd(Simd128Sub(c11, c01), weight_x, c01);
                    Simd128Store(dst + ((size_t)y * width + x) * 4,
                              Simd128MulAdd(Simd128Sub(cx1, cx0), Simd128Splat(ty), cx0));
                }
            }
        }
    }
} // namespace dmTexc
