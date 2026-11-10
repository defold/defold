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

#include "texc_private.h"
#include "texc_resize.h"
#include <dlib/math.h>

namespace dmTexc
{
#if defined(DM_TEXC_TEST_REFERENCE)
    namespace Reference
#else
    namespace Scalar
#endif
    {
        void PreMultiplyAlpha(uint8_t* data, uint32_t width, uint32_t height)
        {
            for (uint32_t i = 0; i < width * height; ++i)
            {
                uint32_t a = data[3];
                data[0] = (uint8_t)((data[0] * a) / 255);
                data[1] = (uint8_t)((data[1] * a) / 255);
                data[2] = (uint8_t)((data[2] * a) / 255);
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
                uint32_t       flipped_y = height - y - 1;
                const uint8_t* src = input_data + y * row_bytes;
                uint8_t*       dst = output_data + flipped_y * row_bytes;

                for (uint32_t x = 0; x < width; ++x)
                {
                    uint8_t a = *(src++);
                    uint8_t b = *(src++);
                    uint8_t g = *(src++);
                    uint8_t r = *(src++);

                    *(dst++) = (uint8_t)((r * a) / 255);
                    *(dst++) = (uint8_t)((g * a) / 255);
                    *(dst++) = (uint8_t)((b * a) / 255);
                    *(dst++) = a;
                }
            }
        }

        static void ConvertRow(const uint8_t* source, float* destination, uint32_t width)
        {
            for (size_t i = 0; i < (size_t)width * 4; ++i)
                destination[i] = source[i] * (1.0f / 255.0f);
        }

        static void FilterHorizontal(const float* source, float* destination, uint32_t width, const ResizeAxis* axis)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                const ResizeSpan* span = &axis->m_Spans[x];
                float             sum[4] = { 0, 0, 0, 0 };
                for (uint32_t i = 0; i < span->m_Count; ++i)
                {
                    const ResizeTap* tap = &axis->m_Taps[span->m_Offset + i];
                    for (uint32_t c = 0; c < 4; ++c)
                        sum[c] += source[(size_t)tap->m_Pixel * 4 + c] * tap->m_Weight;
                }
                memcpy(destination + (size_t)x * 4, sum, sizeof(sum));
            }
        }

        static void AccumulateRow(const float* source, float* destination, size_t count, float weight, bool add)
        {
            if (add)
            {
                for (size_t i = 0; i < count; ++i)
                    destination[i] += source[i] * weight;
            }
            else
            {
                for (size_t i = 0; i < count; ++i)
                    destination[i] = source[i] * weight;
            }
        }

        static void StoreRow(const float* source, uint8_t* destination, uint32_t width)
        {
            for (size_t i = 0; i < (size_t)width * 4; ++i)
            {
                float value = source[i];
                value = value < 0.0f ? 0.0f : value;
                value = value > 1.0f ? 1.0f : value;
                destination[i] = (uint8_t)(value * 255.0f + 0.5f);
            }
        }

        bool ResizeRGBA8(const uint8_t* source, uint32_t source_width, uint32_t source_height, uint8_t* destination, uint32_t width, uint32_t height)
        {
            static const ResizeKernels kernels = { ConvertRow, FilterHorizontal, AccumulateRow, StoreRow };
            return ResizeRGBA8WithKernels(source, source_width, source_height, destination, width, height, &kernels);
        }

        void ResizeRGBA32FPixels(const float* src, uint32_t source_width, uint32_t source_height, float* dst, uint32_t width, uint32_t height)
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
                    float    src_x = dmMath::Max(0.0f, (x + 0.5f) * scale_x - 0.5f);
                    uint32_t x0 = dmMath::Min((uint32_t)src_x, source_width - 1);
                    uint32_t x1 = dmMath::Min(x0 + 1, source_width - 1);
                    float    tx = src_x - x0;

                    for (uint32_t c = 0; c < 4; ++c)
                    {
                        float c00 = src[(((size_t)y0 * source_width + x0) * 4) + c];
                        float c10 = src[(((size_t)y0 * source_width + x1) * 4) + c];
                        float c01 = src[(((size_t)y1 * source_width + x0) * 4) + c];
                        float c11 = src[(((size_t)y1 * source_width + x1) * 4) + c];
                        float cx0 = c00 + (c10 - c00) * tx;
                        float cx1 = c01 + (c11 - c01) * tx;
                        dst[(((size_t)y * width + x) * 4) + c] = cx0 + (cx1 - cx0) * ty;
                    }
                }
            }
        }
    } // namespace Scalar
} // namespace dmTexc
