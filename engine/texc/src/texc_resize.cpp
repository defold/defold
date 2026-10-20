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

// Lanczos coefficients and normalization adapted from Basis Universal 2.50:
// encoder/basisu_resample_filters.cpp and encoder/basisu_resampler.cpp.
// Copyright (C) 2019-2024 Binomial LLC. All Rights Reserved.
// These adapted portions are licensed under the Apache License, Version 2.0.
// You may obtain a copy at http://www.apache.org/licenses/LICENSE-2.0.
// See licenses/NOTICE-basisuniversal for the upstream distribution notices.
// Unless required by applicable law or agreed to in writing, software distributed
// under that license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See that license for the
// specific language governing permissions and limitations under that license.

#include "texc_resize.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace dmTexc
{
    static double Sinc(double x)
    {
        x *= 3.14159265358979323846;
        if (x < 0.01f && x > -0.01f)
            return 1.0f + x * x * (-1.0f / 6.0f + x * x * 1.0f / 120.0f);
        return sin(x) / x;
    }

    static float Lanczos4(float x)
    {
        x = fabsf(x);
        if (x >= 4.0f)
            return 0.0f;
        double value = Sinc(x) * Sinc(x / 4.0f);
        return fabs(value) < 0.0000125f ? 0.0f : (float)value;
    }

    // Wide costs prevent the overflow that changes BasisU's order for large images.
    static bool ResizeVerticalFirst(uint32_t source_width, uint32_t source_height, uint32_t width, uint32_t height, uint32_t x_taps, uint32_t y_taps)
    {
        uint64_t xy_ops = (uint64_t)x_taps * source_height + ((uint64_t)4 * y_taps * width) / 3;
        uint64_t yx_ops = ((uint64_t)4 * y_taps * source_width) / 3 + (uint64_t)x_taps * height;
        return xy_ops > yx_ops || (xy_ops == yx_ops && source_width < width);
    }

    static void DestroyAxis(ResizeAxis* axis)
    {
        free(axis->m_Spans);
        free(axis->m_Taps);
    }

    static bool CreateAxis(uint32_t source_count, uint32_t destination_count, ResizeAxis* axis)
    {
        float scale = destination_count / (float)source_count;
        float filter_scale = scale < 1.0f ? scale : 1.0f;
        float half_width = 4.0f / filter_scale;
        axis->m_Spans = (ResizeSpan*)malloc(destination_count * sizeof(ResizeSpan));
        if (!axis->m_Spans)
            return false;
        uint32_t capacity = 0;
        for (uint32_t i = 0; i < destination_count; ++i)
        {
            float center = (i + 0.5f) / scale - 0.5f;
            int   left = (int)floorf(center - half_width);
            int   right = (int)ceilf(center + half_width);
            capacity += right - left + 1;
            uint32_t rows = (uint32_t)(right - left + 1);
            if (rows > axis->m_CacheRows)
                axis->m_CacheRows = rows;
        }
        if (axis->m_CacheRows > source_count)
            axis->m_CacheRows = source_count;
        axis->m_Taps = (ResizeTap*)malloc(capacity * sizeof(ResizeTap));
        if (!axis->m_Taps)
            return false;
        for (uint32_t i = 0; i < destination_count; ++i)
        {
            float center = (i + 0.5f) / scale - 0.5f;
            int   left = (int)floorf(center - half_width);
            int   right = (int)ceilf(center + half_width);
            float total = 0.0f;
            for (int j = left; j <= right; ++j)
                total += Lanczos4((center - (float)j) * filter_scale);
            float       normalization = 1.0f / total;
            ResizeSpan* span = &axis->m_Spans[i];
            span->m_Offset = axis->m_TapCount;
            span->m_Count = 0;
            total = 0.0f;
            float    maximum_weight = -1e20f;
            uint32_t maximum_index = 0;
            for (int j = left; j <= right; ++j)
            {
                float weight = Lanczos4((center - (float)j) * filter_scale) * normalization;
                if (weight == 0.0f)
                    continue;
                ResizeTap* tap = &axis->m_Taps[axis->m_TapCount++];
                tap->m_Pixel = j < 0 ? 0 : (j >= (int)source_count ? source_count - 1 : (uint32_t)j);
                tap->m_Weight = weight;
                if (weight > maximum_weight)
                {
                    maximum_weight = weight;
                    maximum_index = axis->m_TapCount - 1;
                }
                total += weight;
                ++span->m_Count;
            }
            // Match BasisU's correction and retain duplicate clamped edge taps.
            if (total != 1.0f)
                axis->m_Taps[maximum_index].m_Weight += 1.0f - total;
        }
        return true;
    }

    bool ResizeRGBA8WithKernels(const uint8_t* source, uint32_t source_width, uint32_t source_height, uint8_t* destination, uint32_t width, uint32_t height, const ResizeKernels* kernels)
    {
        // Match the dimension range accepted by the previous BasisU resampler.
        if (!source_width || !source_height || !width || !height ||
            source_width > 16384 || source_height > 16384 || width > 16384 || height > 16384)
            return false;
        if (source_width == width && source_height == height)
        {
            memcpy(destination, source, (size_t)width * height * 4);
            return true;
        }
        ResizeAxis axes[2];
        memset(axes, 0, sizeof(axes));
        if (!CreateAxis(source_width, width, &axes[0]) || !CreateAxis(source_height, height, &axes[1]))
        {
            DestroyAxis(&axes[0]);
            DestroyAxis(&axes[1]);
            return false;
        }
        bool     vertical_first = ResizeVerticalFirst(source_width, source_height, width, height, axes[0].m_TapCount, axes[1].m_TapCount);
        uint32_t intermediate_width = vertical_first ? source_width : width;
        size_t   row_elements = (size_t)intermediate_width * 4;
        size_t   row_bytes = row_elements * sizeof(float);
        // Scale the byte budget from 512 KiB at 4K to 2 MiB at 16K source width.
        // A fixed row cap repeatedly filters evicted rows during large reductions.
        size_t cache_budget_bytes = (size_t)source_width * 128;
        size_t budget_rows = cache_budget_bytes / row_bytes;
        // Retain the previous allowance for wide intermediate rows.
        if (budget_rows < 64)
            budget_rows = 64;
        uint32_t cache_rows = axes[1].m_CacheRows;
        if (cache_rows > budget_rows)
            cache_rows = (uint32_t)budget_rows;
        float*   cache = (float*)malloc(cache_rows * row_bytes);
        int32_t* tags = (int32_t*)malloc(cache_rows * sizeof(int32_t));
        float*   work = (float*)malloc(row_elements * sizeof(float));
        float*   extra = (float*)malloc((size_t)(vertical_first ? width : source_width) * 4 * sizeof(float));
        bool     success = cache && tags && work && extra;
        if (success)
        {
            for (uint32_t i = 0; i < cache_rows; ++i)
                tags[i] = -1;
            for (uint32_t y = 0; y < height; ++y)
            {
                const ResizeSpan* span = &axes[1].m_Spans[y];
                for (uint32_t i = 0; i < span->m_Count; ++i)
                {
                    const ResizeTap* tap = &axes[1].m_Taps[span->m_Offset + i];
                    uint32_t         slot = tap->m_Pixel % cache_rows;
                    float*           row = cache + slot * row_elements;
                    if (tags[slot] != (int32_t)tap->m_Pixel)
                    {
                        const uint8_t* input = source + (size_t)tap->m_Pixel * source_width * 4;
                        if (vertical_first)
                            kernels->m_ConvertRow(input, row, source_width);
                        else
                        {
                            kernels->m_ConvertRow(input, extra, source_width);
                            kernels->m_FilterHorizontal(extra, row, width, &axes[0]);
                        }
                        tags[slot] = (int32_t)tap->m_Pixel;
                    }
                    kernels->m_AccumulateRow(row, work, row_elements, tap->m_Weight, i != 0);
                }
                const float* output = work;
                if (vertical_first)
                {
                    kernels->m_FilterHorizontal(work, extra, width, &axes[0]);
                    output = extra;
                }
                kernels->m_StoreRow(output, destination + (size_t)y * width * 4, width);
            }
        }
        free(cache);
        free(tags);
        free(work);
        free(extra);
        DestroyAxis(&axes[0]);
        DestroyAxis(&axes[1]);
        return success;
    }
} // namespace dmTexc
